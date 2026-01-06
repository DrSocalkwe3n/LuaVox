#include "ServerSession.hpp"
#include "Client/Abstract.hpp"
#include "Client/AssetsManager.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "TOSAsync.hpp"
#include "TOSLib.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <functional>
#include <memory>
#include <Common/Packets.hpp>
#include <glm/ext.hpp>
#include <optional>
#include <unordered_map>
#include <unordered_set>


namespace LV::Client {

namespace {

const char* assetTypeName(EnumAssets type) {
    switch(type) {
    case EnumAssets::Nodestate: return "nodestate";
    case EnumAssets::Model: return "model";
    case EnumAssets::Texture: return "texture";
    case EnumAssets::Particle: return "particle";
    case EnumAssets::Animation: return "animation";
    case EnumAssets::Sound: return "sound";
    case EnumAssets::Font: return "font";
    default: return "unknown";
    }
}

const char* toClientPacketName(ToClient type) {
    switch(type) {
    case ToClient::Init: return "Init";
    case ToClient::Disconnect: return "Disconnect";
    case ToClient::AssetsBindDK: return "AssetsBindDK";
    case ToClient::AssetsBindHH: return "AssetsBindHH";
    case ToClient::AssetsInitSend: return "AssetsInitSend";
    case ToClient::AssetsNextSend: return "AssetsNextSend";
    case ToClient::DefinitionsUpdate: return "DefinitionsUpdate";
    case ToClient::ChunkVoxels: return "ChunkVoxels";
    case ToClient::ChunkNodes: return "ChunkNodes";
    case ToClient::ChunkLightPrism: return "ChunkLightPrism";
    case ToClient::RemoveRegion: return "RemoveRegion";
    case ToClient::Tick: return "Tick";
    case ToClient::TestLinkCameraToEntity: return "TestLinkCameraToEntity";
    case ToClient::TestUnlinkCamera: return "TestUnlinkCamera";
    default: return "Unknown";
    }
}

}

ServerSession::ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket>&& socket)
    : IAsyncDestructible(ioc), Socket(std::move(socket)), AM(ioc, "Cache")
{
    assert(Socket.get());

    Profiles.DefNode[0] = {0};
    Profiles.DefNode[1] = {1};
    Profiles.DefNode[2] = {2};
    Profiles.DefNode[3] = {3};
    Profiles.DefNode[4] = {4};

    try {
        asio::co_spawn(ioc, run(AUC.use()), asio::detached);
        // TODO: добавить оптимизацию для подключения клиента к внутреннему серверу
    } catch(const std::exception &exc) {
        MAKE_ERROR("Ошибка инициализации обработчика объекта подключения к серверу:\n" << exc.what());
    }
}

coro<> ServerSession::asyncDestructor() {
    co_await IAsyncDestructible::asyncDestructor();
}



using namespace TOS;

ServerSession::~ServerSession() {
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

void ServerSession::requestModsReload() {
    if(!Socket || !isConnected())
        return;

    Net::Packet packet;
    packet << (uint8_t) ToServer::L1::System
        << (uint8_t) ToServer::L2System::ReloadMods;

    Socket->pushPacket(std::move(packet));
    LOG.info() << "Запрос на перезагрузку модов отправлен";
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

void ServerSession::update(GlobalTime gTime, float dTime) {
    // Если AssetsManager запрашивает ресурсы с сервера
    {
        std::vector<Hash_t> needRequest = AM.pullNeededResources();
        if(!needRequest.empty()) {
            Net::Packet pack;
            std::vector<Net::Packet> packets;

            auto check = [&]() {
                if(pack.size() > 64000)
                    packets.emplace_back(std::move(pack));
            };

            pack << (uint8_t) ToServer::L1::System << (uint8_t) ToServer::L2System::ResourceRequest;
            pack << (uint16_t) needRequest.size();
            for(const Hash_t& hash : needRequest) {
                pack.write((const std::byte*) hash.data(), 32);
                check();
            }

            if(pack.size())
                packets.emplace_back(std::move(pack));

            Socket->pushPackets(&packets);
        }
    }

    if(!AsyncContext.TickSequence.get_read().empty()) {
        // Есть такты с сервера
        // Оповещаем о подготовке к обработке тактов
        if(RS)
            RS->prepareTickSync();

        std::vector<TickData> ticks = std::move(*AsyncContext.TickSequence.lock());

        IRenderSession::TickSyncData result;
        // Перевариваем данные по тактам

        // Профили
        std::unordered_map<DefVoxelId, void*> profile_Voxel_AddOrChange;
        std::vector<DefVoxelId> profile_Voxel_Lost;
        std::unordered_map<DefNodeId, DefNode_t> profile_Node_AddOrChange;
        std::vector<DefNodeId> profile_Node_Lost;
        std::unordered_map<DefWorldId, void*> profile_World_AddOrChange;
        std::vector<DefWorldId> profile_World_Lost;
        std::unordered_map<DefPortalId, void*> profile_Portal_AddOrChange;
        std::vector<DefPortalId> profile_Portal_Lost;
        std::unordered_map<DefEntityId, DefEntityInfo> profile_Entity_AddOrChange;
        std::vector<DefEntityId> profile_Entity_Lost;
        std::unordered_map<DefItemId, void*> profile_Item_AddOrChange;
        std::vector<DefItemId> profile_Item_Lost;
        std::unordered_map<EntityId_t, EntityInfo> entity_AddOrChange;
        std::vector<EntityId_t> entity_Lost;

        {
            for(TickData& data : ticks) {
                // Привязки Id -> Domain+Key
                for(const auto& binds : data.BindsDK) {
                    AM.pushAssetsBindDK(binds.Domains, binds.Keys);
                }

                // Привязки Id -> Hash+Header
                for(auto& binds : data.BindsHH) {
                    AM.pushAssetsBindHH(std::move(binds.HashAndHeaders));
                }

                // Полученные ресурсы с сервера
                if(!data.ReceivedAssets.empty())
                    AM.pushNewResources(std::move(data.ReceivedAssets));

                {
                    for(auto& [id, profile] : data.Profile_Voxel_AddOrChange) {
                        auto iter = std::lower_bound(profile_Voxel_Lost.begin(), profile_Voxel_Lost.end(), id);
                        if(iter != profile_Voxel_Lost.end() && *iter == id)
                            profile_Voxel_Lost.erase(iter);

                        profile_Voxel_AddOrChange[id] = profile;
                    }

                    for(DefVoxelId id : data.Profile_Voxel_Lost) {
                        profile_Voxel_AddOrChange.erase(id);
                    }

                    profile_Voxel_Lost.insert(profile_Voxel_Lost.end(), data.Profile_Voxel_Lost.begin(), data.Profile_Voxel_Lost.end());
                    std::sort(profile_Voxel_Lost.begin(), profile_Voxel_Lost.end());
                    auto eraseIter = std::unique(profile_Voxel_Lost.begin(), profile_Voxel_Lost.end());
                    profile_Voxel_Lost.erase(eraseIter, profile_Voxel_Lost.end());
                }

                {
                    for(auto& [id, profile] : data.Profile_Node_AddOrChange) {
                        auto iter = std::lower_bound(profile_Node_Lost.begin(), profile_Node_Lost.end(), id);
                        if(iter != profile_Node_Lost.end() && *iter == id)
                            profile_Node_Lost.erase(iter);

                        profile_Node_AddOrChange[id] = profile;
                    }

                    for(DefNodeId id : data.Profile_Node_Lost) {
                        profile_Node_AddOrChange.erase(id);
                    }

                    profile_Node_Lost.insert(profile_Node_Lost.end(), data.Profile_Node_Lost.begin(), data.Profile_Node_Lost.end());
                    std::sort(profile_Node_Lost.begin(), profile_Node_Lost.end());
                    auto eraseIter = std::unique(profile_Node_Lost.begin(), profile_Node_Lost.end());
                    profile_Node_Lost.erase(eraseIter, profile_Node_Lost.end());
                }

                {
                    for(auto& [id, profile] : data.Profile_World_AddOrChange) {
                        auto iter = std::lower_bound(profile_World_Lost.begin(), profile_World_Lost.end(), id);
                        if(iter != profile_World_Lost.end() && *iter == id)
                            profile_World_Lost.erase(iter);

                        profile_World_AddOrChange[id] = profile;
                    }

                    for(DefWorldId id : data.Profile_World_Lost) {
                        profile_World_AddOrChange.erase(id);
                    }

                    profile_World_Lost.insert(profile_World_Lost.end(), data.Profile_World_Lost.begin(), data.Profile_World_Lost.end());
                    std::sort(profile_World_Lost.begin(), profile_World_Lost.end());
                    auto eraseIter = std::unique(profile_World_Lost.begin(), profile_World_Lost.end());
                    profile_World_Lost.erase(eraseIter, profile_World_Lost.end());
                }

                {
                    for(auto& [id, profile] : data.Profile_Portal_AddOrChange) {
                        auto iter = std::lower_bound(profile_Portal_Lost.begin(), profile_Portal_Lost.end(), id);
                        if(iter != profile_Portal_Lost.end() && *iter == id)
                            profile_Portal_Lost.erase(iter);

                        profile_Portal_AddOrChange[id] = profile;
                    }

                    for(DefPortalId id : data.Profile_Portal_Lost) {
                        profile_Portal_AddOrChange.erase(id);
                    }

                    profile_Portal_Lost.insert(profile_Portal_Lost.end(), data.Profile_Portal_Lost.begin(), data.Profile_Portal_Lost.end());
                    std::sort(profile_Portal_Lost.begin(), profile_Portal_Lost.end());
                    auto eraseIter = std::unique(profile_Portal_Lost.begin(), profile_Portal_Lost.end());
                    profile_Portal_Lost.erase(eraseIter, profile_Portal_Lost.end());
                }

                {
                    for(auto& [id, profile] : data.Profile_Entity_AddOrChange) {
                        auto iter = std::lower_bound(profile_Entity_Lost.begin(), profile_Entity_Lost.end(), id);
                        if(iter != profile_Entity_Lost.end() && *iter == id)
                            profile_Entity_Lost.erase(iter);

                        profile_Entity_AddOrChange[id] = profile;
                    }

                    for(DefEntityId id : data.Profile_Entity_Lost) {
                        profile_Entity_AddOrChange.erase(id);
                    }

                    profile_Entity_Lost.insert(profile_Entity_Lost.end(), data.Profile_Entity_Lost.begin(), data.Profile_Entity_Lost.end());
                    std::sort(profile_Entity_Lost.begin(), profile_Entity_Lost.end());
                    auto eraseIter = std::unique(profile_Entity_Lost.begin(), profile_Entity_Lost.end());
                    profile_Entity_Lost.erase(eraseIter, profile_Entity_Lost.end());
                }

                {
                    for(auto& [id, profile] : data.Profile_Item_AddOrChange) {
                        auto iter = std::lower_bound(profile_Item_Lost.begin(), profile_Item_Lost.end(), id);
                        if(iter != profile_Item_Lost.end() && *iter == id)
                            profile_Item_Lost.erase(iter);

                        profile_Item_AddOrChange[id] = profile;
                    }

                    for(DefItemId id : data.Profile_Item_Lost) {
                        profile_Item_AddOrChange.erase(id);
                    }

                    profile_Item_Lost.insert(profile_Item_Lost.end(), data.Profile_Item_Lost.begin(), data.Profile_Item_Lost.end());
                    std::sort(profile_Item_Lost.begin(), profile_Item_Lost.end());
                    auto eraseIter = std::unique(profile_Item_Lost.begin(), profile_Item_Lost.end());
                    profile_Item_Lost.erase(eraseIter, profile_Item_Lost.end());
                }

                {
            for(auto& [id, info] : data.Entity_AddOrChange) {
                auto iter = std::lower_bound(entity_Lost.begin(), entity_Lost.end(), id);
                if(iter != entity_Lost.end() && *iter == id)
                    entity_Lost.erase(iter);

                        entity_AddOrChange[id] = info;
                    }

                    for(EntityId_t id : data.Entity_Lost) {
                        entity_AddOrChange.erase(id);
                    }

                    entity_Lost.insert(entity_Lost.end(), data.Entity_Lost.begin(), data.Entity_Lost.end());
                    std::sort(entity_Lost.begin(), entity_Lost.end());
                    auto eraseIter = std::unique(entity_Lost.begin(), entity_Lost.end());
                entity_Lost.erase(eraseIter, entity_Lost.end());
            }
        }

        {
            AssetsManager::ResourceUpdates updates = AM.pullResourceUpdates();

            if(!updates.Models.empty()) {
                auto& map = Assets[EnumAssets::Model];
                for(auto& update : updates.Models) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Model = std::move(update.Model);
                    entry.ModelHeader = std::move(update.Header);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Model].push_back(update.Id);
                }
            }

            if(!updates.Nodestates.empty()) {
                auto& map = Assets[EnumAssets::Nodestate];
                for(auto& update : updates.Nodestates) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Nodestate = std::move(update.Nodestate);
                    entry.NodestateHeader = std::move(update.Header);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Nodestate].push_back(update.Id);
                }
            }

            if(!updates.Textures.empty()) {
                auto& map = Assets[EnumAssets::Texture];
                for(auto& update : updates.Textures) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Domain = std::move(update.Domain);
                    entry.Key = std::move(update.Key);
                    entry.Width = update.Width;
                    entry.Height = update.Height;
                    entry.Pixels = std::move(update.Pixels);
                    entry.Header = std::move(update.Header);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Texture].push_back(update.Id);
                }
            }

            if(!updates.Particles.empty()) {
                auto& map = Assets[EnumAssets::Particle];
                for(auto& update : updates.Particles) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Data = std::move(update.Data);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Particle].push_back(update.Id);
                }
            }

            if(!updates.Animations.empty()) {
                auto& map = Assets[EnumAssets::Animation];
                for(auto& update : updates.Animations) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Data = std::move(update.Data);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Animation].push_back(update.Id);
                }
            }

            if(!updates.Sounds.empty()) {
                auto& map = Assets[EnumAssets::Sound];
                for(auto& update : updates.Sounds) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Data = std::move(update.Data);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Sound].push_back(update.Id);
                }
            }

            if(!updates.Fonts.empty()) {
                auto& map = Assets[EnumAssets::Font];
                for(auto& update : updates.Fonts) {
                    AssetEntry entry;
                    entry.Id = update.Id;
                    entry.Data = std::move(update.Data);
                    map[entry.Id] = std::move(entry);
                    result.Assets_ChangeOrAdd[EnumAssets::Font].push_back(update.Id);
                }
            }
        }

        for(auto& [id, _] : profile_Voxel_AddOrChange)
            result.Profiles_ChangeOrAdd[EnumDefContent::Voxel].push_back(id);
        result.Profiles_Lost[EnumDefContent::Voxel] = profile_Voxel_Lost;

            for(auto& [id, _] : profile_Node_AddOrChange)
                result.Profiles_ChangeOrAdd[EnumDefContent::Node].push_back(id);
            result.Profiles_Lost[EnumDefContent::Node] = profile_Node_Lost;

            for(auto& [id, _] : profile_World_AddOrChange)
                result.Profiles_ChangeOrAdd[EnumDefContent::World].push_back(id);
            result.Profiles_Lost[EnumDefContent::World] = profile_World_Lost;

            for(auto& [id, _] : profile_Portal_AddOrChange)
                result.Profiles_ChangeOrAdd[EnumDefContent::Portal].push_back(id);
            result.Profiles_Lost[EnumDefContent::Portal] = profile_Portal_Lost;

            for(auto& [id, _] : profile_Entity_AddOrChange)
                result.Profiles_ChangeOrAdd[EnumDefContent::Entity].push_back(id);
            result.Profiles_Lost[EnumDefContent::Entity] = profile_Entity_Lost;

            for(auto& [id, _] : profile_Item_AddOrChange)
                result.Profiles_ChangeOrAdd[EnumDefContent::Item].push_back(id);
            result.Profiles_Lost[EnumDefContent::Item] = profile_Item_Lost;
        }

        // Чанки
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::vector<VoxelCube>>> chunks_AddOrChange_Voxel_Result;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::array<Node, 16*16*16>>> chunks_AddOrChange_Node_Result;
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> chunks_Changed;
        std::unordered_map<WorldId_t, std::unordered_set<Pos::GlobalRegion>> regions_Lost_Result;

        {
            std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::u8string>> chunks_AddOrChange_Voxel;
            std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::u8string>> chunks_AddOrChange_Node;
            std::unordered_map<WorldId_t, std::unordered_set<Pos::GlobalRegion>> regions_Lost;

            for(TickData& data : ticks) {
                for(auto& [wId, chunks] : data.Chunks_AddOrChange_Voxel) {
                    if(auto iter = regions_Lost.find(wId); iter != regions_Lost.end()) {
                        for(const auto& [pos, value] : chunks) {
                            iter->second.erase(Pos::GlobalRegion(pos >> 2));
                        }
                    }

                    chunks_AddOrChange_Voxel[wId].merge(chunks);
                }

                data.Chunks_AddOrChange_Voxel.clear();

                for(auto& [wId, chunks] : data.Chunks_AddOrChange_Node) {
                    if(auto iter = regions_Lost.find(wId); iter != regions_Lost.end()) {
                        for(const auto& [pos, value] : chunks) {
                            iter->second.erase(Pos::GlobalRegion(pos >> 2));
                        }
                    }

                    chunks_AddOrChange_Node[wId].merge(chunks);
                }

                data.Chunks_AddOrChange_Node.clear();

                for(auto& [wId, regions] : data.Regions_Lost) {
                    std::sort(regions.begin(), regions.end());

                    if(auto iter = chunks_AddOrChange_Voxel.find(wId); iter != chunks_AddOrChange_Voxel.end())
                    {
                        std::vector<Pos::GlobalChunk> toDelete;
                        for(auto& [pos, value] : iter->second) {
                            if(std::binary_search(regions.begin(), regions.end(), Pos::GlobalRegion(pos >> 2))) {
                                toDelete.push_back(pos);
                            }
                        }

                        for(Pos::GlobalChunk pos : toDelete)
                            iter->second.erase(iter->second.find(pos));
                    }

                    if(auto iter = chunks_AddOrChange_Node.find(wId); iter != chunks_AddOrChange_Node.end())
                    {
                        std::vector<Pos::GlobalChunk> toDelete;
                        for(auto& [pos, value] : iter->second) {
                            if(std::binary_search(regions.begin(), regions.end(), Pos::GlobalRegion(pos >> 2))) {
                                toDelete.push_back(pos);
                            }
                        }

                        for(Pos::GlobalChunk pos : toDelete)
                            iter->second.erase(iter->second.find(pos));
                    }
                
                    regions_Lost[wId].insert_range(regions);
                }

                data.Regions_Lost.clear();
            }

            for(auto& [wId, list] : chunks_AddOrChange_Voxel) {
                auto& caocvr = chunks_AddOrChange_Voxel_Result[wId];
                auto& c = chunks_Changed[wId];

                for(auto& [pos, val] : list) {
                    auto& sizes = VisibleChunkCompressed[wId][pos];
                    VisibleChunkCompressedBytes -= sizes.Voxels;
                    sizes.Voxels = val.size();
                    VisibleChunkCompressedBytes += sizes.Voxels;

                    caocvr[pos] = unCompressVoxels(val);
                    c.push_back(pos);
                }
            }

            for(auto& [wId, list] : chunks_AddOrChange_Node) {
                auto& caocvr = chunks_AddOrChange_Node_Result[wId];
                auto& c = chunks_Changed[wId];

                for(auto& [pos, val] : list) {
                    auto& sizes = VisibleChunkCompressed[wId][pos];
                    VisibleChunkCompressedBytes -= sizes.Nodes;
                    sizes.Nodes = val.size();
                    VisibleChunkCompressedBytes += sizes.Nodes;

                    auto& chunkNodes = caocvr[pos];
                    unCompressNodes(val, chunkNodes.data());
                    c.push_back(pos);
                }
            }

            regions_Lost_Result = std::move(regions_Lost);

            for(auto& [wId, list] : chunks_Changed) {
                std::sort(list.begin(), list.end());
                auto eraseIter = std::unique(list.begin(), list.end());
                list.erase(eraseIter, list.end());
            }
        }

        result.Chunks_ChangeOrAdd = std::move(chunks_Changed);
        for(auto& [wId, regions] : regions_Lost_Result)
            result.Chunks_Lost[wId] = std::vector<Pos::GlobalRegion>(regions.begin(), regions.end());


        {
            for(TickData& data : ticks) {
            }
        }

        if(RS)
            RS->pushStageTickSync();

        // Применяем изменения по ресурсам, профилям и контенту
        // Определения
        {
            for(auto& [resId, def] : profile_Node_AddOrChange) {
                Profiles.DefNode[resId] = def;
            }
            for(auto& [resId, def] : profile_Entity_AddOrChange) {
                Profiles.DefEntity[resId] = def;
            }
        }

        // Чанки
        {
            for(auto& [wId, lost] : regions_Lost_Result) {
                auto iterWorld = Content.Worlds.find(wId);
                auto iterSizesWorld = VisibleChunkCompressed.find(wId);
                if(iterWorld != Content.Worlds.end()) {
                    for(const Pos::GlobalRegion& rPos : lost) {
                        auto iterRegion = iterWorld->second.Regions.find(rPos);
                        if(iterRegion != iterWorld->second.Regions.end())
                            iterWorld->second.Regions.erase(iterRegion);
                    }
                }

                if(iterSizesWorld == VisibleChunkCompressed.end())
                    continue;

                for(const Pos::GlobalRegion& rPos : lost) {
                    for(auto iter = iterSizesWorld->second.begin(); iter != iterSizesWorld->second.end(); ) {
                        if(Pos::GlobalRegion(iter->first >> 2) == rPos) {
                            VisibleChunkCompressedBytes -= iter->second.Voxels;
                            VisibleChunkCompressedBytes -= iter->second.Nodes;
                            iter = iterSizesWorld->second.erase(iter);
                        } else {
                            ++iter;
                        }
                    }
                }

                if(iterSizesWorld->second.empty())
                    VisibleChunkCompressed.erase(iterSizesWorld);
            }

            for(auto& [wId, voxels] : chunks_AddOrChange_Voxel_Result) {
                auto& regions = Content.Worlds[wId].Regions;

                for(auto& [pos, data] : voxels) {
                    regions[pos >> 2].Chunks[Pos::bvec4u(pos & 0x3).pack()].Voxels = std::move(data);
                }
            }

            for(auto& [wId, nodes] : chunks_AddOrChange_Node_Result) {
                auto& regions = Content.Worlds[wId].Regions;

                for(auto& [pos, data] : nodes) {
                    regions[pos >> 2].Chunks[Pos::bvec4u(pos & 0x3).pack()].Nodes = std::move(data);
                }
            }

        }

        // Сущности
        {
            for(auto& [entityId, info] : entity_AddOrChange) {
                auto iter = Content.Entityes.find(entityId);
                if(iter != Content.Entityes.end() && iter->second.WorldId != info.WorldId) {
                    auto iterWorld = Content.Worlds.find(iter->second.WorldId);
                    if(iterWorld != Content.Worlds.end()) {
                        auto &list = iterWorld->second.Entitys;
                        list.erase(std::remove(list.begin(), list.end(), entityId), list.end());
                    }
                }

                Content.Entityes[entityId] = info;

                auto &list = Content.Worlds[info.WorldId].Entitys;
                if(std::find(list.begin(), list.end(), entityId) == list.end())
                    list.push_back(entityId);
            }

            for(EntityId_t entityId : entity_Lost) {
                auto iter = Content.Entityes.find(entityId);
                if(iter != Content.Entityes.end()) {
                    auto iterWorld = Content.Worlds.find(iter->second.WorldId);
                    if(iterWorld != Content.Worlds.end()) {
                        auto &list = iterWorld->second.Entitys;
                        list.erase(std::remove(list.begin(), list.end(), entityId), list.end());
                    }
                    Content.Entityes.erase(iter);
                } else {
                    for(auto& [wId, worldInfo] : Content.Worlds) {
                        auto &list = worldInfo.Entitys;
                        list.erase(std::remove(list.begin(), list.end(), entityId), list.end());
                    }
                }
            }
        }

        if(RS)
            RS->tickSync(result);
    }

    AM.tick();

    // Здесь нужно обработать управляющие пакеты







    // Оповещение модуля рендера об изменениях ресурсов
    // std::unordered_map<EnumAssets, std::unordered_map<ResourceId, AssetEntry>> changedResources;
    // std::unordered_map<EnumAssets, std::unordered_set<ResourceId>> lostResources;

    // Обработка полученных ресурсов

    // Получаем ресурсы, загруженные с дискового кеша

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

    // {
    //     // Пакеты
    //     ParsedPacket *pack;
    //     while(NetInputPackets.pop(pack)) {
    //         delete pack;
    //     }
    // }

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

void ServerSession::setRenderSession(IRenderSession* session) {
    RS = session;
}

void ServerSession::resetResourceSyncState() {
    AsyncContext.AssetsLoading.clear();
    AsyncContext.ThisTickEntry = {};
    AsyncContext.TickSequence.lock()->clear();
}

coro<> ServerSession::run(AsyncUseControl::Lock) {
    try {
        while(!IsGoingShutdown && IsConnected) {
            co_await readPacket(*Socket);
        }
    } catch(const std::exception &exc) {
        LOG.error() << "Ошибка обработки сокета:\n" << exc.what();
    }

    IsConnected = false;
    resetResourceSyncState();

    co_return;
}

void ServerSession::protocolError() {
    shutdown(EnumDisconnect::ProtocolError);
}

coro<> ServerSession::readPacket(Net::AsyncSocket &sock) {
    uint8_t first = co_await sock.read<uint8_t>();

    if(DebugLogPackets) {
        ToClient type = static_cast<ToClient>(first);
        LOG.debug() << "Recv packet=" << toClientPacketName(type) << " id=" << int(first);
    }

    switch((ToClient) first) {
    case ToClient::Init:
        co_return;
    case ToClient::Disconnect:
        co_await rP_Disconnect(sock);
        co_return;
    case ToClient::AssetsBindDK:
        co_await rP_AssetsBindDK(sock);
        co_return;
    case ToClient::AssetsBindHH:
        co_await rP_AssetsBindHH(sock);
        co_return;
    case ToClient::AssetsInitSend:
        co_await rP_AssetsInitSend(sock);
        co_return;
    case ToClient::AssetsNextSend:
        co_await rP_AssetsNextSend(sock);
        co_return;
    case ToClient::DefinitionsUpdate:
        co_await rP_DefinitionsUpdate(sock);
        co_return;
    case ToClient::ChunkVoxels:
        co_await rP_ChunkVoxels(sock);
        co_return;
    case ToClient::ChunkNodes:
        co_await rP_ChunkNodes(sock);
        co_return;
    case ToClient::ChunkLightPrism:
        co_await rP_ChunkLightPrism(sock);
        co_return;
    case ToClient::RemoveRegion:
        co_await rP_RemoveRegion(sock);
        co_return;
    case ToClient::Tick:
        co_await rP_Tick(sock);
        co_return;
    case ToClient::TestLinkCameraToEntity:
        co_await rP_TestLinkCameraToEntity(sock);
        co_return;
    case ToClient::TestUnlinkCamera:
        co_await rP_TestUnlinkCamera(sock);
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Disconnect(Net::AsyncSocket &sock) {
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

coro<> ServerSession::rP_AssetsBindDK(Net::AsyncSocket &sock) {
    UpdateAssetsBindsDK update;

    std::string compressed = co_await sock.read<std::string>();
    std::u8string in((const char8_t*) compressed.data(), compressed.size());
    std::u8string data = unCompressLinear(in);
    Net::LinearReader lr(data);

    uint16_t domainsCount = lr.read<uint16_t>();
    update.Domains.reserve(domainsCount);
    for(uint16_t i = 0; i < domainsCount; ++i)
        update.Domains.push_back(lr.read<std::string>());

    for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
        update.Keys[type].resize(update.Domains.size());

        uint32_t count = lr.read<uint32_t>();
        for(uint32_t i = 0; i < count; ++i) {
            uint16_t domainId = lr.read<uint16_t>();
            std::string key = lr.read<std::string>();
            if(domainId >= update.Domains.size())
                continue;

            update.Keys.at(type).at(domainId).push_back(key);
        }
    }

    AsyncContext.ThisTickEntry.BindsDK.emplace_back(std::move(update));
}

coro<> ServerSession::rP_AssetsBindHH(Net::AsyncSocket &sock) {
    UpdateAssetsBindsHH update;

    for(size_t typeIndex = 0; typeIndex < static_cast<size_t>(EnumAssets::MAX_ENUM); ++typeIndex) {
        uint32_t count = co_await sock.read<uint32_t>();
        if(count == 0)
            continue;

        for(size_t iter = 0; iter < count; ++iter) {
            uint32_t id = co_await sock.read<uint32_t>();
            ResourceFile::Hash_t hash;
            co_await sock.read((std::byte*) hash.data(), hash.size());
            std::string headerStr = co_await sock.read<std::string>();

            update.HashAndHeaders[typeIndex].emplace_back(id, hash, std::u8string((const char8_t*) headerStr.data(), headerStr.size()));
        }
    }

    AsyncContext.ThisTickEntry.BindsHH.emplace_back(std::move(update));
}

coro<> ServerSession::rP_AssetsInitSend(Net::AsyncSocket &sock) {
    uint32_t size = co_await sock.read<uint32_t>();
    Hash_t hash;
    co_await sock.read((std::byte*) hash.data(), hash.size());

    AsyncContext.AssetsLoading[hash] = AssetLoading{
        .Data = std::u8string(size, '\0'),
        .Offset = 0
    };
}

coro<> ServerSession::rP_AssetsNextSend(Net::AsyncSocket &sock) {
    Hash_t hash;
    co_await sock.read((std::byte*) hash.data(), hash.size());
    uint32_t size = co_await sock.read<uint32_t>();

    if(!AsyncContext.AssetsLoading.contains(hash)) {
        std::vector<std::byte> discard(size);
        co_await sock.read(discard.data(), size);
        co_return;
    }

    AssetLoading& al = AsyncContext.AssetsLoading.at(hash);
    if(al.Data.size() - al.Offset < size)
        MAKE_ERROR("Несоответствие ожидаемого размера ресурса");

    co_await sock.read((std::byte*) al.Data.data() + al.Offset, size);
    al.Offset += size;

    if(al.Offset != al.Data.size())
        co_return;

    AsyncContext.ThisTickEntry.ReceivedAssets.emplace_back(hash, std::move(al.Data));
    AsyncContext.AssetsLoading.erase(AsyncContext.AssetsLoading.find(hash));
}

coro<> ServerSession::rP_DefinitionsUpdate(Net::AsyncSocket &sock) {
    static std::atomic<uint32_t> debugDefLogCount = 0;
    uint32_t typeCount = co_await sock.read<uint32_t>();
    typeCount = std::min<uint32_t>(typeCount, static_cast<uint32_t>(EnumDefContent::MAX_ENUM));

    for(uint32_t type = 0; type < typeCount; ++type) {
        uint32_t count = co_await sock.read<uint32_t>();
        for(uint32_t i = 0; i < count; ++i) {
            ResourceId id = co_await sock.read<ResourceId>();
            std::string dataStr = co_await sock.read<std::string>();
            (void)dataStr;

            if(type == static_cast<uint32_t>(EnumDefContent::Node)) {
                DefNode_t def;
                def.NodestateId = 0;
                def.TexId = id;
                AsyncContext.ThisTickEntry.Profile_Node_AddOrChange.emplace_back(id, def);
                if(id < 32) {
                    uint32_t idx = debugDefLogCount.fetch_add(1);
                    if(idx < 64) {
                        LOG.debug() << "DefNode id=" << id
                            << " nodestate=" << def.NodestateId
                            << " tex=" << def.TexId;
                    }
                }
            }
        }
    }

    uint32_t lostCount = co_await sock.read<uint32_t>();
    lostCount = std::min<uint32_t>(lostCount, static_cast<uint32_t>(EnumDefContent::MAX_ENUM));
    for(uint32_t type = 0; type < lostCount; ++type) {
        uint32_t count = co_await sock.read<uint32_t>();
        for(uint32_t i = 0; i < count; ++i) {
            ResourceId id = co_await sock.read<ResourceId>();
            if(type == static_cast<uint32_t>(EnumDefContent::Node))
                AsyncContext.ThisTickEntry.Profile_Node_Lost.push_back(id);
        }
    }

    uint32_t dkCount = co_await sock.read<uint32_t>();
    dkCount = std::min<uint32_t>(dkCount, static_cast<uint32_t>(EnumDefContent::MAX_ENUM));
    for(uint32_t type = 0; type < dkCount; ++type) {
        uint32_t count = co_await sock.read<uint32_t>();
        for(uint32_t i = 0; i < count; ++i) {
            std::string key = co_await sock.read<std::string>();
            std::string domain = co_await sock.read<std::string>();
            (void)key;
            (void)domain;
        }
    }

    co_return;
}

coro<> ServerSession::rP_ChunkVoxels(Net::AsyncSocket &sock) {
    WorldId_t wcId = co_await sock.read<WorldId_t>();
    Pos::GlobalChunk pos;
    pos.unpack(co_await sock.read<Pos::GlobalChunk::Pack>());

    uint32_t compressedSize = co_await sock.read<uint32_t>();
    assert(compressedSize <= std::pow(2, 24));
    std::u8string compressed(compressedSize, '\0');
    co_await sock.read((std::byte*) compressed.data(), compressedSize);

    AsyncContext.ThisTickEntry.Chunks_AddOrChange_Voxel[wcId].insert({pos, std::move(compressed)});
    co_return;
}

coro<> ServerSession::rP_ChunkNodes(Net::AsyncSocket &sock) {
    WorldId_t wcId = co_await sock.read<WorldId_t>();
    Pos::GlobalChunk pos;
    pos.unpack(co_await sock.read<Pos::GlobalChunk::Pack>());

    uint32_t compressedSize = co_await sock.read<uint32_t>();
    assert(compressedSize <= std::pow(2, 24));
    std::u8string compressed(compressedSize, '\0');
    co_await sock.read((std::byte*) compressed.data(), compressedSize);

    AsyncContext.ThisTickEntry.Chunks_AddOrChange_Node[wcId].insert({pos, std::move(compressed)});
    co_return;
}

coro<> ServerSession::rP_ChunkLightPrism(Net::AsyncSocket &sock) {
    (void)sock;
    co_return;
}

coro<> ServerSession::rP_RemoveRegion(Net::AsyncSocket &sock) {
    WorldId_t wcId = co_await sock.read<WorldId_t>();
    Pos::GlobalRegion pos;
    pos.unpack(co_await sock.read<Pos::GlobalRegion::Pack>());

    AsyncContext.ThisTickEntry.Regions_Lost[wcId].push_back(pos);
    co_return;
}

coro<> ServerSession::rP_Tick(Net::AsyncSocket &sock) {
    (void)sock;
    AsyncContext.TickSequence.lock()->push_back(std::move(AsyncContext.ThisTickEntry));
    AsyncContext.ThisTickEntry = {};
    co_return;
}

coro<> ServerSession::rP_TestLinkCameraToEntity(Net::AsyncSocket &sock) {
    (void) sock;
    co_return;
}

coro<> ServerSession::rP_TestUnlinkCamera(Net::AsyncSocket &sock) {
    (void) sock;
    co_return;
}

}
