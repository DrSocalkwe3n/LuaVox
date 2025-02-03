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


namespace AL::Server {

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

    struct WorldObj {
        std::unordered_map<ContentBridgeId_t, ContentBridge> ContentBridges;

        std::vector<ContentViewCircle> calcCVCs(ContentViewCircle circle, int depth = 2);
        std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> remapCVCsByWorld(const std::vector<ContentViewCircle> &list);
        std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> calcAndRemapCVC(ContentViewCircle circle, int depth = 2) {
            return remapCVCsByWorld(calcCVCs(circle, depth));
        }

        std::unordered_map<WorldId_t, std::unique_ptr<World>> Worlds;

        /*
            Регистрация миров по строке

        */

        private:
            void _calcContentViewCircles(ContentViewCircle circle, int depth);
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

private:
    void init(fs::path worldPath);
    void prerun();
    void run();

    void stepContent();
    void stepPlayers();
    void stepWorlds();
    void stepViewContent();
    void stepSendPlayersPackets();
    void stepLoadRegions();
    void stepGlobal();
    void stepSave();
    void save();
};

}