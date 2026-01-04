#pragma once

#include "Common/AssetsPreloader.hpp"
#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1

#include <Common/Net.hpp>
#include <Common/Lockable.hpp>
#include <atomic>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <condition_variable>
#include <filesystem>
#include "Common/Abstract.hpp"
#include "RemoteClient.hpp"
#include "Server/Abstract.hpp"
#include <TOSLib.hpp>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <sol/forward.hpp>
#include <sol/state.hpp>
#include <thread>
#include <unordered_map>

#include "WorldDefManager.hpp"
#include "ContentManager.hpp"
#include "World.hpp"

#include "SaveBackend.hpp"


namespace LV::Server {

struct ModRequest {
    std::string Id;
    std::array<uint32_t, 4> MinVersion, MaxVersion;
};

struct ModInfo {
    std::string Id, Name, Description, Author;
    std::vector<std::string> AlternativeIds;
    std::array<uint32_t, 4> Version;
    std::vector<ModRequest> Dependencies, Optional;
    float LoadPriority;
    fs::path Path;
    bool HasLiveReload;

    std::string dump() const;
};

namespace fs = std::filesystem;

class GameServer : public AsyncObject {
    TOS::Logger LOG = "GameServer";
    DestroyLock UseLock;
    std::thread RunThread;

    bool IsAlive = true, IsGoingShutdown = false;
    std::string ShutdownReason;
    std::atomic<bool> ModsReloadRequested = false;
    static constexpr float
        PerTickDuration = 1/30.f,   // Минимальная и стартовая длина такта
        PerTickAdjustment = 1/60.f; // Подгонка длительности такта в случае провисаний
    float 
        CurrentTickDuration = PerTickDuration, // Текущая длительность такта
        GlobalTickLagTime = 0;                 // На сколько глобально запаздываем по симуляции

    struct {
        Lockable<std::set<std::string>> ConnectedPlayersSet;
        Lockable<std::list<std::shared_ptr<RemoteClient>>> NewConnectedPlayers;
    } External;

    struct ContentObj {
    public:
        AssetsPreloader AM;
        ContentManager CM;

        // Если контент был перерегистрирован (исключая двоичные ресурсы), то профили будут повторно разосланы
        ResourceRequest OnContentChanges;


        ContentObj(asio::io_context&)
            : AM(), CM(AM)
        {}
    } Content;

    struct {
        std::vector<std::shared_ptr<RemoteClient>> RemoteClients;
        ServerTime AfterStartTime = {0, 0};

    } Game;

    struct Expanse_t {
        std::unordered_map<ContentBridgeId_t, ContentBridge> ContentBridges;

        // Вычисляет окружности обозримой области
        // depth ограничивает глубину входа в ContentBridges
        std::vector<ContentViewCircle> accumulateContentViewCircles(ContentViewCircle circle, int depth = 2);
        // Вынести в отдельный поток
        static ContentViewInfo makeContentViewInfo(const std::vector<ContentViewCircle> &views);
        ContentViewInfo makeContentViewInfo(ContentViewCircle circle, int depth = 2) {
            return makeContentViewInfo(accumulateContentViewCircles(circle, depth));
        }

        // std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> remapCVCsByWorld(const std::vector<ContentViewCircle> &list);
        // std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> calcAndRemapCVC(ContentViewCircle circle, int depth = 2) {
        //     return remapCVCsByWorld(calcCVCs(circle, depth));
        // }

        std::unordered_map<WorldId_t, std::unique_ptr<World>> Worlds;

        /*
            Регистрация миров по строке
        */


        /*
            
        */


        private:
            void _accumulateContentViewCircles(ContentViewCircle circle, int depth);
    } Expanse;

    struct {
        std::unique_ptr<IWorldSaveBackend> World;
        std::unique_ptr<IPlayerSaveBackend> Player;
        std::unique_ptr<IAuthSaveBackend> Auth;
        std::unique_ptr<IModStorageSaveBackend> ModStorage;
    } SaveBackend;

    /*
        Обязательно между тактами

        Конвертация ресурсов игры, их хранение в кеше и загрузка в память для отправки клиентам
            io_uring или последовательное чтение
        
        Исполнение асинхронного луа
            Пул для постоянной работы и синхронизации времени с главным потоком

        Сжатие/расжатие регионов в базе
            Локальный поток должен собирать ключи профилей для базы
            Остальное внутри базы
    */

    /*
        Отправка изменений чанков клиентам

            После окончания такта пул копирует изменённые чанки 
            - синхронизация сбора в stepDatabaseSync - 
            сжимает их и отправляет клиентам
            - синхронизация в начале stepPlayerProceed -
            ^ к этому моменту все данные должны быть отправлены в RemoteClient
    */
    struct BackingChunkPressure_t {
        TOS::Logger LOG = "BackingChunkPressure";
        volatile bool NeedShutdown = false;
        std::vector<std::thread> Threads;
        std::mutex Mutex;
        volatile int RunCollect = 0, RunCompress = 0, Iteration = 0;
        std::condition_variable Symaphore;
        std::unordered_map<WorldId_t, std::unique_ptr<World>> *Worlds;

        void startCollectChanges() {
            std::lock_guard<std::mutex> lock(Mutex);
            RunCollect = Threads.size();
            RunCompress = Threads.size();
            Iteration += 1;
            assert(RunCollect != 0);
            Symaphore.notify_all();
        }

        void endCollectChanges() {
            std::unique_lock<std::mutex> lock(Mutex);
            Symaphore.wait(lock, [&](){ return RunCollect == 0 || NeedShutdown; });
        }

        void endWithResults() {
            std::unique_lock<std::mutex> lock(Mutex);
            Symaphore.wait(lock, [&](){ return RunCompress == 0 || NeedShutdown; });
        }

        void stop() {
            {
                std::unique_lock<std::mutex> lock(Mutex);
                NeedShutdown = true;
                Symaphore.notify_all();
            }

            for(std::thread& thread : Threads)
                thread.join();
        }

        /* __attribute__((optimize("O3"))) */ void run(int id);
    } BackingChunkPressure;

    /*
        Генератор шума
    */
    struct BackingNoiseGenerator_t {
        struct NoiseKey {
            WorldId_t WId;
            Pos::GlobalRegion RegionPos;
        };

        TOS::Logger LOG = "BackingNoiseGenerator";
        bool NeedShutdown = false;
        std::vector<std::thread> Threads;
        TOS::SpinlockObject<std::queue<NoiseKey>> Input;
        TOS::SpinlockObject<std::vector<std::pair<NoiseKey, std::array<float, 64*64*64>>>> Output;

        void stop() {
            NeedShutdown = true;

            for(std::thread& thread : Threads)
                thread.join();
        }

        void run(int id);

        std::vector<std::pair<NoiseKey, std::array<float, 64*64*64>>>
        tickSync(std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> &&input) {
            {
                auto lock = Input.lock();
                
                for(auto& [worldId, region] : input) {
                    for(auto& regionPos : region) {
                        lock->push({worldId, regionPos});
                    }
                }
            }

            auto lock = Output.lock();
            std::vector<std::pair<NoiseKey, std::array<float, 64*64*64>>> out = std::move(*lock);
            lock->reserve(8000);

            return std::move(out);
        }
    } BackingNoiseGenerator;

    /*
        Обработчик асинронного луа
    */
    struct BackingAsyncLua_t {
        TOS::Logger LOG = "BackingAsyncLua";
        bool NeedShutdown = false;
        std::vector<std::thread> Threads;
        TOS::SpinlockObject<std::queue<std::pair<BackingNoiseGenerator_t::NoiseKey, std::array<float, 64*64*64>>>> NoiseIn;
        TOS::SpinlockObject<std::vector<std::pair<BackingNoiseGenerator_t::NoiseKey, World::RegionIn>>> RegionOut;

        void stop() {
            NeedShutdown = true;

            for(std::thread& thread : Threads)
                thread.join();
        }

        void run(int id);
    } BackingAsyncLua;

    sol::state LuaMainState;
    std::vector<ModInfo> LoadedMods;
    std::vector<std::pair<std::string, sol::table>> ModInstances;
    // Идентификатор текущегго мода, находящевося в обработке
    std::string CurrentModId;
    AssetsPreloader::AssetsRegister AssetsInit;
    DefEntityId PlayerEntityDefId = 0;

public:
    GameServer(asio::io_context &ioc, fs::path worldPath);
    virtual ~GameServer();

    void shutdown(const std::string reason) {
        if(ShutdownReason.empty())
            ShutdownReason = reason;
        IsGoingShutdown = true;
    }

    bool isAlive() {
        return IsAlive;
    }

    void waitShutdown() {
        UseLock.wait_no_use();
    }
    void requestModsReload();

    // Подключение tcp сокета
    coro<> pushSocketConnect(tcp::socket socket);
    // Сокет, прошедший авторизацию (onSocketConnect() передаёт его в onSocketAuthorized())
    coro<> pushSocketAuthorized(tcp::socket socket, const std::string username);
    // Инициализация игрового протокола для сокета (onSocketAuthorized() может передать сокет в onSocketGame())
    coro<> pushSocketGameProtocol(tcp::socket socket, const std::string username);

private:
    void init(fs::path worldPath);
    void prerun();
    void run();

    void initLuaAssets();
    void initLuaPre();
    void initLua();
    void initLuaPost();

    /*
        Подключение/отключение игроков
    */

    void stepConnections();

    /*
        Переинициализация модов, если требуется
    */

    void stepModInitializations();
    void reloadMods();

    /*
        Пересчёт зон видимости игроков, если необходимо
        Выгрузить более не используемые регионы
        Сохранение регионов
        Создание списка регионов необходимых для загрузки (бд автоматически будет предзагружать)
        <Синхронизация с модулем сохранений>
        Очередь загрузки, выгрузка регионов и получение загруженных из бд регионов
        Получить список регионов отсутствующих в сохранении и требующих генерации
        Подпись на загруженные регионы (отправить полностью на клиент)
    */

    IWorldSaveBackend::TickSyncInfo_Out stepDatabaseSync();

    /*
        Синхронизация с генератором карт (отправка запросов на генерацию и получение шума для обработки модами)
        Обработка модами сырых регионов полученных с бд
        Синхронизация с потоками модов
    */

    void stepGeneratorAndLuaAsync(IWorldSaveBackend::TickSyncInfo_Out db);

    /*
        Пакеты игроков получает асинхронный поток в RemoteClient
        Остаётся только обработать распаршенные пакеты 
    */

    void stepPlayerProceed();

    /*
        Физика
    */

    void stepWorldPhysic();

    /*
        Глобальный такт
    */

    void stepGlobalStep();

    /*
        Обработка запросов двоичных ресурсов и определений
        Отправка пакетов игрокам
        Запуск задачи ChunksChanges
    */
    void stepSyncContent();
};

}
