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

struct ParsedPacket {
    ToClient::L1 Level1;
    uint8_t Level2;

    ParsedPacket(ToClient::L1 l1, uint8_t l2)
        : Level1(l1), Level2(l2)
    {}
    virtual ~ParsedPacket();
};

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

    bool isConnected() {
        return Socket->isAlive() && IsConnected; 
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
    AssetsManager::Ptr AM;

    static constexpr uint64_t TIME_BEFORE_UNLOAD_RESOURCE = 180;
    struct {
        // Существующие привязки ресурсов
        std::unordered_set<ResourceId> ExistBinds[(int) EnumAssets::MAX_ENUM];
        // Используемые в данных момент ресурсы (определяется по действующей привязке)
        std::unordered_map<ResourceId, AssetEntry> InUse[(int) EnumAssets::MAX_ENUM];
        // Недавно использованные ресурсы, пока хранятся здесь в течении TIME_BEFORE_UNLOAD_RESOURCE секунд
        std::unordered_map<std::string, std::pair<AssetEntry, uint64_t>> NotInUse[(int) EnumAssets::MAX_ENUM];
    } Assets;

    struct AssetLoading {
        EnumAssets Type;
        ResourceId Id;
        std::string Domain, Key;
        std::u8string Data;
        size_t Offset;
    };

    struct AssetBindEntry {
        EnumAssets Type;
        ResourceId Id;
        std::string Domain, Key;
        Hash_t Hash;
    };

    struct TickData {
        std::vector<WorldId_t> LostWorld;
        // std::vector<std::pair<WorldId_t, DefWorld>>

    };

    struct AssetsBindsChange {
        // Новые привязки ресурсов
        std::vector<AssetBindEntry> Binds;
        // Потерянные из видимости ресурсы
        std::vector<ResourceId> Lost[(int) EnumAssets::MAX_ENUM];
    };

    struct {
    // Сюда обращается ветка, обрабатывающая сокет; run()
        // Получение ресурсов с сервера
        std::unordered_map<Hash_t, AssetLoading> AssetsLoading;
        // Накопление данных за такт сервера
        TickData ThisTickEntry;

    // Сбда обращается ветка обновления IServerSession, накапливая данные до SyncTick
        // Ресурсы, ожидающие ответа от менеджера кеша
        std::unordered_map<std::string, std::vector<std::pair<std::string, Hash_t>>> ResourceWait[(int) EnumAssets::MAX_ENUM];
        // Полученные ресурсы в ожидании стадии синхронизации такта
        std::unordered_map<EnumAssets, std::vector<AssetEntry>> ReceivedResources;
        // Полученные изменения связок в ожидании стадии синхронизации такта
        AssetsBindsChange Binds;
        // Список ресурсов на которые уже был отправлен запрос на загрузку ресурса
        std::vector<Hash_t> AlreadyLoading;


    // Обменный пункт
        // Полученные ресурсы с сервера
        TOS::SpinlockObject<std::vector<AssetEntry>> LoadedAssets;
        // Изменения в наблюдаемых ресурсах
        TOS::SpinlockObject<std::vector<AssetsBindsChange>> AssetsBinds;
        // Пакеты обновлений игрового мира
        TOS::SpinlockObject<std::queue<TickData>> TickSequence;
    } AsyncContext;



    bool IsConnected = true, IsGoingShutdown = false;

    boost::lockfree::spsc_queue<ParsedPacket*> NetInputPackets;

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

    // Приём данных с сокета
    coro<> run(AsyncUseControl::Lock);
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_System(Net::AsyncSocket &sock);
    coro<> rP_Resource(Net::AsyncSocket &sock);
    coro<> rP_Definition(Net::AsyncSocket &sock);
    coro<> rP_Content(Net::AsyncSocket &sock);


    // Нужен сокет, на котором только что был согласован игровой протокол (asyncInitGameProtocol)
    ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket> &&socket);

    virtual coro<> asyncDestructor() override;
};

}