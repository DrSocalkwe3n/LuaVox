#pragma once

#include "Abstract.hpp"
#include "Common/Async.hpp"
#include "Common/Lockable.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include <TOSLib.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <boost/lockfree/spsc_queue.hpp>


namespace LV::Client {

struct ParsedPacket {
    ToClient::L1 Level1;
    uint8_t Level2;

    ParsedPacket(ToClient::L1 l1, uint8_t l2)
        : Level1(l1), Level2(l2)
    {}
    virtual ~ParsedPacket();
};

class ServerSession : public AsyncObject, public IServerSession, public ISurfaceEventListener {
    std::unique_ptr<Net::AsyncSocket> Socket;
    IRenderSession *RS = nullptr;
    DestroyLock UseLock;
    bool IsConnected = true, IsGoingShutdown = false;

    TOS::Logger LOG = "ServerSession";

    struct {
        glm::quat Quat;
    } Camera;

    boost::lockfree::spsc_queue<ParsedPacket*> NetInputPackets;

    //
    glm::vec3 PYR = glm::vec3(0), PYR_Offset = glm::vec3(0);
    double PYR_At = 0;
    static constexpr float PYR_TIME_DELTA = 30;
    GlobalTime GTime;

public:
    // Нужен сокет, на котором только что был согласован игровой протокол (asyncInitGameProtocol)
    ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket> &&socket, IRenderSession *rs = nullptr)
        : AsyncObject(ioc), Socket(std::move(socket)), RS(rs), NetInputPackets(1024)
    {
        assert(Socket.get());
        co_spawn(run());
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

    void waitShutdown() {
        UseLock.wait_no_use();
    }


    // ISurfaceEventListener
    
    virtual void onResize(uint32_t width, uint32_t height) override;
    virtual void onChangeFocusState(bool isFocused) override;
    virtual void onCursorPosChange(int32_t width, int32_t height) override;
    virtual void onCursorMove(float xMove, float yMove) override;

    virtual void onCursorBtn(EnumCursorBtn btn, bool state) override;
    virtual void onKeyboardBtn(int btn, int state) override;
    virtual void onJoystick() override;

    virtual void atFreeDrawTime(GlobalTime gTime, float dTime) override;

private:
    coro<> run();
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_System(Net::AsyncSocket &sock);
    coro<> rP_Resource(Net::AsyncSocket &sock);
    coro<> rP_Definition(Net::AsyncSocket &sock);
    coro<> rP_Content(Net::AsyncSocket &sock);
};

}