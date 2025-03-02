#pragma once

#include <Common/Net.hpp>
#include <Common/Lockable.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <filesystem>
#include "RemoteClient.hpp"
#include "Server/Abstract.hpp"
#include <TOSLib.hpp>
#include <memory>
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
        // WorldDefManager WorldDM;
        // VoxelDefManager VoxelDM;
        // NodeDefManager NodeDM;
        BinaryResourceManager TextureM;
        BinaryResourceManager ModelM;
        BinaryResourceManager SoundM;

        ContentObj(asio::io_context &ioc,
                std::shared_ptr<ResourceFile> zeroTexture,
                std::shared_ptr<ResourceFile> zeroModel,
                std::shared_ptr<ResourceFile> zeroSound)
            : TextureM(ioc, zeroTexture),
              ModelM(ioc, zeroModel),
              SoundM(ioc, zeroSound)
        {

        }

    } Content;

    struct {
        std::vector<std::unique_ptr<ContentEventController>> CECs;
        // Индекс игрока, у которого в следующем такте будет пересмотрен ContentEventController->ContentViewCircles
        uint16_t CEC_NextRebuildViewCircles = 0, CEC_NextCheckRegions = 0;
        ServerTime AfterStartTime = {0, 0};

    } Game;

    struct Expanse_t {
        std::unordered_map<ContentBridgeId_t, ContentBridge> ContentBridges;

        // Вычисляет окружности обозримой области
        // depth ограничивает глубину входа в ContentBridges
        std::vector<ContentViewCircle> accumulateContentViewCircles(ContentViewCircle circle, int depth = 2);
        // Вынести в отдельный поток
        static ContentViewGlobal makeContentViewGlobal(const std::vector<ContentViewCircle> &views);
        ContentViewGlobal makeContentViewGlobal(ContentViewCircle circle, int depth = 2) {
            return makeContentViewGlobal(accumulateContentViewCircles(circle, depth));
        }

        // std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> remapCVCsByWorld(const std::vector<ContentViewCircle> &list);
        // std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> calcAndRemapCVC(ContentViewCircle circle, int depth = 2) {
        //     return remapCVCsByWorld(calcCVCs(circle, depth));
        // }

        std::unordered_map<WorldId_t, std::unique_ptr<World>> Worlds;

        /*
            Регистрация миров по строке

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

public:
    GameServer(asio::io_context &ioc, fs::path worldPath)
        : AsyncObject(ioc),
          Content(ioc, nullptr, nullptr, nullptr)
    {
        init(worldPath);
    }

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

    /* Загрузит, сгенерирует или просто выдаст регион из мира, который должен существовать */
    Region* forceGetRegion(WorldId_t worldId, Pos::GlobalRegion pos);

private:
    void init(fs::path worldPath);
    void prerun();
    void run();

    void stepContent();
    /*
        Дождаться и получить необходимые данные с бд или диска
        Получить несрочные данные
    */
    void stepSyncWithAsync();
    void stepPlayers();
    void stepWorlds();
    /*
        Пересмотр наблюдаемых зон (чанки, регионы, миры)
        Добавить требуемые регионы в список на предзагрузку с приоритетом
        TODO: нужен механизм асинхронной загрузки регионов с бд

        В начале следующего такта обязательное дожидание прогрузки активной зоны
        и 
        оповещение миров об изменениях в наблюдаемых регионах 
    */
    void stepViewContent();
    void stepSendPlayersPackets();
    void stepLoadRegions();
    void stepGlobal();
    void stepSave();
    void save();
};

}