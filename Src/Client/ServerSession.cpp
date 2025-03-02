#include "ServerSession.hpp"
#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "TOSLib.hpp"
#include <GLFW/glfw3.h>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <functional>
#include <memory>
#include <Common/Packets.hpp>
#include <glm/ext.hpp>
#include <unordered_map>
#include <unordered_set>


namespace LV::Client {

ParsedPacket::~ParsedPacket() = default;

struct PP_Content_ChunkVoxels : public ParsedPacket {
    WorldId_c Id;
    Pos::GlobalChunk Pos;
    std::vector<VoxelCube> Cubes;

    PP_Content_ChunkVoxels(ToClient::L1 l1, uint8_t l2, WorldId_c id, Pos::GlobalChunk pos, std::vector<VoxelCube> &&cubes)
        : ParsedPacket(l1, l2), Id(id), Pos(pos), Cubes(std::move(cubes))
    {}
};

struct PP_Content_ChunkRemove : public ParsedPacket {
    WorldId_c Id;
    Pos::GlobalChunk Pos;

    PP_Content_ChunkRemove(ToClient::L1 l1, uint8_t l2, WorldId_c id, Pos::GlobalChunk pos)
        : ParsedPacket(l1, l2), Id(id), Pos(pos)
    {}
};

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
    Socket->closeRead();
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
    if(!isFocused)
        CursorMode = EnumCursorMoveMode::Default;
}

void ServerSession::onCursorPosChange(int32_t width, int32_t height) {

}

void ServerSession::onCursorMove(float xMove, float yMove) {
    xMove /= 10.f;
    yMove /= 10.f;

    glm::vec3 deltaPYR;

    static constexpr float PI = glm::pi<float>(), PI2 = PI*2, PI_HALF = PI/2, PI_DEG = PI/180;

    deltaPYR.x = std::clamp(PYR.x + yMove*PI_DEG, -PI_HALF+PI_DEG, PI_HALF-PI_DEG)-PYR.x;
    deltaPYR.y = std::fmod(PYR.y + xMove*PI_DEG, PI2)-PYR.y;
    deltaPYR.z = 0;

    double gTime = GTime;
    float deltaTime = 1-std::min<float>(gTime-PYR_At, 1/PYR_TIME_DELTA)*PYR_TIME_DELTA;
    PYR_At = GTime;

    PYR += deltaPYR;
    PYR_Offset = deltaPYR+deltaTime*PYR_Offset;
}

void ServerSession::onCursorBtn(ISurfaceEventListener::EnumCursorBtn btn, bool state) {

}

void ServerSession::onKeyboardBtn(int btn, int state) {
    if(btn == GLFW_KEY_TAB && !state) {
        CursorMode = CursorMode == EnumCursorMoveMode::Default ? EnumCursorMoveMode::MoveAndHidden : EnumCursorMoveMode::Default;
        Keys.clear();
    }


    if(CursorMode == EnumCursorMoveMode::MoveAndHidden)
    {
        if(btn == GLFW_KEY_W)
            Keys.W = state;
        else if(btn == GLFW_KEY_A)
            Keys.A = state;
        else if(btn == GLFW_KEY_S)
            Keys.S = state;
        else if(btn == GLFW_KEY_D)
            Keys.D = state;
        else if(btn == GLFW_KEY_LEFT_SHIFT)
            Keys.SHIFT = state;
        else if(btn == GLFW_KEY_SPACE)
            Keys.SPACE = state;
        else if(btn == GLFW_KEY_LEFT_CONTROL)
            Keys.CTRL = state;
    }
}

void ServerSession::onJoystick() {
    
}

void ServerSession::atFreeDrawTime(GlobalTime gTime, float dTime) {
    GTime = gTime;

    Pos += glm::vec3(Speed) * dTime;
    Speed -= glm::dvec3(Speed) * double(dTime);

    glm::mat4 rot(1);
    float deltaTime = 1-std::min<float>(gTime-PYR_At, 1/PYR_TIME_DELTA)*PYR_TIME_DELTA;
    rot = glm::rotate(rot, PYR.y-deltaTime*PYR_Offset.y, {0, 1, 0});

    float mltpl = 16*dTime*Pos::Object_t::BS;
    if(Keys.CTRL)
        mltpl *= 16;

    Speed += glm::vec3(rot*glm::vec4(0, 0, 1, 1)*float(Keys.W))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(-1, 0, 0, 1)*float(Keys.A))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(0, 0, -1, 1)*float(Keys.S))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(1, 0, 0, 1)*float(Keys.D))*mltpl;
    Speed += glm::vec3(0, -1, 0)*float(Keys.SHIFT)*mltpl;
    Speed += glm::vec3(0, 1, 0)*float(Keys.SPACE)*mltpl;

    {
        std::unordered_map<WorldId_c, std::tuple<std::unordered_set<Pos::GlobalChunk>, std::unordered_set<Pos::GlobalChunk>>> changeOrAddList_removeList;

        // Пакеты
        ParsedPacket *pack;
        while(NetInputPackets.pop(pack)) {
            if(pack->Level1 == ToClient::L1::Content) {
                ToClient::L2Content l2 = ToClient::L2Content(pack->Level2);
                if(l2 == ToClient::L2Content::ChunkVoxels) {
                    PP_Content_ChunkVoxels &p = *dynamic_cast<PP_Content_ChunkVoxels*>(pack);
                    Pos::GlobalRegion rPos(p.Pos.X >> 4, p.Pos.Y >> 4, p.Pos.Z >> 4);
                    Pos::Local16_u cPos(p.Pos.X & 0xf, p.Pos.Y & 0xf, p.Pos.Z & 0xf);

                    External.Worlds[p.Id].Regions[rPos].Chunks[cPos].Voxels = std::move(p.Cubes);

                    auto &pair = changeOrAddList_removeList[p.Id];
                    std::get<0>(pair).insert(p.Pos);
                } else if(l2 == ToClient::L2Content::RemoveChunk) {
                    PP_Content_ChunkRemove &p = *dynamic_cast<PP_Content_ChunkRemove*>(pack);

                    Pos::GlobalRegion rPos(p.Pos.X >> 4, p.Pos.Y >> 4, p.Pos.Z >> 4);
                    Pos::Local16_u cPos(p.Pos.X & 0xf, p.Pos.Y & 0xf, p.Pos.Z & 0xf);
                    auto &obj = External.Worlds[p.Id].Regions[rPos].Chunks;
                    auto iter = obj.find(cPos);
                    if(iter != obj.end())
                        obj.erase(iter);

                    auto &pair = changeOrAddList_removeList[p.Id];
                    std::get<1>(pair).insert(p.Pos);
                }
            }

            delete pack;
        }

        if(RS && !changeOrAddList_removeList.empty()) {
            for(auto &pair : changeOrAddList_removeList) {
                // Если случится что чанк был изменён и удалён, то исключаем его обновления
                for(Pos::GlobalChunk removed : std::get<1>(pair.second))
                    std::get<0>(pair.second).erase(removed);

                RS->onChunksChange(pair.first, std::get<0>(pair.second), std::get<1>(pair.second));
            }
        }
    }

    // Расчёт камеры
    {
        float deltaTime = 1-std::min<float>(gTime-PYR_At, 1/PYR_TIME_DELTA)*PYR_TIME_DELTA;

        glm::quat quat =
            glm::angleAxis(PYR.x-deltaTime*PYR_Offset.x, glm::vec3(1.f, 0.f, 0.f))
            *   glm::angleAxis(PYR.y-deltaTime*PYR_Offset.y, glm::vec3(0.f, -1.f, 0.f));

        if(RS)
            RS->setCameraPos(0, Pos, quat);


        // Отправка текущей позиции камеры
        if(gTime-LastSendPYR_POS > 1/20.f)
        {
            LastSendPYR_POS = gTime;
            Net::Packet packet;
            ToServer::PacketQuat q;
            q.fromQuat(quat);

            packet << (uint8_t) ToServer::L1::System
                << (uint8_t) ToServer::L2System::Test_CAM_PYR_POS
                << Pos.x << Pos.y << Pos.z;

            for(int iter = 0; iter < 5; iter++)
                packet << q.Data[iter];

            Socket->pushPacket(std::move(packet));
        }
    }
}

coro<> ServerSession::run() {
    auto useLock = UseLock.lock();

    try {
        while(!IsGoingShutdown && IsConnected) {
            co_await readPacket(*Socket);
        }
    } catch(const std::exception &exc) {
        // if(const auto *errc = dynamic_cast<const boost::system::system_error*>(&exc); 
        //     errc && errc->code() == boost::asio::error::operation_aborted)
        // {
        //     co_return;
        // }

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
    case ToClient::L2Definition::World: {
        DefWorldId_c cdId = co_await sock.read<DefWorldId_c>();

        co_return;
    }
    case ToClient::L2Definition::FreeWorld: {
        DefWorldId_c cdId = co_await sock.read<DefWorldId_c>();

        co_return;
    }
    case ToClient::L2Definition::Voxel: {
        DefVoxelId_c cdId = co_await sock.read<DefVoxelId_c>();

        co_return;
    }
    case ToClient::L2Definition::FreeVoxel: {
        DefVoxelId_c cdId = co_await sock.read<DefVoxelId_c>();
    
        co_return;
    }
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
        WorldId_c wcId = co_await sock.read<WorldId_c>();
        Pos::GlobalChunk::Key posKey = co_await sock.read<Pos::GlobalChunk::Key>();
        Pos::GlobalChunk pos = *(Pos::GlobalChunk*) &posKey;

        std::vector<VoxelCube> cubes(co_await sock.read<uint16_t>());
        uint16_t debugCubesCount = cubes.size();
        if(debugCubesCount > 1) {
            int g = 0;
            g++;
        }

        for(size_t iter = 0; iter < cubes.size(); iter++) {
            VoxelCube &cube = cubes[iter];
            cube.VoxelId = co_await sock.read<uint16_t>();
            cube.Left.X = co_await sock.read<uint8_t>();
            cube.Left.Y = co_await sock.read<uint8_t>();
            cube.Left.Z = co_await sock.read<uint8_t>();
            cube.Size.X = co_await sock.read<uint8_t>();
            cube.Size.Y = co_await sock.read<uint8_t>();
            cube.Size.Z = co_await sock.read<uint8_t>();
        }

        PP_Content_ChunkVoxels *packet = new PP_Content_ChunkVoxels(
            ToClient::L1::Content,
            (uint8_t) ToClient::L2Content::ChunkVoxels,
            wcId,
            pos,
            std::move(cubes)
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }

    case ToClient::L2Content::ChunkNodes:
    
        co_return;
    case ToClient::L2Content::ChunkLightPrism:

        co_return;
    case ToClient::L2Content::RemoveChunk: {
        WorldId_c wcId = co_await sock.read<uint8_t>();
        Pos::GlobalChunk::Key posKey = co_await sock.read<Pos::GlobalChunk::Key>();
        Pos::GlobalChunk pos = *(Pos::GlobalChunk*) &posKey;

        PP_Content_ChunkRemove *packet = new PP_Content_ChunkRemove(
            ToClient::L1::Content,
            (uint8_t) ToClient::L2Content::RemoveChunk,
            wcId,
            pos
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }
    default:
        protocolError();
    }
}

}