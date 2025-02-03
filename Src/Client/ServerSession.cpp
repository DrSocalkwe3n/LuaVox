#include "ServerSession.hpp"
#include "Common/Net.hpp"
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <functional>
#include <memory>


namespace AL::Client {

using namespace TOS;

ServerSession::~ServerSession() = default;

coro<> ServerSession::asyncAuthorizeWithServer(tcp::socket &socket, const std::string username, const std::string token, int a_ar_r, std::function<void(const std::string&)> onProgress) {
    assert(a_ar_r >= 0 && a_ar_r <= 2);
    
    std::string progress;
    auto addLog = [&](const std::string &msg) {
        progress += '\n';
        progress += msg;

        if(onProgress)
            onProgress('\n'+msg);
    }; 

    if(username.size() > 255) {
        addLog("Имя пользователя слишком велико (>255)");
        MAKE_ERROR(progress);
    }

    if(token.size() > 255) {
        addLog("Пароль слишком велик (>255)");
        MAKE_ERROR(progress);
    }
    
    
    Net::Packet packet;

    packet.write((const std::byte*) "AlterLuanti", 11);
    packet << uint8_t(0) << uint8_t(a_ar_r) << username << token;

    addLog("Отправляем первый пакет, авторизация или регистрация");
    co_await packet.sendAndFastClear(socket);

    addLog("Ожидаем код ответа");
    uint8_t code = co_await Net::AsyncSocket::read<uint8_t>(socket);

    if(code == 0) {
        addLog("Код = Авторизированы");
    } else if(code == 1) {
        addLog("Код = Зарегистрированы и авторизированы");
    } else if(code == 2 || code == 3) {
        if(code == 2)
            addLog("Код = Не удалось зарегистрироваться");
        else
            addLog("Код = Не удалось авторизоваться");

        std::string reason = co_await Net::AsyncSocket::read<std::string>(socket);
        addLog(reason);

        if(code == 2)
            MAKE_ERROR("Не удалось зарегистрироваться, причина: " << reason);
        else
            MAKE_ERROR("Не удалось авторизоваться, причина: " << reason);
    } else {
        addLog("Получен неизвестный код ответа (может это не игровой сервер?), прерываем");
        MAKE_ERROR(progress);
    }
}

coro<std::unique_ptr<Net::AsyncSocket>> ServerSession::asyncInitGameProtocol(asio::io_context &ioc, tcp::socket &&socket, std::function<void(const std::string&)> onProgress) {
    std::string progress;
    auto addLog = [&](const std::string &msg) {
        progress += '\n';
        progress += msg;

        if(onProgress)
            onProgress('\n'+msg);
    }; 

    addLog("Инициализируем игровой протокол");
    uint8_t code = 0;
    co_await Net::AsyncSocket::write<>(socket, code);
    asio::deadline_timer timer(socket.get_executor());

    while(true) {
        code = co_await Net::AsyncSocket::read<uint8_t>(socket);

        if(code == 0) {
            addLog("Код = Успешно");
            break;
        } else if(code == 1) {
            addLog("Код = Ошибка с причиной");
            addLog(co_await Net::AsyncSocket::read<std::string>(socket));
            MAKE_ERROR(progress);
        } else if(code == 2) {
            addLog("Код = Подождать 4 секунды");
            timer.expires_from_now(boost::posix_time::seconds(4));
            co_await timer.async_wait();
            addLog("Ожидаем новый код");
        } else {
            addLog("Получен неизвестный код ответа (может это не игровой сервер?), прерываем");
            MAKE_ERROR(progress);
        }
    }

    co_return std::make_unique<Net::AsyncSocket>(ioc, std::move(socket));
}

}