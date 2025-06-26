#include <array>
#include <cassert>
#include <memory>
#include <queue>
#include <string>
#include <sqlite3.h>
#include <TOSLib.hpp>
#include <TOSAsync.hpp>
#include <filesystem>
#include <string_view>


namespace LV::Client {

using namespace TOS;
namespace fs = std::filesystem;

// NOT ThreadSafe
class CacheDatabase {
    const fs::path Path;

    sqlite3 *DB = nullptr;
    sqlite3_stmt *STMT_INSERT = nullptr,
        *STMT_UPDATE_TIME = nullptr,
        *STMT_REMOVE = nullptr,
        *STMT_ALL_HASH = nullptr,
        *STMT_SUM = nullptr,
        *STMT_OLD = nullptr,
        *STMT_TO_FREE = nullptr,
        *STMT_COUNT = nullptr;

public:
    CacheDatabase(const fs::path &cachePath);
    ~CacheDatabase();

    CacheDatabase(const CacheDatabase&) = delete;
    CacheDatabase(CacheDatabase&&) = delete;
    CacheDatabase& operator=(const CacheDatabase&) = delete;
    CacheDatabase& operator=(CacheDatabase&&) = delete; 

    /*
        Выдаёт размер занимаемый всем хранимым кешем
    */
    size_t getCacheSize();

    // TODO: добавить ограничения на количество файлов

    /*
        Создаёт линейный массив в котором подряд указаны все хэш суммы в бинарном виде и возвращает их количество
    */
    std::pair<std::string, size_t> getAllHash();

    using HASH = std::array<uint8_t, 32>;

    /*
        Обновляет время использования кеша
    */
    void updateTimeFor(HASH hash);

    /*
        Добавляет запись
    */
    void insert(HASH hash, size_t size);

    /*
        Выдаёт хэши на удаление по размеру в сумме больше bytesToFree.
        Сначала удаляется старьё, потом по приоритету дата использования + размер
    */
    std::vector<HASH> findExcessHashes(size_t bytesToFree, int timeBefore);

    /*
        Удаление записи
    */
    void remove(HASH hash);

    static std::string hashToString(HASH hash);
    static int hexCharToInt(char c);
    static HASH stringToHash(const std::string_view view);
};

/*
    Читает и пишет ресурсы на диск
    В приоритете чтение

    Кодировки только на стороне сервера, на клиенте уже готовые данные

    NOT ThreadSafe
*/
class CacheHandler : public IAsyncDestructible {
protected:
    const fs::path Path;
    CacheDatabase DB;

protected:
    CacheHandler(boost::asio::io_context &ioc, const fs::path &cachePath);

public:
    virtual ~CacheHandler();

    // Добавить задачу на запись
    virtual void pushWrite(std::string &&data, CacheDatabase::HASH hash) = 0;

    // Добавить задачу на чтение
    virtual void pushRead(CacheDatabase::HASH hash) = 0;

    // Получить считанные данные
    virtual std::vector<std::pair<CacheDatabase::HASH, std::string>> pullReads() = 0;

    // Получить список доступных ресурсов
    std::pair<std::string, size_t> getAll();
};

class CacheHandlerBasic : public CacheHandler {
    Logger LOG = "CacheHandlerBasic";

    std::thread ReadThread, ReadWriteThread;

    struct DataTask {
        CacheDatabase::HASH Hash;
        std::shared_ptr<std::string> Data;
    };

    // Очередь задач на чтение
    SpinlockObject<std::queue<CacheDatabase::HASH>> ReadQueue;
    // Кэш данных, которые ещё не записались
    SpinlockObject<std::vector<std::pair<CacheDatabase::HASH, std::shared_ptr<std::string>>>> WriteCache;
    // Очередь записи данных на диск
    SpinlockObject<std::queue<DataTask>> WriteQueue;
    // Список полностью считанных файлов
    SpinlockObject<std::vector<DataTask>> ReadedQueue;
    bool NeedShutdown = false;
    size_t MaxCacheDirectorySize = 8*1024*1024*1024ULL;
    size_t MaxLifeTime = 7*24*60*60;

public:
    using Ptr = std::shared_ptr<CacheHandlerBasic>;

private:
    virtual coro<> asyncDestructor() override;

    void readThread(AsyncUseControl::Lock lock);

    void readWriteThread(AsyncUseControl::Lock lock);

protected:
    CacheHandlerBasic(boost::asio::io_context &ioc, const fs::path& cachePath);

public:
    virtual ~CacheHandlerBasic();

    static std::shared_ptr<CacheHandlerBasic> Create(asio::io_context &ioc, const fs::path& cachePath) {
        return createShared(ioc, new CacheHandlerBasic(ioc, cachePath));
    }

    virtual void pushWrite(std::string &&data, CacheDatabase::HASH hash) override;
    virtual void pushRead(CacheDatabase::HASH hash) override;
    virtual std::vector<std::pair<CacheDatabase::HASH, std::string>> pullReads() override;
};

#ifdef LUAVOX_HAVE_LIBURING

class CacheHandlerUring : public CacheHandler {

};

#endif

}