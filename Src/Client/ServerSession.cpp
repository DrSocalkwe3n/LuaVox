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

std::optional<DefNodeId> debugExpectedGeneratedNodeId(int rx, int ry, int rz) {
    if(ry == 1 && rz == 0)
        return DefNodeId(0);
    if(rx == 0 && ry == 1)
        return DefNodeId(0);
    if(rx == 0 && rz == 0)
        return DefNodeId(1);
    if(ry == 0 && rz == 0)
        return DefNodeId(2);
    if(rx == 0 && ry == 0)
        return DefNodeId(3);
    return std::nullopt;
}

void debugCheckGeneratedChunkNodes(WorldId_t worldId,
    Pos::GlobalChunk chunkPos,
    const std::array<Node, 16 * 16 * 16>& chunk)
{
    if(chunkPos[0] != 0 && chunkPos[1] != 0 && chunkPos[2] != 0)
        return;

    static std::atomic<uint32_t> warnCount = 0;
    if(warnCount.load() >= 16)
        return;

    Pos::bvec4u localChunk = chunkPos & 0x3;
    const int baseX = int(localChunk[0]) * 16;
    const int baseY = int(localChunk[1]) * 16;
    const int baseZ = int(localChunk[2]) * 16;
    const int globalBaseX = int(chunkPos[0]) * 16;
    const int globalBaseY = int(chunkPos[1]) * 16;
    const int globalBaseZ = int(chunkPos[2]) * 16;

    for(int z = 0; z < 16; z++)
        for(int y = 0; y < 16; y++)
            for(int x = 0; x < 16; x++) {
                int rx = baseX + x;
                int ry = baseY + y;
                int rz = baseZ + z;
                int gx = globalBaseX + x;
                int gy = globalBaseY + y;
                int gz = globalBaseZ + z;
                std::optional<DefNodeId> expected = debugExpectedGeneratedNodeId(rx, ry, rz);
                if(!expected)
                    continue;

                const Node& node = chunk[x + y * 16 + z * 16 * 16];
                if(node.NodeId != *expected) {
                    uint32_t index = warnCount.fetch_add(1);
                    if(index < 16) {
                        TOS::Logger("Client>WorldDebug").warn()
                            << "Generated node mismatch world " << worldId
                            << " chunk " << int(chunkPos[0]) << ',' << int(chunkPos[1]) << ',' << int(chunkPos[2])
                            << " at local " << rx << ',' << ry << ',' << rz
                            << " global " << gx << ',' << gy << ',' << gz
                            << " expected " << *expected
                            << " got " << node.NodeId;
                    }
                    return;
                }
            }
}

}

ServerSession::ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket>&& socket)
    : IAsyncDestructible(ioc), Socket(std::move(socket)) //, NetInputPackets(1024)
{
    assert(Socket.get());

    Profiles.DefNode[0] = {0};
    Profiles.DefNode[1] = {1};
    Profiles.DefNode[2] = {2};
    Profiles.DefNode[3] = {3};
    Profiles.DefNode[4] = {4};

    std::fill(NextServerId.begin(), NextServerId.end(), 1);
    for(auto& vec : ServerIdToDK)
        vec.emplace_back();

    try {
        AM = AssetsManager::Create(ioc, "Cache");
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
    // Если были получены ресурсы, отправим их на запись в кеш
    if(!AsyncContext.LoadedAssets.get_read().empty()) {
        std::vector<AssetEntry> assets = std::move(*AsyncContext.LoadedAssets.lock());
        std::vector<Resource> resources;
        resources.reserve(assets.size());

        for(AssetEntry& entry : assets) {
            entry.Hash = entry.Res.hash();
            if(const AssetsManager::BindInfo* bind = AM->getBind(entry.Type, entry.Id))
                entry.Dependencies = AM->rebindHeader(bind->Header);
            else
                entry.Dependencies.clear();

            resources.push_back(entry.Res);
            AsyncContext.LoadedResources.emplace_back(std::move(entry));
            
            // // Проверяем используется ли сейчас ресурс
            // auto iter = MyAssets.ExistBinds[(int) entry.Type].find(entry.Id);
            // if(iter == MyAssets.ExistBinds[(int) entry.Type].end()) {
            //     // Не используется
            //     MyAssets.NotInUse[(int) entry.Type][entry.Domain + ':' + entry.Key] = {entry, TIME_BEFORE_UNLOAD_RESOURCE+time(nullptr)};
            // } else {
            //     // Используется
            //     Assets.InUse[(int) entry.Type][entry.Id] = entry;
            //     changedResources[entry.Type].insert({entry.Id, entry});
            // }
        }

        AM->pushResources(std::move(resources));
    }

    // Получить ресурсы с AssetsManager
    {
        static std::atomic<uint32_t> debugAssetReadLogCount = 0;
        std::vector<std::pair<AssetsManager::ResourceKey, std::optional<Resource>>> resources = AM->pullReads();
        std::vector<Hash_t> needRequest;

        for(auto& [key, res] : resources) {
            {
                auto& waitingByDomain = AsyncContext.ResourceWait[(int) key.Type];
                auto iterDomain = waitingByDomain.find(key.Domain);
                if(iterDomain != waitingByDomain.end()) {
                    auto& entries = iterDomain->second;
                    entries.erase(std::remove_if(entries.begin(), entries.end(),
                        [&](const std::pair<std::string, Hash_t>& entry) {
                            return entry.first == key.Key && entry.second == key.Hash;
                        }),
                        entries.end());
                    if(entries.empty())
                        waitingByDomain.erase(iterDomain);
                }
            }

            if(key.Domain == "test"
                && (key.Type == EnumAssets::Nodestate
                    || key.Type == EnumAssets::Model
                    || key.Type == EnumAssets::Texture))
            {
                uint32_t idx = debugAssetReadLogCount.fetch_add(1);
                if(idx < 128) {
                    if(res) {
                        LOG.debug() << "Cache hit type=" << assetTypeName(key.Type)
                            << " id=" << key.Id
                            << " key=" << key.Domain << ':' << key.Key
                            << " size=" << res->size();
                    } else {
                        LOG.debug() << "Cache miss type=" << assetTypeName(key.Type)
                            << " id=" << key.Id
                            << " key=" << key.Domain << ':' << key.Key
                            << " hash=" << int(key.Hash[0]) << '.'
                            << int(key.Hash[1]) << '.'
                            << int(key.Hash[2]) << '.'
                            << int(key.Hash[3]);
                    }
                }
            }

            if(!res) {
                // Проверить не был ли уже отправлен запрос на получение этого хеша
                auto iter = std::lower_bound(AsyncContext.AlreadyLoading.begin(), AsyncContext.AlreadyLoading.end(), key.Hash);
                if(iter == AsyncContext.AlreadyLoading.end() || *iter != key.Hash) {
                    AsyncContext.AlreadyLoading.insert(iter, key.Hash);
                    needRequest.push_back(key.Hash);
                }
            } else {
                Hash_t actualHash = res->hash();
                if(actualHash != key.Hash) {
                    auto iter = std::lower_bound(AsyncContext.AlreadyLoading.begin(), AsyncContext.AlreadyLoading.end(), key.Hash);
                    if(iter == AsyncContext.AlreadyLoading.end() || *iter != key.Hash) {
                        AsyncContext.AlreadyLoading.insert(iter, key.Hash);
                        needRequest.push_back(key.Hash);
                    }
                }

                std::vector<uint8_t> deps;
                if(const AssetsManager::BindInfo* bind = AM->getBind(key.Type, key.Id))
                    deps = AM->rebindHeader(bind->Header);

                AssetEntry entry {
                    .Type = key.Type,
                    .Id = key.Id,
                    .Domain = key.Domain,
                    .Key = key.Key,
                    .Res = std::move(*res),
                    .Hash = actualHash,
                    .Dependencies = std::move(deps)
                };

                AsyncContext.LoadedResources.emplace_back(std::move(entry));
            }
        }

        if(!needRequest.empty()) {
            assert(needRequest.size() < (1 << 16));

            uint32_t idx = debugAssetReadLogCount.fetch_add(1);
            if(idx < 128) {
                LOG.debug() << "Send ResourceRequest count=" << needRequest.size();
            }

            Net::Packet p;
            p << (uint8_t) ToServer::L1::System << (uint8_t) ToServer::L2System::ResourceRequest;
            p << (uint16_t) needRequest.size();
            for(const Hash_t& hash : needRequest)
                p.write((const std::byte*) hash.data(), 32);

            Socket->pushPacket(std::move(p));
        }
    }

    // Разбираемся с полученными меж тактами привязками ресурсов
    if(!AsyncContext.AssetsBinds.get_read().empty()) {
        AssetsBindsChange abc;

        // Нужно объеденить изменения в один AssetsBindsChange (abc)
        {
            std::vector<AssetsBindsChange> list = std::move(*AsyncContext.AssetsBinds.lock());

            for(AssetsBindsChange entry : list) {
                for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++)
                    std::sort(entry.Lost[type].begin(), entry.Lost[type].end());
                
                // Если до этого была объявлена привязка, а теперь она потеряна, то просто сокращаем значения.
                // Иначе дописываем в lost
                for(ssize_t iter = abc.Binds.size()-1; iter >= 0; iter--) {
                    const AssetBindEntry& abe = abc.Binds[iter];
                    auto& lost = entry.Lost[(int) abe.Type];
                    auto iterator = std::lower_bound(lost.begin(), lost.end(), abe.Id);
                    if(iterator != lost.end() && *iterator == abe.Id) {
                        // Привязка будет удалена
                        lost.erase(iterator);
                        abc.Binds.erase(abc.Binds.begin()+iter);
                    }
                }

                for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                    abc.Lost[type].append_range(entry.Lost[type]);
                    entry.Lost[type].clear();
                    std::sort(abc.Lost[type].begin(), abc.Lost[type].end());
                }

                for(AssetBindEntry& abe : entry.Binds) {
                    auto iterator = std::lower_bound(entry.Lost[(int) abe.Type].begin(), entry.Lost[(int) abe.Type].end(), abe.Id);
                    if(iterator != entry.Lost[(int) abe.Type].end() && *iterator == abe.Id) {
                        // Получили новую привязку, которая была удалена в предыдущем такте
                        entry.Lost[(int) abe.Type].erase(iterator);
                    } else {
                        // Данная привязка не удалялась, может она была изменена?
                        bool hasChanged = false;
                        for(AssetBindEntry& abe2 : abc.Binds) {
                            if(abe2.Type == abe.Type && abe2.Id == abe.Id) {
                                // Привязка была изменена
                                abe2 = std::move(abe);
                                hasChanged = true;
                                break;
                            }
                        }

                        if(!hasChanged)
                            // Изменения не было, это просто новая привязка
                            abc.Binds.emplace_back(std::move(abe));
                    }
                }

                entry.Binds.clear();
            }
        }

        // Запрос к дисковому кешу новых ресурсов
        std::vector<AssetsManager::ResourceKey> needToLoad;
        for(const AssetBindEntry& bind : abc.Binds) {
            bool needQuery = true;
            // Проверить in memory кеш по домену+ключу
            {
                std::string dk = bind.Domain + ':' + bind.Key;
                auto &niubdk = MyAssets.NotInUse[(int) bind.Type];
                auto iter = niubdk.find(dk);
                if(iter != niubdk.end()) {
                    // Есть ресурс
                    needQuery = iter->second.first.Hash != bind.Hash;
                }
            }

            // Проверить если такой запрос уже был отправлен в AssetsManager и ожидает ответа
            if(needQuery) {
                auto& list = AsyncContext.ResourceWait[(int) bind.Type];
                auto iterDomain = list.find(bind.Domain);
                if(iterDomain != list.end()) {
                    for(const auto& [key, hash] : iterDomain->second) {
                        if(key == bind.Key && hash == bind.Hash) {
                            needQuery = false;
                            break;
                        }
                    }
                }
            }

            // Под рукой нет ресурса, отправим на проверку в AssetsManager
            if(needQuery) {
                AsyncContext.ResourceWait[(int) bind.Type][bind.Domain].emplace_back(bind.Key, bind.Hash);
                needToLoad.emplace_back(bind.Hash, bind.Type, bind.Domain, bind.Key, bind.Id);
            }
        }

        // Отправляем запрос на получение ресурсов
        if(!needToLoad.empty()) {
            static std::atomic<uint32_t> debugReadRequestLogCount = 0;
            AssetsManager::ResourceKey firstDebug;
            bool hasDebug = false;
            for(const auto& entry : needToLoad) {
                if(entry.Domain == "test"
                    && (entry.Type == EnumAssets::Nodestate
                        || entry.Type == EnumAssets::Model
                        || entry.Type == EnumAssets::Texture))
                {
                    firstDebug = entry;
                    hasDebug = true;
                    break;
                }
            }
            if(hasDebug && debugReadRequestLogCount.fetch_add(1) < 64) {
                LOG.debug() << "Queue asset read count=" << needToLoad.size()
                    << " type=" << assetTypeName(firstDebug.Type)
                    << " id=" << firstDebug.Id
                    << " key=" << firstDebug.Domain << ':' << firstDebug.Key
                    << " hash=" << int(firstDebug.Hash[0]) << '.'
                    << int(firstDebug.Hash[1]) << '.'
                    << int(firstDebug.Hash[2]) << '.'
                    << int(firstDebug.Hash[3]);
            }
            AM->pushReads(std::move(needToLoad));
        }

        AsyncContext.Binds.push_back(std::move(abc));
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
                    debugCheckGeneratedChunkNodes(wId, pos, chunkNodes);
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


        {
            for(TickData& data : ticks) {
            }
        }

        if(RS)
            RS->pushStageTickSync();

        // Применяем изменения по ресурсам, профилям и контенту
        // Разбираемся с изменениями в привязках ресурсов
        {
            AssetsBindsChange abc;

            for(AssetsBindsChange entry : AsyncContext.Binds) {
                for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++)
                    std::sort(entry.Lost[type].begin(), entry.Lost[type].end());
                
                for(ssize_t iter = abc.Binds.size()-1; iter >= 0; iter--) {
                    const AssetBindEntry& abe = abc.Binds[iter];
                    auto& lost = entry.Lost[(int) abe.Type];
                    auto iterator = std::lower_bound(lost.begin(), lost.end(), abe.Id);
                    if(iterator != lost.end() && *iterator == abe.Id) {
                        lost.erase(iterator);
                        abc.Binds.erase(abc.Binds.begin()+iter);
                    }
                }

                for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                    abc.Lost[type].append_range(entry.Lost[type]);
                    entry.Lost[type].clear();
                    std::sort(abc.Lost[type].begin(), abc.Lost[type].end());
                }

                for(AssetBindEntry& abe : entry.Binds) {
                    auto iterator = std::lower_bound(entry.Lost[(int) abe.Type].begin(), entry.Lost[(int) abe.Type].end(), abe.Id);
                    if(iterator != entry.Lost[(int) abe.Type].end() && *iterator == abe.Id) {
                        // Получили новую привязку, которая была удалена в предыдущем такте
                        entry.Lost[(int) abe.Type].erase(iterator);
                    } else {
                        // Данная привязка не удалялась, может она была изменена?
                        bool hasChanged = false;
                        for(AssetBindEntry& abe2 : abc.Binds) {
                            if(abe2.Type == abe.Type && abe2.Id == abe.Id) {
                                // Привязка была изменена
                                abe2 = std::move(abe);
                                hasChanged = true;
                                break;
                            }
                        }

                        if(!hasChanged)
                            // Изменения не было, это просто новая привязка
                            abc.Binds.emplace_back(std::move(abe));
                    }
                }

                entry.Binds.clear();
            }
            
            AsyncContext.Binds.clear();

            for(AssetBindEntry& entry : abc.Binds) {
                std::vector<uint8_t> deps;
                if(!entry.Header.empty())
                    deps = AM->rebindHeader(entry.Header);

                MyAssets.ExistBinds[(int) entry.Type].insert(entry.Id);
                result.Assets_ChangeOrAdd[entry.Type].push_back(entry.Id);

                auto iterLoaded = IServerSession::Assets[entry.Type].find(entry.Id);
                if(iterLoaded != IServerSession::Assets[entry.Type].end())
                    iterLoaded->second.Dependencies = deps;

                // Если ресурс был в кеше, то достаётся от туда
                auto iter = MyAssets.NotInUse[(int) entry.Type].find(entry.Domain+':'+entry.Key);
                if(iter != MyAssets.NotInUse[(int) entry.Type].end()) {
                    iter->second.first.Dependencies = deps;
                    IServerSession::Assets[entry.Type][entry.Id] = std::get<0>(iter->second);
                    result.Assets_ChangeOrAdd[entry.Type].push_back(entry.Id);
                    MyAssets.NotInUse[(int) entry.Type].erase(iter);
                }
            }

            for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                for(ResourceId id : abc.Lost[type]) {
                    MyAssets.ExistBinds[type].erase(id);

                    // Потерянные ресурсы уходят в кеш
                    auto iter = IServerSession::Assets[(EnumAssets) type].find(id);
                    if(iter != IServerSession::Assets[(EnumAssets) type].end()) {
                        MyAssets.NotInUse[(int) iter->second.Type][iter->second.Domain+':'+iter->second.Key] = {iter->second, TIME_BEFORE_UNLOAD_RESOURCE+time(nullptr)};
                        IServerSession::Assets[(EnumAssets) type].erase(iter);
                        result.Assets_Lost[iter->second.Type].push_back(iter->second.Id);
                    }
                }

                result.Assets_Lost[(EnumAssets) type] = std::move(abc.Lost[type]);
            }
        }

        // Получаем ресурсы
        {
            for(AssetEntry& entry : AsyncContext.LoadedResources) {
                if(MyAssets.ExistBinds[(int) entry.Type].contains(entry.Id)) {
                    // Ресурс ещё нужен
                    IServerSession::Assets[entry.Type][entry.Id] = entry;
                    result.Assets_ChangeOrAdd[entry.Type].push_back(entry.Id);
                } else {
                    // Ресурс уже не нужен, отправляем в кеш
                    MyAssets.NotInUse[(int) entry.Type][entry.Domain+':'+entry.Key] = {entry, TIME_BEFORE_UNLOAD_RESOURCE+time(nullptr)};
                }
            }
            
            AsyncContext.LoadedResources.clear();
        }

        // Чистим кеш ресурсов
        {
            uint64_t now = time(nullptr);
            for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
                std::vector<std::string> toDelete;
                for(auto& [key, value] : MyAssets.NotInUse[type]) {
                    if(std::get<1>(value) < now)
                        toDelete.push_back(key);
                }

                for(std::string& key : toDelete)
                    MyAssets.NotInUse[type].erase(MyAssets.NotInUse[type].find(key));
            }
        }

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

    // Здесь нужно обработать управляющие пакеты







    // Оповещение модуля рендера об изменениях ресурсов
    std::unordered_map<EnumAssets, std::unordered_map<ResourceId, AssetEntry>> changedResources;
    std::unordered_map<EnumAssets, std::unordered_set<ResourceId>> lostResources;

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
    AM->clearServerBindings();
    AsyncContext.AssetsLoading.clear();
    AsyncContext.AlreadyLoading.clear();
    for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++)
        AsyncContext.ResourceWait[type].clear();
    for(auto& vec : ServerIdToDK)
        vec.clear();
    std::fill(NextServerId.begin(), NextServerId.end(), 1);
    for(auto& vec : ServerIdToDK)
        vec.emplace_back();
    AsyncContext.Binds.clear();
    AsyncContext.LoadedResources.clear();
    AsyncContext.ThisTickEntry = {};
    AsyncContext.LoadedAssets.lock()->clear();
    AsyncContext.AssetsBinds.lock()->clear();
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
    case ToClient::L2System::SyncTick:
        AsyncContext.TickSequence.lock()->push_back(std::move(AsyncContext.ThisTickEntry));
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Resource(Net::AsyncSocket &sock) {
    static std::atomic<uint32_t> debugResourceLogCount = 0;
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Resource) second) {
    case ToClient::L2Resource::Bind:
    {
        uint32_t count = co_await sock.read<uint32_t>();
        std::vector<AssetBindEntry> binds;
        binds.reserve(count);

        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();

            if(type >= (int) EnumAssets::MAX_ENUM)
                protocolError();

            uint32_t id = co_await sock.read<uint32_t>();
            std::string domain, key;
            domain = co_await sock.read<std::string>();
            key = co_await sock.read<std::string>();
            Hash_t hash;
            co_await sock.read((std::byte*) hash.data(), hash.size());
            uint32_t headerSize = co_await sock.read<uint32_t>();
            std::vector<uint8_t> header;
            if(headerSize > 0) {
                header.resize(headerSize);
                co_await sock.read((std::byte*) header.data(), header.size());
            }

            AssetsManager::BindResult bindResult = AM->bindServerResource(
                (EnumAssets) type, (ResourceId) id, domain, key, hash, header);

            if(!bindResult.Changed)
                continue;

            binds.emplace_back(AssetBindEntry{
                .Type = (EnumAssets) type,
                .Id = bindResult.LocalId,
                .Domain = std::move(domain),
                .Key = std::move(key),
                .Hash = hash,
                .Header = std::move(header)
            });

            if(binds.back().Domain == "test"
                && (binds.back().Type == EnumAssets::Nodestate
                    || binds.back().Type == EnumAssets::Model
                    || binds.back().Type == EnumAssets::Texture))
            {
                uint32_t idx = debugResourceLogCount.fetch_add(1);
                if(idx < 128) {
                    LOG.debug() << "Bind asset type=" << assetTypeName(binds.back().Type)
                        << " id=" << binds.back().Id
                        << " key=" << binds.back().Domain << ':' << binds.back().Key
                        << " hash=" << int(binds.back().Hash[0]) << '.'
                        << int(binds.back().Hash[1]) << '.'
                        << int(binds.back().Hash[2]) << '.'
                        << int(binds.back().Hash[3]);
                }
            }
        }

        AsyncContext.AssetsBinds.lock()->push_back(AssetsBindsChange(binds, {}));
        co_return;
    }
    case ToClient::L2Resource::BindDK:
    {
        uint32_t count = co_await sock.read<uint32_t>();
        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();
            if(type >= (int) EnumAssets::MAX_ENUM)
                protocolError();

            std::string domain = co_await sock.read<std::string>();
            std::string key = co_await sock.read<std::string>();

            ResourceId serverId = NextServerId[type]++;
            auto& table = ServerIdToDK[type];
            if(table.size() <= serverId)
                table.resize(serverId+1);
            table[serverId] = {std::move(domain), std::move(key)};
        }
        co_return;
    }
    case ToClient::L2Resource::BindHash:
    {
        uint32_t count = co_await sock.read<uint32_t>();
        std::vector<AssetBindEntry> binds;
        binds.reserve(count);

        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();
            if(type >= (int) EnumAssets::MAX_ENUM)
                protocolError();

            uint32_t id = co_await sock.read<uint32_t>();
            Hash_t hash;
            co_await sock.read((std::byte*) hash.data(), hash.size());
            uint32_t headerSize = co_await sock.read<uint32_t>();
            std::vector<uint8_t> header;
            if(headerSize > 0) {
                header.resize(headerSize);
                co_await sock.read((std::byte*) header.data(), header.size());
            }

            auto& table = ServerIdToDK[type];
            if(id >= table.size()) {
                LOG.warn() << "BindHash without domain/key for id=" << id;
                continue;
            }

            const auto& [domain, key] = table[id];
            if(domain.empty() && key.empty()) {
                LOG.warn() << "BindHash missing domain/key for id=" << id;
                continue;
            }

            AssetsManager::BindResult bindResult = AM->bindServerResource(
                (EnumAssets) type, (ResourceId) id, domain, key, hash, header);

            if(!bindResult.Changed)
                continue;

            binds.emplace_back(AssetBindEntry{
                .Type = (EnumAssets) type,
                .Id = bindResult.LocalId,
                .Domain = domain,
                .Key = key,
                .Hash = hash,
                .Header = std::move(header)
            });

            if(binds.back().Domain == "test"
                && (binds.back().Type == EnumAssets::Nodestate
                    || binds.back().Type == EnumAssets::Model
                    || binds.back().Type == EnumAssets::Texture))
            {
                uint32_t idx = debugResourceLogCount.fetch_add(1);
                if(idx < 128) {
                    LOG.debug() << "Bind asset type=" << assetTypeName(binds.back().Type)
                        << " id=" << binds.back().Id
                        << " key=" << binds.back().Domain << ':' << binds.back().Key
                        << " hash=" << int(binds.back().Hash[0]) << '.'
                        << int(binds.back().Hash[1]) << '.'
                        << int(binds.back().Hash[2]) << '.'
                        << int(binds.back().Hash[3]);
                }
            }
        }

        if(!binds.empty())
            AsyncContext.AssetsBinds.lock()->push_back(AssetsBindsChange(binds, {}));
        co_return;
    }
    case ToClient::L2Resource::Lost:
    {
        uint32_t count = co_await sock.read<uint32_t>();
        AssetsBindsChange abc;

        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();
            uint32_t id = co_await sock.read<uint32_t>();

            if(type >= (int) EnumAssets::MAX_ENUM)
                protocolError();

            auto localId = AM->unbindServerResource((EnumAssets) type, id);
            if(!localId)
                continue;

            abc.Lost[(int) type].push_back(*localId);
        }

        AsyncContext.AssetsBinds.lock()->emplace_back(std::move(abc));
        co_return;
    }
    case ToClient::L2Resource::InitResSend:
    {
        uint32_t size = co_await sock.read<uint32_t>();
        Hash_t hash;
        co_await sock.read((std::byte*) hash.data(), hash.size());
        ResourceId id = co_await sock.read<uint32_t>();
        EnumAssets type = (EnumAssets) co_await sock.read<uint8_t>();

        if(type >= EnumAssets::MAX_ENUM)
            protocolError();

        std::string domain = co_await sock.read<std::string>();
        std::string key = co_await sock.read<std::string>();
        ResourceId localId = 0;
        if(auto mapped = AM->getLocalIdFromServer(type, id)) {
            localId = *mapped;
        } else {
            localId = AM->getId(type, domain, key);
            AM->bindServerResource(type, id, domain, key, hash, {});
        }

        if(domain == "test"
            && (type == EnumAssets::Nodestate
                || type == EnumAssets::Model
                || type == EnumAssets::Texture))
        {
            uint32_t idx = debugResourceLogCount.fetch_add(1);
            if(idx < 128) {
                LOG.debug() << "InitResSend type=" << assetTypeName(type)
                    << " id=" << localId
                    << " key=" << domain << ':' << key
                    << " size=" << size;
            }
        }

        AsyncContext.AssetsLoading[hash] = AssetLoading{
            type, localId, std::move(domain), std::move(key),
            std::u8string(size, '\0'), 0
        };

        co_return;
    }
    case ToClient::L2Resource::ChunkSend:
    {
        Hash_t hash;
        co_await sock.read((std::byte*) hash.data(), hash.size());
        try {
            uint32_t size = co_await sock.read<uint32_t>();
            assert(AsyncContext.AssetsLoading.contains(hash));
            AssetLoading& al = AsyncContext.AssetsLoading.at(hash);
            if(al.Data.size()-al.Offset < size)
                MAKE_ERROR("Несоответствие ожидаемого размера ресурса");

            co_await sock.read((std::byte*) al.Data.data() + al.Offset, size);
            al.Offset += size;

            if(al.Offset == al.Data.size()) {
                // Ресурс полностью загружен
                if(al.Domain == "test"
                    && (al.Type == EnumAssets::Nodestate
                        || al.Type == EnumAssets::Model
                        || al.Type == EnumAssets::Texture))
                {
                    uint32_t idx = debugResourceLogCount.fetch_add(1);
                    if(idx < 128) {
                        LOG.debug() << "Resource loaded type=" << assetTypeName(al.Type)
                            << " id=" << al.Id
                            << " key=" << al.Domain << ':' << al.Key
                            << " size=" << al.Data.size();
                    }
                }

                AsyncContext.LoadedAssets.lock()->emplace_back(AssetEntry{
                    .Type = al.Type,
                    .Id = al.Id,
                    .Domain = std::move(al.Domain),
                    .Key = std::move(al.Key),
                    .Res = std::move(al.Data),
                    .Hash = hash
                });

                AsyncContext.AssetsLoading.erase(AsyncContext.AssetsLoading.find(hash));

                auto iter = std::lower_bound(AsyncContext.AlreadyLoading.begin(), AsyncContext.AlreadyLoading.end(), hash);
                if(iter != AsyncContext.AlreadyLoading.end() && *iter == hash)
                    AsyncContext.AlreadyLoading.erase(iter);
            }
        } catch(const std::exception& exc) {
            std::string err = exc.what();
            int g = 0;
        }
    
        co_return;
    }
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Definition(Net::AsyncSocket &sock) {
    static std::atomic<uint32_t> debugDefLogCount = 0;
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Definition) second) {
    case ToClient::L2Definition::World: {
        DefWorldId cdId = co_await sock.read<DefWorldId>();

        co_return;
    }
    case ToClient::L2Definition::FreeWorld: {
        DefWorldId cdId = co_await sock.read<DefWorldId>();

        co_return;
    }
    case ToClient::L2Definition::Voxel: {
        DefVoxelId cdId = co_await sock.read<DefVoxelId>();

        co_return;
    }
    case ToClient::L2Definition::FreeVoxel: {
        DefVoxelId cdId = co_await sock.read<DefVoxelId>();
    
        co_return;
    }
    case ToClient::L2Definition::Node:
    {
        DefNode_t def;
        DefNodeId id = co_await sock.read<DefNodeId>();
        ResourceId serverNodestate = co_await sock.read<uint32_t>();
        if(auto localId = AM->getLocalIdFromServer(EnumAssets::Nodestate, serverNodestate))
            def.NodestateId = *localId;
        else
            def.NodestateId = 0;
        def.TexId = id;

        if(id < 32) {
            uint32_t idx = debugDefLogCount.fetch_add(1);
            if(idx < 64) {
                LOG.debug() << "DefNode id=" << id
                    << " nodestate=" << def.NodestateId
                    << " tex=" << def.TexId;
            }
        }

        AsyncContext.ThisTickEntry.Profile_Node_AddOrChange.emplace_back(id, def);

        co_return;
    }
    case ToClient::L2Definition::FreeNode:
    {
        DefNodeId id = co_await sock.read<DefNodeId>();
        AsyncContext.ThisTickEntry.Profile_Node_Lost.push_back(id);

        co_return;
    }
    case ToClient::L2Definition::Portal:

        co_return;
    case ToClient::L2Definition::FreePortal:
    
        co_return;
    case ToClient::L2Definition::Entity:
    {
        DefEntityId id = co_await sock.read<DefEntityId>();
        DefEntityInfo def;
        AsyncContext.ThisTickEntry.Profile_Entity_AddOrChange.emplace_back(id, def);
        co_return;
    }
    case ToClient::L2Definition::FreeEntity:
    {
        DefEntityId id = co_await sock.read<DefEntityId>();
        AsyncContext.ThisTickEntry.Profile_Entity_Lost.push_back(id);
        co_return;
    }
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Content(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Content) second) {
    case ToClient::L2Content::World: {
        WorldId_t wId = co_await sock.read<uint32_t>();
        AsyncContext.ThisTickEntry.Worlds_AddOrChange.emplace_back(wId, nullptr);
        co_return;
    }
    case ToClient::L2Content::RemoveWorld: {
        WorldId_t wId = co_await sock.read<uint32_t>();
        AsyncContext.ThisTickEntry.Worlds_Lost.push_back(wId);
        co_return;
    }
    case ToClient::L2Content::Portal:

        co_return;
    case ToClient::L2Content::RemovePortal:
    
        co_return;
    case ToClient::L2Content::Entity:
    {
        EntityId_t id = co_await sock.read<EntityId_t>();
        DefEntityId defId = co_await sock.read<DefEntityId>();
        WorldId_t worldId = co_await sock.read<WorldId_t>();

        Pos::Object pos;
        pos.x = co_await sock.read<decltype(pos.x)>();
        pos.y = co_await sock.read<decltype(pos.y)>();
        pos.z = co_await sock.read<decltype(pos.z)>();

        ToServer::PacketQuat q;
        for(int iter = 0; iter < 5; iter++)
            q.Data[iter] = co_await sock.read<uint8_t>();

        EntityInfo info;
        info.DefId = defId;
        info.WorldId = worldId;
        info.Pos = pos;
        info.Quat = q.toQuat();

        AsyncContext.ThisTickEntry.Entity_AddOrChange.emplace_back(id, info);
        co_return;
    }
    case ToClient::L2Content::RemoveEntity:
    {
        EntityId_t id = co_await sock.read<EntityId_t>();
        AsyncContext.ThisTickEntry.Entity_Lost.push_back(id);
        co_return;
    }
    case ToClient::L2Content::ChunkVoxels:
    {
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

    case ToClient::L2Content::ChunkNodes:
    {
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
    case ToClient::L2Content::ChunkLightPrism:

        co_return;
    case ToClient::L2Content::RemoveRegion: {
        WorldId_t wcId = co_await sock.read<WorldId_t>();
        Pos::GlobalRegion pos;
        pos.unpack(co_await sock.read<Pos::GlobalRegion::Pack>());

        AsyncContext.ThisTickEntry.Regions_Lost[wcId].push_back(pos);

        co_return;
    }
    default:
        protocolError();
    }
}

}
