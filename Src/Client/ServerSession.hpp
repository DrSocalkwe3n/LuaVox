#pragma once

#include "Abstract.hpp"
#include "Common/Net.hpp"
#include <TOSLib.hpp>
#include <boost/asio/io_context.hpp>


namespace AL::Client {

class ServerSession : public AsyncObject, public IServerSession, public ISurfaceEventListener {
    std::unique_ptr<Net::AsyncSocket> _Socket;
    Net::AsyncSocket &Socket;
    IRenderSession *RS = nullptr;

public:
    // Нужен сокет, на котором только что был согласован игровой протокол (asyncInitGameProtocol)
    ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket> &&socket, IRenderSession *rs = nullptr)
        : AsyncObject(ioc), _Socket(std::move(socket)), Socket(*socket), RS(rs)
    {
        assert(socket.get());
        co_spawn(run());
    }

    virtual ~ServerSession();

    // Авторизоваться или (зарегистрироваться и авторизоваться) или зарегистрироваться
    static coro<> asyncAuthorizeWithServer(tcp::socket &socket, const std::string username, const std::string token, int a_ar_r, std::function<void(const std::string&)> onProgress = nullptr);
    // Начать игровой протокол в авторизированном сокете
    static coro<std::unique_ptr<Net::AsyncSocket>> asyncInitGameProtocol(asio::io_context &ioc, tcp::socket &&socket, std::function<void(const std::string&)> onProgress = nullptr);



    // ISurfaceEventListener
    
    // virtual void onResize(uint32_t width, uint32_t height) override;
    // virtual void onChangeFocusState(bool isFocused) override;
    // virtual void onCursorPosChange(int32_t width, int32_t height) override;
    // virtual void onCursorMove(float xMove, float yMove) override;
    // virtual void onFrameRendering() override;
    // virtual void onFrameRenderEnd() override;

    // virtual void onCursorBtn(EnumCursorBtn btn, bool state) override;
    // virtual void onKeyboardBtn(int btn, int state) override;
    // virtual void onJoystick() override;

private:
    coro<> run();
};

}