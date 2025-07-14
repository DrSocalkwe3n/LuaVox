#include "ServerSession.hpp"
#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "TOSLib.hpp"
#include "glm/ext/quaternion_geometric.hpp"
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
    WorldId_t Id;
    Pos::GlobalChunk Pos;
    std::vector<VoxelCube> Cubes;

    PP_Content_ChunkVoxels(WorldId_t id, Pos::GlobalChunk pos, std::vector<VoxelCube> &&cubes)
        : ParsedPacket(ToClient::L1::Content, (uint8_t) ToClient::L2Content::ChunkVoxels), Id(id), Pos(pos), Cubes(std::move(cubes))
    {}
};

struct PP_Content_ChunkNodes : public ParsedPacket {
    WorldId_t Id;
    Pos::GlobalChunk Pos;
    std::array<Node, 16*16*16> Nodes;

    PP_Content_ChunkNodes(WorldId_t id, Pos::GlobalChunk pos)
        : ParsedPacket(ToClient::L1::Content, (uint8_t) ToClient::L2Content::ChunkNodes), Id(id), Pos(pos)
    {
    }
};

struct PP_Content_RegionRemove : public ParsedPacket {
    WorldId_t Id;
    Pos::GlobalRegion Pos;

    PP_Content_RegionRemove(WorldId_t id, Pos::GlobalRegion pos)
        : ParsedPacket(ToClient::L1::Content, (uint8_t) ToClient::L2Content::RemoveRegion), Id(id), Pos(pos)
    {}
};

struct PP_Definition_FreeNode : public ParsedPacket {
    DefNodeId_t Id;

    PP_Definition_FreeNode(DefNodeId_t id)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::Node), 
            Id(id)
    {}
};

struct PP_Definition_Node : public ParsedPacket {
    DefNodeId_t Id;
    DefNode_t Def;

    PP_Definition_Node(DefNodeId_t id, DefNode_t def)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::Node), 
            Id(id), Def(def)
    {}
};

struct PP_Resource_InitResSend : public ParsedPacket {
    Hash_t Hash;
    BinaryResource Resource;

    PP_Resource_InitResSend(Hash_t hash, BinaryResource res)
        : ParsedPacket(ToClient::L1::Resource, (uint8_t) ToClient::L2Resource::InitResSend), Hash(hash), Resource(res)
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

    deltaPYR.x = std::clamp(PYR.x - yMove*PI_DEG, -PI_HALF+PI_DEG, PI_HALF-PI_DEG)-PYR.x;
    deltaPYR.y = std::fmod(PYR.y - xMove*PI_DEG, PI2)-PYR.y;
    deltaPYR.z = 0;

    double gTime = GTime;
    float deltaTime = 1-std::min<float>(gTime-PYR_At, 1/PYR_TIME_DELTA)*PYR_TIME_DELTA;
    PYR_At = GTime;

    PYR += deltaPYR;
    PYR_Offset = deltaPYR+deltaTime*PYR_Offset;
}

void ServerSession::onCursorBtn(ISurfaceEventListener::EnumCursorBtn btn, bool state) {
    if(!state)
        return;

    if(btn == EnumCursorBtn::Left) {
        Net::Packet packet;

        packet << (uint8_t) ToServer::L1::System
            << (uint8_t) ToServer::L2System::BlockChange
            << uint8_t(0);

        Socket->pushPacket(std::move(packet));
    } else if(btn == EnumCursorBtn::Right) {
        Net::Packet packet;

        packet << (uint8_t) ToServer::L1::System
            << (uint8_t) ToServer::L2System::BlockChange
            << uint8_t(1);

        Socket->pushPacket(std::move(packet));
    }
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

    Speed += glm::vec3(rot*glm::vec4(0, 0, -1, 1)*float(Keys.W))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(-1, 0, 0, 1)*float(Keys.A))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(0, 0, 1, 1)*float(Keys.S))*mltpl;
    Speed += glm::vec3(rot*glm::vec4(1, 0, 0, 1)*float(Keys.D))*mltpl;
    Speed += glm::vec3(0, -1, 0)*float(Keys.SHIFT)*mltpl;
    Speed += glm::vec3(0, 1, 0)*float(Keys.SPACE)*mltpl;

    {
        std::unordered_map<WorldId_t, std::tuple<std::unordered_set<Pos::GlobalChunk>, std::unordered_set<Pos::GlobalRegion>>> changeOrAddList_removeList;
        std::unordered_map<EnumDefContent, std::vector<ResourceId_t>> onContentDefinesAdd;
        std::unordered_map<EnumDefContent, std::vector<ResourceId_t>> onContentDefinesLost;

        // Пакеты
        ParsedPacket *pack;
        while(NetInputPackets.pop(pack)) {
            if(pack->Level1 == ToClient::L1::Definition) {
                ToClient::L2Resource l2 = ToClient::L2Resource(pack->Level2);
                if(l2 == ToClient::L2Resource::InitResSend) {
                    PP_Resource_InitResSend &p = *dynamic_cast<PP_Resource_InitResSend*>(pack);
                    
                }

            } else if(pack->Level1 == ToClient::L1::Definition) {
                ToClient::L2Definition l2 = ToClient::L2Definition(pack->Level2);
                if(l2 == ToClient::L2Definition::Node) {
                    PP_Definition_Node &p = *dynamic_cast<PP_Definition_Node*>(pack);
                    Registry.DefNode[p.Id] = p.Def;
                    onContentDefinesAdd[EnumDefContent::Node].push_back(p.Id);
                } else if(l2 == ToClient::L2Definition::FreeNode) {
                    PP_Definition_FreeNode &p = *dynamic_cast<PP_Definition_FreeNode*>(pack);
                    onContentDefinesLost[EnumDefContent::Node].push_back(p.Id);
                }


            } else if(pack->Level1 == ToClient::L1::Content) {
                ToClient::L2Content l2 = ToClient::L2Content(pack->Level2);
                if(l2 == ToClient::L2Content::ChunkVoxels) {
                    PP_Content_ChunkVoxels &p = *dynamic_cast<PP_Content_ChunkVoxels*>(pack);
                    Pos::GlobalRegion rPos = p.Pos >> 2;
                    Pos::bvec4u cPos = p.Pos & 0x3;

                    Data.Worlds[p.Id].Regions[rPos].Chunks[cPos.pack()].Voxels = std::move(p.Cubes);

                    auto &pair = changeOrAddList_removeList[p.Id];
                    std::get<0>(pair).insert(p.Pos);
                } else if(l2 == ToClient::L2Content::ChunkNodes) {
                    PP_Content_ChunkNodes &p = *dynamic_cast<PP_Content_ChunkNodes*>(pack);
                    Pos::GlobalRegion rPos = p.Pos >> 2;
                    Pos::bvec4u cPos = p.Pos & 0x3;

                    Node *nodes = (Node*) Data.Worlds[p.Id].Regions[rPos].Chunks[cPos.pack()].Nodes.data();
                    std::copy(p.Nodes.begin(), p.Nodes.end(), nodes);
                    
                    auto &pair = changeOrAddList_removeList[p.Id];
                    std::get<0>(pair).insert(p.Pos);
                } else if(l2 == ToClient::L2Content::RemoveRegion) {
                    PP_Content_RegionRemove &p = *dynamic_cast<PP_Content_RegionRemove*>(pack);

                    auto &regions = Data.Worlds[p.Id].Regions;
                    auto obj = regions.find(p.Pos);
                    if(obj != regions.end()) {
                        regions.erase(obj);

                        auto &pair = changeOrAddList_removeList[p.Id];
                        std::get<1>(pair).insert(p.Pos);
                    }
                }
            }

            delete pack;
        }

        if(RS && !changeOrAddList_removeList.empty()) {
            for(auto &pair : changeOrAddList_removeList) {
                // Если случится что чанк был изменён и удалён, то исключаем его обновления
                for(Pos::GlobalRegion removed : std::get<1>(pair.second)) {
                    Pos::GlobalChunk pos = removed << 2;
                    for(int z = 0; z < 4; z++)
                        for(int y = 0; y < 4; y++)
                            for(int x = 0; x < 4; x++) {
                                std::get<0>(pair.second).erase(pos+Pos::GlobalChunk(x, y, z));
                            }
                }

                RS->onChunksChange(pair.first, std::get<0>(pair.second), std::get<1>(pair.second));
            }

            if(!onContentDefinesAdd.empty()) {
                RS->onContentDefinesAdd(std::move(onContentDefinesAdd));
            }

            if(!onContentDefinesLost.empty()) {
                RS->onContentDefinesLost(std::move(onContentDefinesLost));
            }
        }
    }

    // Расчёт камеры
    {
        float deltaTime = 1-std::min<float>(gTime-PYR_At, 1/PYR_TIME_DELTA)*PYR_TIME_DELTA;

        glm::quat quat =
            glm::angleAxis(PYR.x-deltaTime*PYR_Offset.x, glm::vec3(1.f, 0.f, 0.f))
            *
            glm::angleAxis(PYR.y-deltaTime*PYR_Offset.y, glm::vec3(0.f, -1.f, 0.f));

        quat = glm::normalize(quat);

        if(RS)
            RS->setCameraPos(0, Pos, quat);


        // Отправка текущей позиции камеры
        if(gTime-LastSendPYR_POS > 1/20.f)
        {
            LastSendPYR_POS = gTime;
            Net::Packet packet;
            ToServer::PacketQuat q;
            q.fromQuat(glm::inverse(quat));

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
    {
        uint32_t size = co_await sock.read<uint32_t>();
        Hash_t hash;
        co_await sock.read((std::byte*) hash.data(), hash.size());

        uint32_t chunkSize = co_await sock.read<uint32_t>();
        std::u8string data(size, '\0');

        co_await sock.read((std::byte*) data.data(), data.size());

        PP_Resource_InitResSend *packet = new PP_Resource_InitResSend(
            hash,
            std::make_shared<std::u8string>(std::move(data))
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }
    case ToClient::L2Resource::ChunkSend:
    
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Definition(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Definition) second) {
    case ToClient::L2Definition::World: {
        DefWorldId_t cdId = co_await sock.read<DefWorldId_t>();

        co_return;
    }
    case ToClient::L2Definition::FreeWorld: {
        DefWorldId_t cdId = co_await sock.read<DefWorldId_t>();

        co_return;
    }
    case ToClient::L2Definition::Voxel: {
        DefVoxelId_t cdId = co_await sock.read<DefVoxelId_t>();

        co_return;
    }
    case ToClient::L2Definition::FreeVoxel: {
        DefVoxelId_t cdId = co_await sock.read<DefVoxelId_t>();
    
        co_return;
    }
    case ToClient::L2Definition::Node:
    {
        DefNodeId_t id;
        DefNode_t def;
        id = co_await sock.read<DefNodeId_t>();
        def.DrawType = (DefNode_t::EnumDrawType) co_await sock.read<uint8_t>();
        for(int iter = 0; iter < 6; iter++) {
            auto &pl = def.Texs[iter].Pipeline;
            pl.resize(co_await sock.read<uint16_t>());
            co_await sock.read((std::byte*) pl.data(), pl.size());
        }

        PP_Definition_Node *packet = new PP_Definition_Node(
            id,
            def
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }
    case ToClient::L2Definition::FreeNode:
    {
        DefNodeId_t id = co_await sock.read<DefNodeId_t>();
    
        PP_Definition_FreeNode *packet = new PP_Definition_FreeNode(
            id
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }
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
        WorldId_t wcId = co_await sock.read<WorldId_t>();
        Pos::GlobalChunk pos;
        pos.unpack(co_await sock.read<Pos::GlobalChunk::Pack>());

        uint32_t compressedSize = co_await sock.read<uint32_t>();
        assert(compressedSize <= std::pow(2, 24));
        std::u8string compressed(compressedSize, '\0');
        co_await sock.read((std::byte*) compressed.data(), compressedSize);

        PP_Content_ChunkVoxels *packet = new PP_Content_ChunkVoxels(
            wcId,
            pos,
            unCompressVoxels(compressed) // TODO: вынести в отдельный поток
        );

        while(!NetInputPackets.push(packet));

        co_return;
    }

    case ToClient::L2Content::ChunkNodes:
    {
        WorldId_t wcId = co_await sock.read<WorldId_t>();
        Pos::GlobalChunk pos;
        pos.unpack(co_await sock.read<Pos::GlobalChunk::Pack>());

        uint32_t compressedSize = co_await sock.read<uint32_t>();
        assert(compressedSize <= std::pow(2, 24));
        std::u8string compressed(compressedSize, '\0');
        co_await sock.read((std::byte*) compressed.data(), compressedSize);

        PP_Content_ChunkNodes *packet = new PP_Content_ChunkNodes(
            wcId,
            pos
        );

        unCompressNodes(compressed, (Node*) packet->Nodes.data()); // TODO: вынести в отдельный поток
        
        while(!NetInputPackets.push(packet));

        co_return;
    }
    case ToClient::L2Content::ChunkLightPrism:

        co_return;
    case ToClient::L2Content::RemoveRegion: {
        WorldId_t wcId = co_await sock.read<WorldId_t>();
        Pos::GlobalRegion pos;
        pos.unpack(co_await sock.read<Pos::GlobalRegion::Pack>());

        PP_Content_RegionRemove *packet = new PP_Content_RegionRemove(
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