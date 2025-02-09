#include "ServerSession.hpp"
#include "Client/Abstract.hpp"
#include "Common/Net.hpp"
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <functional>
#include <memory>
#include <Common/Packets.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>


namespace LV::Client {

using namespace TOS;

ServerSession::~ServerSession() {
    WorkDeadline.cancel();
    UseLock.wait_no_use();
}

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

void ServerSession::shutdown(EnumDisconnect type) {
    IsGoingShutdown = true;
    Net::Packet packet;
    packet << (uint8_t) ToServer::L1::System
        << (uint8_t) ToServer::L2System::Disconnect
        << (uint8_t) type;
        
    Socket->pushPacket(std::move(packet));

    std::string reason;
    if(type == EnumDisconnect::ByInterface)
        reason = "по запросу интерфейса";
    else if(type == EnumDisconnect::CriticalError)
        reason = "на сервере произошла критическая ошибка";
    else if(type == EnumDisconnect::ProtocolError)
        reason = "ошибка протокола (клиент)";

    LOG.info() << "Отключение от сервера: " << reason;
}

void ServerSession::onResize(uint32_t width, uint32_t height) {

}

void ServerSession::onChangeFocusState(bool isFocused) {

}

void ServerSession::onCursorPosChange(int32_t width, int32_t height) {

}

void ServerSession::onCursorMove(float xMove, float yMove) {
    glm::vec3 front = Camera.Quat*glm::vec3(0.0f, 0.0f, -1.0f);
}

void ServerSession::onFrameRendering() {

}

void ServerSession::onFrameRenderEnd() {

}

void ServerSession::onCursorBtn(ISurfaceEventListener::EnumCursorBtn btn, bool state) {

}

void ServerSession::onKeyboardBtn(int btn, int state) {
    if(btn == GLFW_KEY_TAB && !state) {
        CursorMode = CursorMode == EnumCursorMoveMode::Default ? EnumCursorMoveMode::MoveAndHidden : EnumCursorMoveMode::Default;
    }
}

void ServerSession::onJoystick() {
    
}

coro<> ServerSession::run() {
    auto useLock = UseLock.lock();

    try {
        while(!IsGoingShutdown && IsConnected) {
            co_await readPacket(*Socket);
        }
    } catch(const std::exception &exc) {
        if(const auto *errc = dynamic_cast<const boost::system::system_error*>(&exc); 
            errc && errc->code() == boost::asio::error::operation_aborted)
        {
            co_return;
        }

        TOS::Logger("ServerSession").warn() << exc.what();
    }

    IsConnected = false;

    co_return;
}

void ServerSession::protocolError() {
    shutdown(EnumDisconnect::ProtocolError);
}

coro<> ServerSession::readPacket(Net::AsyncSocket &sock) {
    uint8_t first = co_await sock.read<uint8_t>();

    switch((ToClient::L1) first) {
    case ToClient::L1::System: co_await rP_System(sock);            co_return;
    case ToClient::L1::Resource: co_await rP_Resource(sock);        co_return;
    case ToClient::L1::Definition: co_await rP_Definition(sock);    co_return;
    case ToClient::L1::Content: co_await rP_Content(sock);          co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_System(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2System) second) {
    case ToClient::L2System::Init:

        co_return;
    case ToClient::L2System::Disconnect:
    {
        EnumDisconnect type = (EnumDisconnect) co_await sock.read<uint8_t>();
        std::string reason = co_await sock.read<std::string>();

        if(type == EnumDisconnect::ByInterface)
            reason = "по запросу интерфейса " + reason;
        else if(type == EnumDisconnect::CriticalError)
            reason = "на сервере произошла критическая ошибка " + reason;
        else if(type == EnumDisconnect::ProtocolError)
            reason = "ошибка протокола (сервер) " + reason;

        LOG.info() << "Отключение от сервера: " << reason;

        co_return;
    }
    case ToClient::L2System::LinkCameraToEntity:

        co_return;
    case ToClient::L2System::UnlinkCamera:

        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Resource(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Resource) second) {
    case ToClient::L2Resource::Texture:

        co_return;
    case ToClient::L2Resource::FreeTexture:

        co_return;
    case ToClient::L2Resource::Sound:

        co_return;
    case ToClient::L2Resource::FreeSound:
    
        co_return;
    case ToClient::L2Resource::Model:

        co_return;
    case ToClient::L2Resource::FreeModel:

        co_return;
    case ToClient::L2Resource::InitResSend:

        co_return;
    case ToClient::L2Resource::ChunkSend:
    
        co_return;
    case ToClient::L2Resource::SendCanceled:
    
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Definition(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Definition) second) {
    case ToClient::L2Definition::World:

        co_return;
    case ToClient::L2Definition::FreeWorld:

        co_return;
    case ToClient::L2Definition::Voxel:

        co_return;
    case ToClient::L2Definition::FreeVoxel:
    
        co_return;
    case ToClient::L2Definition::Node:

        co_return;
    case ToClient::L2Definition::FreeNode:
    
        co_return;
    case ToClient::L2Definition::Portal:

        co_return;
    case ToClient::L2Definition::FreePortal:
    
        co_return;
    case ToClient::L2Definition::Entity:

        co_return;
    case ToClient::L2Definition::FreeEntity:
    
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Content(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Content) second) {
    case ToClient::L2Content::World:

        co_return;
    case ToClient::L2Content::RemoveWorld:

        co_return;
    case ToClient::L2Content::Portal:

        co_return;
    case ToClient::L2Content::RemovePortal:
    
        co_return;
    case ToClient::L2Content::Entity:

        co_return;
    case ToClient::L2Content::RemoveEntity:
    
        co_return;
    case ToClient::L2Content::ChunkVoxels:
    {
        WorldId_c wcId = co_await sock.read<uint8_t>();
        Pos::GlobalChunk::Key posKey = co_await sock.read<Pos::GlobalChunk::Key>();
        Pos::GlobalChunk pos = *(Pos::GlobalChunk*) &posKey;

        std::vector<VoxelCube> cubes(co_await sock.read<uint16_t>());

        for(size_t iter = 0; iter < cubes.size(); iter++) {
            VoxelCube &cube = cubes[iter];
            cube.VoxelId = co_await sock.read<uint16_t>();
            cube.Left.X = co_await sock.read<uint8_t>();
            cube.Left.Y = co_await sock.read<uint8_t>();
            cube.Left.Z = co_await sock.read<uint8_t>();
            cube.Right.X = co_await sock.read<uint8_t>();
            cube.Right.Y = co_await sock.read<uint8_t>();
            cube.Right.Z = co_await sock.read<uint8_t>();
        }

        LOG.info() << "Приняты воксели чанка " << int(wcId) << " / " << pos.X << ":" << pos.Y << ":" << pos.Z << " Вокселей " << cubes.size();

        co_return;
    }

    case ToClient::L2Content::ChunkNodes:
    
        co_return;
    case ToClient::L2Content::ChunkLightPrism:

        co_return;
    case ToClient::L2Content::RemoveChunk:
    
        co_return;
    default:
        protocolError();
    }
}

}