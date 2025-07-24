#pragma once

#include <Common/Net.hpp>
#include <Common/Lockable.hpp>
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
#include <thread>
#include <unordered_map>
#include "ContentEventController.hpp"

#include "WorldDefManager.hpp"
#include "BinaryResourceManager.hpp"
#include "World.hpp"

#include "SaveBackend.hpp"


namespace LV::Server {

namespace fs = std::filesystem;

class GameServer : public AsyncObject {
    TOS::Logger LOG = "GameServer";
    DestroyLock UseLock;
    std::thread RunThread;

    bool IsAlive = true, IsGoingShutdown = false;
    std::string ShutdownReason;
    static constexpr float
        PerTickDuration = 1/30.f,   // Минимальная и стартовая длина такта
        PerTickAdjustment = 1/60.f; // Подгонка длительности такта в случае провисаний
    float 
        CurrentTickDuration = PerTickDuration, // Текущая длительность такта
        GlobalTickLagTime = 0;                 // На сколько глобально запаздываем по симуляции

    struct {
        Lockable<std::set<std::string>> ConnectedPlayersSet;
        Lockable<std::list<std::unique_ptr<RemoteClient>>> NewConnectedPlayers;
    } External;

    struct ContentObj {
    public:
        BinaryResourceManager BRM;

        ResourceId_t NextId[(int) EnumDefContent::MAX_ENUM] = {0};
        std::unordered_map<std::string, ResourceId_t> ContentKeyToId[(int) EnumDefContent::MAX_ENUM]; // EnumDefContent

        std::unordered_map<DefVoxelId_t, DefVoxel_t>    ContentIdToDef_Voxel;
        std::unordered_map<DefNodeId_t, DefNode_t>      ContentIdToDef_Node;
        std::unordered_map<DefWorldId_t, DefWorld_t>    ContentIdToDef_World;
        std::unordered_map<DefPortalId_t, DefPortal_t>  ContentIdToDef_Portal;
        std::unordered_map<DefEntityId_t, DefEntity_t>  ContentIdToDef_Entity;
        std::unordered_map<DefItemId_t, DefItem_t>      ContentIdToDef_Item;


        ResourceId_t getContentDefId(const std::string& key, EnumDefContent def) {
            int index = int(def);
            assert(index < (int) EnumDefContent::MAX_ENUM);

            auto &container = ContentKeyToId[index];
            auto iter = container.find(key);
            if(iter == container.end()) {
                assert(NextId[index] != ResourceId_t(-1));
                ResourceId_t nextId = NextId[index]++;
                container.insert(iter, {key, nextId});
                return nextId;
            }

            return iter->second;
        }

        // Если контент был перерегистрирован (исключая двоичные ресурсы), то профили будут повторно разосланы
        ResourceRequest OnContentChanges;


        ContentObj(asio::io_context& ioc)
            : BRM(ioc)
        {}
    } Content;

    struct {
        std::vector<std::shared_ptr<ContentEventController>> CECs;
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

        __attribute__((optimize("O3"))) void run(int id);
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

    // Подключение tcp сокета
    coro<> pushSocketConnect(tcp::socket socket);
    // Сокет, прошедший авторизацию (onSocketConnect() передаёт его в onSocketAuthorized())
    coro<> pushSocketAuthorized(tcp::socket socket, const std::string username);
    // Инициализация игрового протокола для сокета (onSocketAuthorized() может передать сокет в onSocketGame())
    coro<> pushSocketGameProtocol(tcp::socket socket, const std::string username);

    TexturePipeline buildTexturePipeline(const std::string& pipeline);
    std::string deBuildTexturePipeline(const TexturePipeline& pipeline);

private:
    void init(fs::path worldPath);
    void prerun();
    void run();

    struct ModInfo {
        std::string Id, Title, Description;
        fs::path Path;

        std::vector<std::string> Dependencies, OptionalDependencies;
    };

    std::vector<ModInfo> readModDataPath(const fs::path& modsDir);

    /*
        Подключение/отключение игроков
    */

    void stepConnections();

    /*
        Переинициализация модов, если требуется
    */

    void stepModInitializations();

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