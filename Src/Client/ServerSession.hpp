#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "Common/Async.hpp"
#include "Common/Lockable.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include "TOSAsync.hpp"
#include <TOSLib.hpp>
#include <boost/asio/io_context.hpp>
#include <filesystem>
#include <memory>
#include <boost/lockfree/spsc_queue.hpp>
#include <Client/AssetsManager.hpp>
#include <queue>
#include <unordered_map>


namespace LV::Client {

class ServerSession : public IAsyncDestructible, public IServerSession, public ISurfaceEventListener {
public:
    using Ptr = std::shared_ptr<ServerSession>;

public:
    static Ptr Create(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket> &&socket) {
        return createShared(ioc, new ServerSession(ioc, std::move(socket)));
    }

    virtual ~ServerSession();

    // Авторизоваться или (зарегистрироваться и авторизоваться) или зарегистрироваться
    static coro<> asyncAuthorizeWithServer(tcp::socket &socket, const std::string username, const std::string token, int a_ar_r, std::function<void(const std::string&)> onProgress = nullptr);
    // Начать игровой протокол в авторизированном сокете
    static coro<std::unique_ptr<Net::AsyncSocket>> asyncInitGameProtocol(asio::io_context &ioc, tcp::socket &&socket, std::function<void(const std::string&)> onProgress = nullptr);

    void shutdown(EnumDisconnect type);
    void requestModsReload();

    bool isConnected() {
        return Socket->isAlive() && IsConnected; 
    }

    uint64_t getVisibleCompressedChunksBytes() const {
        return VisibleChunkCompressedBytes;
    }

    // ISurfaceEventListener
    
    virtual void onResize(uint32_t width, uint32_t height) override;
    virtual void onChangeFocusState(bool isFocused) override;
    virtual void onCursorPosChange(int32_t width, int32_t height) override;
    virtual void onCursorMove(float xMove, float yMove) override;

    virtual void onCursorBtn(EnumCursorBtn btn, bool state) override;
    virtual void onKeyboardBtn(int btn, int state) override;
    virtual void onJoystick() override;

    // IServerSession

    virtual void update(GlobalTime gTime, float dTime) override;
    void setRenderSession(IRenderSession* session);

private:
    TOS::Logger LOG = "ServerSession";

    std::unique_ptr<Net::AsyncSocket> Socket;
    IRenderSession *RS = nullptr;

    // Обработчик кеша ресурсов сервера
    AssetsManager AM;

    static constexpr uint64_t TIME_BEFORE_UNLOAD_RESOURCE = 180;
    struct {
        // Существующие привязки ресурсов
        // std::unordered_set<ResourceId> ExistBinds[(int) EnumAssets::MAX_ENUM];
        // Недавно использованные ресурсы, пока хранятся здесь в течении TIME_BEFORE_UNLOAD_RESOURCE секунд
        // std::unordered_map<std::string, std::pair<AssetEntry, uint64_t>> NotInUse[(int) EnumAssets::MAX_ENUM];
    } MyAssets;

    struct AssetLoadingEntry {
        EnumAssets Type;
        ResourceId Id;
        std::string Domain;
        std::string Key;
    };

    struct AssetLoading {
        std::u8string Data;
        size_t Offset = 0;
    };

    struct AssetBindEntry {
        EnumAssets Type;
        ResourceId Id;
        std::string Domain, Key;
        Hash_t Hash;
        std::vector<uint8_t> Header;
    };

    struct UpdateAssetsBindsDK {
        std::vector<std::string> Domains;
        std::array<
            std::vector<std::vector<std::string>>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > Keys;
    };

    struct UpdateAssetsBindsHH {
        std::array<
            std::vector<std::tuple<ResourceId, ResourceFile::Hash_t, ResourceHeader>>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        > HashAndHeaders;
    };

    struct TickData {
        // Полученные изменения привязок Domain+Key
        std::vector<UpdateAssetsBindsDK> BindsDK;
        // Полученные изменения привязок Hash+Header
        std::vector<UpdateAssetsBindsHH> BindsHH;
        // Полученные с сервера ресурсы
        std::vector<std::tuple<ResourceFile::Hash_t, std::u8string>> ReceivedAssets;

        std::vector<std::pair<DefVoxelId, void*>> Profile_Voxel_AddOrChange;
        std::vector<DefVoxelId> Profile_Voxel_Lost;
        std::vector<std::pair<DefNodeId, DefNode_t>> Profile_Node_AddOrChange;
        std::vector<DefNodeId> Profile_Node_Lost;
        std::vector<std::pair<DefWorldId, void*>> Profile_World_AddOrChange;
        std::vector<DefWorldId> Profile_World_Lost;
        std::vector<std::pair<DefPortalId, void*>> Profile_Portal_AddOrChange;
        std::vector<DefPortalId> Profile_Portal_Lost;
        std::vector<std::pair<DefEntityId, DefEntityInfo>> Profile_Entity_AddOrChange;
        std::vector<DefEntityId> Profile_Entity_Lost;
        std::vector<std::pair<DefItemId, void*>> Profile_Item_AddOrChange;
        std::vector<DefItemId> Profile_Item_Lost;

        std::vector<std::pair<WorldId_t, void*>> Worlds_AddOrChange;
        std::vector<WorldId_t> Worlds_Lost;

        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::u8string>> Chunks_AddOrChange_Voxel;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::u8string>> Chunks_AddOrChange_Node;
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Regions_Lost;
        std::vector<std::pair<EntityId_t, EntityInfo>> Entity_AddOrChange;
        std::vector<EntityId_t> Entity_Lost;
    };

    struct ChunkCompressedSize {
        uint32_t Voxels = 0;
        uint32_t Nodes = 0;
    };

    struct {
    // Сюда обращается ветка, обрабатывающая сокет; run()
        // Получение ресурсов с сервера
        std::unordered_map<Hash_t, AssetLoading> AssetsLoading;
        // Накопление данных за такт сервера
        TickData ThisTickEntry;

    // Обменный пункт
        // Пакеты обновлений игрового мира
        TOS::SpinlockObject<std::vector<TickData>> TickSequence;
    } AsyncContext;



    bool IsConnected = true, IsGoingShutdown = false;

    // PYR - поворот камеры по осям xyz в радианах, PYR_Offset для сглаживание поворота
    glm::vec3 PYR = glm::vec3(0), PYR_Offset = glm::vec3(0);
    double PYR_At = 0;
    static constexpr float PYR_TIME_DELTA = 30;
    GlobalTime GTime;

    struct {
        bool W = false, A = false, S = false, D = false, SHIFT = false, SPACE = false;
        bool CTRL = false;

        void clear()
        {
            std::memset(this, 0, sizeof(*this));
        }
    } Keys;
    Pos::Object Pos = Pos::Object(0), Speed = Pos::Object(0);

    GlobalTime LastSendPYR_POS;

    std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, ChunkCompressedSize>> VisibleChunkCompressed;
    uint64_t VisibleChunkCompressedBytes = 0;

    // Приём данных с сокета
    coro<> run(AsyncUseControl::Lock);
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_Disconnect(Net::AsyncSocket &sock);
    coro<> rP_AssetsBindDK(Net::AsyncSocket &sock);
    coro<> rP_AssetsBindHH(Net::AsyncSocket &sock);
    coro<> rP_AssetsInitSend(Net::AsyncSocket &sock);
    coro<> rP_AssetsNextSend(Net::AsyncSocket &sock);
    coro<> rP_DefinitionsUpdate(Net::AsyncSocket &sock);
    coro<> rP_ChunkVoxels(Net::AsyncSocket &sock);
    coro<> rP_ChunkNodes(Net::AsyncSocket &sock);
    coro<> rP_ChunkLightPrism(Net::AsyncSocket &sock);
    coro<> rP_RemoveRegion(Net::AsyncSocket &sock);
    coro<> rP_Tick(Net::AsyncSocket &sock);
    coro<> rP_TestLinkCameraToEntity(Net::AsyncSocket &sock);
    coro<> rP_TestUnlinkCamera(Net::AsyncSocket &sock);


    // Нужен сокет, на котором только что был согласован игровой протокол (asyncInitGameProtocol)
    ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket> &&socket);

    virtual coro<> asyncDestructor() override;
    void resetResourceSyncState();
};

}
