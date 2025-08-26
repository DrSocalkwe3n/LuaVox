#pragma once

#include "Common/Abstract.hpp"
#include <cassert>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <sqlite3.h>
#include <TOSLib.hpp>
#include <TOSAsync.hpp>
#include <filesystem>
#include <string_view>
#include <thread>


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
    // std::pair<std::string, size_t> getAllHash();

    /*
        Обновляет время использования кеша
    */
    void updateTimeFor(Hash_t hash);

    /*
        Добавляет запись
    */
    void insert(Hash_t hash, size_t size);

    /*
        Выдаёт хэши на удаление по размеру в сумме больше bytesToFree.
        Сначала удаляется старьё, потом по приоритету дата использования + размер
    */
    std::vector<Hash_t> findExcessHashes(size_t bytesToFree, int timeBefore);

    /*
        Удаление записи
    */
    void remove(Hash_t hash);

    static std::string hashToString(Hash_t hash);
    static int hexCharToInt(char c);
    static Hash_t stringToHash(const std::string_view view);
};

/*
    Менеджер предоставления ресурсов. Управляет ресурс паками 
    и хранением кешированных ресурсов с сервера.
    Интерфейс однопоточный.

    Обработка файлов в отдельном потоке.
*/
class AssetsManager : public IAsyncDestructible {
public:
    using Ptr = std::shared_ptr<AssetsManager>;

    struct ResourceKey {
        Hash_t Hash;
        EnumAssets Type;
        std::string Domain, Key;
    };

public:
    virtual ~AssetsManager();
    static std::shared_ptr<AssetsManager> Create(asio::io_context &ioc, const fs::path& cachePath,
            size_t maxCacheDirectorySize = 8*1024*1024*1024ULL, size_t maxLifeTime = 7*24*60*60) {
        return createShared(ioc, new AssetsManager(ioc, cachePath, maxCacheDirectorySize, maxLifeTime));
    }

    // Добавить новый полученный с сервера ресурс
    void pushResources(std::vector<Resource> resources) {
        WriteQueue.lock()->push_range(resources);
    }

    // Добавить задачи на чтение
    void pushReads(std::vector<ResourceKey> keys) {
        ReadQueue.lock()->push_range(keys);
    }

    // Получить считанные данные
    std::vector<std::pair<Hash_t, std::optional<Resource>>> pullReads() {
        return std::move(*ReadyQueue.lock());
    }

    // Размер всего хранимого кеша
    size_t getCacheSize() {
        return DatabaseSize;
    }

    // Обновить параметры хранилища кеша
    void updateParams(size_t maxLifeTime, size_t maxCacheDirectorySize) {
        auto lock = Changes.lock();
        lock->MaxLifeTime = maxLifeTime;
        lock->MaxCacheDatabaseSize = maxCacheDirectorySize;
        lock->MaxChange = true;
    }

    // Установка путей до папок assets
    void setResourcePacks(std::vector<fs::path> packsAssets) {
        auto lock = Changes.lock();
        lock->Assets = std::move(packsAssets);
        lock->AssetsChange = true;
    }

    // Запуск процедуры проверки хешей всего хранимого кеша
    void runFullDatabaseRecheck(std::move_only_function<void(std::string result)>&& func) {
        auto lock = Changes.lock();
        lock->OnRecheckEnd = std::move(func);
        lock->FullRecheck = true;
    }

    bool hasError() {
        return IssuedAnError;
    }

private:
    Logger LOG = "Client>ResourceHandler";
    const fs::path
        CachePath,
        PathDatabase = CachePath / "db.sqlite3",
        PathFiles = CachePath / "blobs";
    static constexpr size_t SMALL_RESOURCE = 1 << 21;

    sqlite3 *DB = nullptr;                  // База хранения кеша меньше 2мб и информации о кеше на диске
    sqlite3_stmt
        *STMT_DISK_INSERT = nullptr,        // Вставка записи о хеше
        *STMT_DISK_UPDATE_TIME = nullptr,   // Обновить дату последнего использования
        *STMT_DISK_REMOVE = nullptr,        // Удалить хеш
        *STMT_DISK_CONTAINS = nullptr,      // Проверка наличия хеша
        *STMT_DISK_SUM = nullptr,           // Вычисляет занятое место на диске
        *STMT_DISK_COUNT = nullptr,         // Возвращает количество записей
        
        *STMT_INLINE_INSERT = nullptr,      // Вставка ресурса
        *STMT_INLINE_GET = nullptr,         // Поиск ресурса по хешу
        *STMT_INLINE_UPDATE_TIME = nullptr, // Обновить дату последнего использования
        *STMT_INLINE_SUM = nullptr,         // Размер внутреннего хранилища
        *STMT_INLINE_COUNT = nullptr;       // Возвращает количество записей

    // Полный размер данных на диске (насколько известно)
    volatile size_t DatabaseSize = 0;

    // Очередь задач на чтение
    TOS::SpinlockObject<std::queue<ResourceKey>> ReadQueue;
    // Очередь на запись ресурсов
    TOS::SpinlockObject<std::queue<Resource>> WriteQueue;
    // Очередь на выдачу результатов чтения
    TOS::SpinlockObject<std::vector<std::pair<Hash_t, std::optional<Resource>>>> ReadyQueue;

    struct Changes_t {
        std::vector<fs::path> Assets;
        volatile bool AssetsChange = false;
        size_t MaxCacheDatabaseSize, MaxLifeTime;
        volatile bool MaxChange = false;
        std::optional<std::move_only_function<void(std::string)>> OnRecheckEnd;
        volatile bool FullRecheck = false;
    };

    TOS::SpinlockObject<Changes_t> Changes;

    bool NeedShutdown = false, IssuedAnError = false;
    std::thread OffThread;


    virtual coro<> asyncDestructor();
    AssetsManager(boost::asio::io_context &ioc, const fs::path &cachePath,
        size_t maxCacheDatabaseSize, size_t maxLifeTime);

    void readWriteThread(AsyncUseControl::Lock lock);
};

}