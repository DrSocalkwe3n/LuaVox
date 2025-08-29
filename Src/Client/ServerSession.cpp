#include "ServerSession.hpp"
#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "TOSAsync.hpp"
#include "TOSLib.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
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

ServerSession::ServerSession(asio::io_context &ioc, std::unique_ptr<Net::AsyncSocket>&& socket)
    : IAsyncDestructible(ioc), Socket(std::move(socket)), NetInputPackets(1024)
{
    assert(Socket.get());

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

struct PP_Definition_Voxel : public ParsedPacket {
    DefVoxelId Id;
    DefVoxel_t Def;

    PP_Definition_Voxel(DefVoxelId id, DefVoxel_t def)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::Voxel), 
            Id(id), Def(def)
    {}
};

struct PP_Definition_FreeVoxel : public ParsedPacket {
    DefVoxelId Id;

    PP_Definition_FreeVoxel(DefVoxelId id)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::FreeVoxel), 
            Id(id)
    {}
};

struct PP_Definition_Node : public ParsedPacket {
    DefNodeId Id;
    DefNode_t Def;

    PP_Definition_Node(DefNodeId id, DefNode_t def)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::Node), 
            Id(id), Def(def)
    {}
};

struct PP_Definition_FreeNode : public ParsedPacket {
    DefNodeId Id;

    PP_Definition_FreeNode(DefNodeId id)
        : ParsedPacket(ToClient::L1::Definition, (uint8_t) ToClient::L2Definition::FreeNode), 
            Id(id)
    {}
};

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
            resources.push_back(entry.Res);
            AsyncContext.ReceivedResources[entry.Type].push_back(entry);
            
            // // Проверяем используется ли сейчас ресурс
            // auto iter = Assets.ExistBinds[(int) entry.Type].find(entry.Id);
            // if(iter == Assets.ExistBinds[(int) entry.Type].end()) {
            //     // Не используется
            //     Assets.NotInUse[(int) entry.Type][entry.Domain + ':' + entry.Key] = {entry, TIME_BEFORE_UNLOAD_RESOURCE+time(nullptr)};
            // } else {
            //     // Используется
            //     Assets.InUse[(int) entry.Type][entry.Id] = entry;
            //     changedResources[entry.Type].insert({entry.Id, entry});
            // }
        }

        AM->pushResources(std::move(resources));
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
                    if(iterator != lost.end()) {
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
                    if(iterator != entry.Lost[(int) abe.Type].end()) {
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
            bool needQuery = false;
            // Проверить in memory кеш по домену+ключу
            {
                std::string dk = bind.Domain + ':' + bind.Key;
                auto &niubdk = Assets.NotInUse[(int) bind.Type];
                auto iter = niubdk.find(dk);
                if(iter != niubdk.end()) {
                    // Есть ресурс
                    needQuery = true;
                }
            }

            // Проверить если такой запрос уже был отправлен в AssetsManager и ожидает ответа
            if(!needQuery) {
                auto& list = AsyncContext.ResourceWait[(int) bind.Type];
                auto iterDomain = list.find(bind.Domain);
                if(iterDomain != list.end()) {
                    for(const auto& [key, hash] : iterDomain->second) {
                        if(key == bind.Key && hash == bind.Hash) {
                            needQuery = true;
                            break;
                        }
                    }
                }
            }

            // Assets.ExistBinds[(int) bind.Type].insert(bind.Id);

            // Под рукой нет ресурса, отправим на проверку в AssetsManager
            if(needQuery) {
                AsyncContext.ResourceWait[(int) bind.Type][bind.Domain].emplace_back(bind.Key, bind.Hash);
                needToLoad.emplace_back(bind.Hash, bind.Type, bind.Domain, bind.Key, bind.Id);
            }
        }

        // Отправляем запрос на получение ресурсов
        if(!needToLoad.empty())
            AM->pushReads(std::move(needToLoad));
    }

    if(!AsyncContext.TickSequence.get_read().empty()) {
        // Есть такты с сервера
        // Оповещаем о подготовке к обработке тактов
        if(RS)
            RS->prepareTickSync();

        IRenderSession::TickSyncData result;
        // Перевариваем данные по тактам




        if(RS)
            RS->pushStageTickSync();

        // Применяем изменения по ресурсам, профилям и контенту

        if(RS)
            RS->tickSync(result);
    }

    // Здесь нужно обработать управляющие пакеты







    // Оповещение модуля рендера об изменениях ресурсов
    std::unordered_map<EnumAssets, std::unordered_map<ResourceId, AssetEntry>> changedResources;
    std::unordered_map<EnumAssets, std::unordered_set<ResourceId>> lostResources;

    // Обработка полученных ресурсов
    

    // Обработка полученных тактов
    while(!AsyncContext.TickSequence.get_read().empty()) {
        TickData tick;
        
        {
            auto lock = AsyncContext.TickSequence.lock();
            tick = lock->front();
            lock->pop();
        }

        // Потерянные привязки ресурсов
        for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++) {
            for(ResourceId id : tick.AssetsLost[type]) {
                Assets.ExistBinds[type].erase(id);
                changedResources[(EnumAssets) type].erase(id);
            }
            // Assets.ExistBinds[type].erase(tick.AssetsLost[type].begin(), tick.AssetsLost[type].end());    
            lostResources[(EnumAssets) type].insert_range(tick.AssetsLost[type]);
        }

    }

    // Получаем ресурсы, загруженные с дискового кеша

    if(RS) {
        // Уведомляем рендер опотерянных и изменённых ресурсах
        if(!lostResources.empty()) {
            std::unordered_map<EnumAssets, std::vector<ResourceId>> lostResources2;

            for(auto& [type, list] : lostResources)
                lostResources2[type].append_range(list);
            
            lostResources.clear();
            RS->onAssetsLost(std::move(lostResources2));
        }

        if(!changedResources.empty()) {
            std::unordered_map<EnumAssets, std::vector<AssetEntry>> changedResources2;

            for(auto& [type, list] : changedResources) {
                auto& a = changedResources2[type];
                for(auto& [key, val] : list)
                    a.push_back(val);
            }

            changedResources.clear();
            RS->onAssetsChanges(std::move(changedResources2));
        }
    }

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
        std::unordered_map<EnumDefContent, std::vector<ResourceId>> onContentDefinesAdd;
        std::unordered_map<EnumDefContent, std::vector<ResourceId>> onContentDefinesLost;

        // Пакеты
        ParsedPacket *pack;
        while(NetInputPackets.pop(pack)) {
            if(pack->Level1 == ToClient::L1::Definition) {
                ToClient::L2Definition l2 = ToClient::L2Definition(pack->Level2);

                if(l2 == ToClient::L2Definition::Voxel) {
                    PP_Definition_Voxel &p = *dynamic_cast<PP_Definition_Voxel*>(pack);
                    Registry.DefVoxel[p.Id] = p.Def;
                    onContentDefinesAdd[EnumDefContent::Voxel].push_back(p.Id);
                } else if(l2 == ToClient::L2Definition::FreeVoxel) {
                    PP_Definition_FreeVoxel &p = *dynamic_cast<PP_Definition_FreeVoxel*>(pack);
                    {
                        auto iter = Registry.DefVoxel.find(p.Id);
                        if(iter != Registry.DefVoxel.end())
                            Registry.DefVoxel.erase(iter);
                    }
                    onContentDefinesLost[EnumDefContent::Voxel].push_back(p.Id);
                } else if(l2 == ToClient::L2Definition::Node) {
                    PP_Definition_Node &p = *dynamic_cast<PP_Definition_Node*>(pack);
                    Registry.DefNode[p.Id] = p.Def;
                    onContentDefinesAdd[EnumDefContent::Node].push_back(p.Id);
                } else if(l2 == ToClient::L2Definition::FreeNode) {
                    PP_Definition_FreeNode &p = *dynamic_cast<PP_Definition_FreeNode*>(pack);
                    {
                        auto iter = Registry.DefNode.find(p.Id);
                        if(iter != Registry.DefNode.end())
                            Registry.DefNode.erase(iter);
                    }
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
                    Pos::GlobalChunk pos = Pos::GlobalChunk(removed) << 2;
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

void ServerSession::setRenderSession(IRenderSession* session) {
    RS = session;
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
        AsyncContext.TickSequence.lock()->push(std::move(AsyncContext.ThisTickEntry));
        co_return;
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Resource(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToClient::L2Resource) second) {
    case ToClient::L2Resource::Bind:
    {
        uint32_t count = co_await sock.read<uint32_t>();
        AsyncContext.ThisTickEntry.AssetsBinds.reserve(AsyncContext.ThisTickEntry.AssetsBinds.size()+count);

        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();
            uint32_t id = co_await sock.read<uint32_t>();
            std::string domain, key;
            domain = co_await sock.read<std::string>();
            key = co_await sock.read<std::string>();
            Hash_t hash;
            co_await sock.read((std::byte*) hash.data(), hash.size());

            AsyncContext.ThisTickEntry.AssetsBinds.emplace_back(
                (EnumAssets) type, (ResourceId) id, std::move(domain),
                std::move(key), hash
            );
        }
    }
    case ToClient::L2Resource::Lost:
    {
        uint32_t count = co_await sock.read<uint32_t>();

        for(size_t iter = 0; iter < count; iter++) {
            uint8_t type = co_await sock.read<uint8_t>();
            uint32_t id = co_await sock.read<uint32_t>();

            AsyncContext.ThisTickEntry.AssetsLost[(int) type].push_back(id);
        }
    }
    case ToClient::L2Resource::InitResSend:
    {
        uint32_t size = co_await sock.read<uint32_t>();
        Hash_t hash;
        co_await sock.read((std::byte*) hash.data(), hash.size());
        ResourceId id = co_await sock.read<uint32_t>();
        EnumAssets type = (EnumAssets) co_await sock.read<uint8_t>();
        std::string domain = co_await sock.read<std::string>();
        std::string key = co_await sock.read<std::string>();

        AsyncContext.AssetsLoading[hash] = AssetLoading{
            type, id, std::move(domain), std::move(key), 
            std::u8string(size, '\0'), 0
        };

        co_return;
    }
    case ToClient::L2Resource::ChunkSend:
    {
        Hash_t hash;
        co_await sock.read((std::byte*) hash.data(), hash.size());
        uint32_t size = co_await sock.read<uint32_t>();
        AssetLoading& al = AsyncContext.AssetsLoading.at(hash);
        if(al.Data.size()-al.Offset < size)
            MAKE_ERROR("Несоответствие ожидаемого размера ресурса");

        co_await sock.read((std::byte*) al.Data.data() + al.Offset, size);
        al.Offset += size;

        if(al.Offset == al.Data.size()) {
            // Ресурс полностью загружен
            AsyncContext.LoadedAssets.lock()->emplace_back(
                al.Type, al.Id, std::move(al.Domain), std::move(al.Key), std::move(al.Data)
            );

            AsyncContext.AssetsLoading.erase(AsyncContext.AssetsLoading.find(hash));
        }
    
        co_return;
    }
    default:
        protocolError();
    }
}

coro<> ServerSession::rP_Definition(Net::AsyncSocket &sock) {
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
        // DefNodeId id;
        // DefNode_t def;
        // id = co_await sock.read<DefNodeId>();
        // def.DrawType = (DefNode_t::EnumDrawType) co_await sock.read<uint8_t>();
        // for(int iter = 0; iter < 6; iter++) {
        //     auto &pl = def.Texs[iter].Pipeline;
        //     pl.resize(co_await sock.read<uint16_t>());
        //     co_await sock.read((std::byte*) pl.data(), pl.size());
        // }

        // PP_Definition_Node *packet = new PP_Definition_Node(
        //     id,
        //     def
        // );

        // while(!NetInputPackets.push(packet));

        // co_return;
    }
    case ToClient::L2Definition::FreeNode:
    {
        DefNodeId id = co_await sock.read<DefNodeId>();
    
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