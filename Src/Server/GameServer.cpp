#include "GameServer.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include <boost/json/parse.hpp>
#include <chrono>
#include <glm/geometric.hpp>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "SaveBackends/Filesystem.hpp"
#include "Server/SaveBackend.hpp"
#include "Server/World.hpp"

namespace LV::Server {

GameServer::~GameServer() {
    shutdown("on ~GameServer");
    RunThread.join();
    WorkDeadline.cancel();
    UseLock.wait_no_use();
    LOG.info() << "Сервер уничтожен";
}

static thread_local std::vector<ContentViewCircle> TL_Circles;

std::vector<ContentViewCircle> GameServer::Expanse_t::accumulateContentViewCircles(ContentViewCircle circle, int depth)
{
    TL_Circles.clear();
    TL_Circles.reserve(256);
    TL_Circles.push_back(circle);
    _accumulateContentViewCircles(circle, depth);
    return TL_Circles;
}

void GameServer::Expanse_t::_accumulateContentViewCircles(ContentViewCircle circle, int depth) {
    for(const auto &pair : ContentBridges) {
        auto &br = pair.second;
        if(br.LeftWorld == circle.WorldId) {
            glm::i32vec3 vec = circle.Pos-br.LeftPos;
            ContentViewCircle circleNew = {br.RightWorld, br.RightPos, circle.Range-(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+16)};

            if(circleNew.Range >= 0) {
                bool isIn = false;

                for(ContentViewCircle &exCircle : TL_Circles) {
                    if(exCircle.WorldId != circleNew.WorldId)
                        continue;

                    vec = exCircle.Pos-circleNew.Pos;
                    if(exCircle.Range >= vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+circleNew.Range) {
                        isIn = true;
                        break;
                    }
                }

                if(isIn)
                    continue;

                TL_Circles.push_back(circleNew);
                if(depth > 1)
                    _accumulateContentViewCircles(circleNew, depth-1);
            }
        }

        if(br.IsTwoWay && br.RightWorld == circle.WorldId) {
            glm::i32vec3 vec = circle.Pos-br.RightPos;
            ContentViewCircle circleNew = {br.LeftWorld, br.LeftPos, circle.Range-(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+16)};

            if(circleNew.Range >= 0) {
                bool isIn = false;

                for(ContentViewCircle &exCircle : TL_Circles) {
                    if(exCircle.WorldId != circleNew.WorldId)
                        continue;

                    vec = exCircle.Pos-circleNew.Pos;
                    if(exCircle.Range >= vec.x*vec.x+vec.y*vec.y+vec.z*vec.z+circleNew.Range) {
                        isIn = true;
                        break;
                    }
                }

                if(isIn)
                    continue;

                TL_Circles.push_back(circleNew);
                if(depth > 1)
                    _accumulateContentViewCircles(circleNew, depth-1);
            }
        }
    }
}

// std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> GameServer::WorldObj::remapCVCsByWorld(const std::vector<ContentViewCircle> &list) {
//     std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> out;

//     for(const ContentViewCircle &circle : list) {
//         out[circle.WorldId].push_back(circle);
//     }

//     return out;
// }


ContentViewGlobal GameServer::Expanse_t::makeContentViewGlobal(const std::vector<ContentViewCircle> &views) {
    ContentViewGlobal cvg;
    Pos::GlobalRegion posRegion, lastPosRegion;
    std::bitset<4096> *cache = nullptr;

    for(const ContentViewCircle &circle : views) {
        ContentViewWorld &cvw = cvg[circle.WorldId];
        uint16_t chunkRange = std::sqrt(circle.Range);
        for(int32_t z = -chunkRange; z <= chunkRange; z++)
            for(int32_t y = -chunkRange; y <= chunkRange; y++)
                for(int32_t x = -chunkRange; x <= chunkRange; x++)
                {
                    if(z*z+y*y+x*x > circle.Range)
                        continue;

                    Pos::GlobalChunk posChunk(x+circle.Pos.x, y+circle.Pos.y, z+circle.Pos.z);
                    posRegion.fromChunk(posChunk);

                    if(!cache || lastPosRegion != posRegion) {
                        lastPosRegion = posRegion;
                        cache = &cvw[posRegion];
                    }

                    cache->_Unchecked_set(posChunk.toLocal());
                }
    }

    return cvg;
}

coro<> GameServer::pushSocketConnect(tcp::socket socket) {
    auto useLock = UseLock.lock();

    try {
        std::string magic = "AlterLuanti";
        co_await Net::AsyncSocket::read(socket, (std::byte*) magic.data(), magic.size());
        
        if(magic != "AlterLuanti") {
            co_return;
        }

        uint8_t mver = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
        if(mver != 0) {
            co_return;
        }

        uint8_t a_ar_r = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
        std::string username = co_await Net::AsyncSocket::read<std::string>(socket);
        if(username.size() > 255)
            co_return;


        std::string token = co_await Net::AsyncSocket::read<std::string>(socket);
        if(token.size() > 255)
            co_return;

        uint8_t response_code;
        std::string response_message;

        if(a_ar_r < 0 || a_ar_r > 2)
            co_return;

        bool authorized = false;
        // Авторизация
        if (a_ar_r == 0 || a_ar_r == 1) {
            authorized = true;
            response_code = 0;
            // Авторизация
        }

        bool justRegistered = false;

        if (!authorized && (a_ar_r == 1 || a_ar_r == 2)) {
            // Регистрация

            response_code = 1;
        }

        co_await Net::AsyncSocket::write<uint8_t>(socket, response_code);
        
        if(response_code > 1) {
            co_await Net::AsyncSocket::write(socket, "Неизвестный протокол");
        } else
            co_await pushSocketAuthorized(std::move(socket), username);
        
    } catch (const std::exception& e) {
    }
}

coro<> GameServer::pushSocketAuthorized(tcp::socket socket, const std::string username) {
    auto useLock = UseLock.lock();
    uint8_t code = co_await Net::AsyncSocket::read<uint8_t>(socket);
        
    if(code == 0) {
        co_await pushSocketGameProtocol(std::move(socket), username);
    } else {
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        co_await Net::AsyncSocket::write(socket, "Неизвестный протокол");
    }
}

coro<> GameServer::pushSocketGameProtocol(tcp::socket socket, const std::string username) {
    auto useLock = UseLock.lock();
    // Проверить не подключен ли уже игрок
    std::string ep = socket.remote_endpoint().address().to_string() + ':' + std::to_string(socket.remote_endpoint().port());

    bool isConnected = External.ConnectedPlayersSet.lock_read()->contains(username);

    if(isConnected) {
        LOG.info() << "Игрок не смог подключится (уже в игре) " << username;
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        co_await Net::AsyncSocket::write(socket, "Вы уже подключены к игре");
    } else if(IsGoingShutdown) {
        LOG.info() << "Игрок не смог подключится (сервер завершает работу) " << username;
        co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
        if(ShutdownReason.empty())
            co_await Net::AsyncSocket::write(socket, "Сервер завершает работу");
        else
            co_await Net::AsyncSocket::write(socket, "Сервер завершает работу, причина: "+ShutdownReason);
    } else {
        auto lock = External.ConnectedPlayersSet.lock_write();
        isConnected = lock->contains(username);

        if(isConnected) {
            lock.unlock();
            LOG.info() << "Игрок не смог подключится (уже в игре) " << username;
            co_await Net::AsyncSocket::write<uint8_t>(socket, 1);
            co_await Net::AsyncSocket::write(socket, "Вы уже подключены к игре");
        } else {
            LOG.info() << "Подключился к игре " << username;
            lock->insert(username);
            lock.unlock();

            co_await Net::AsyncSocket::write<uint8_t>(socket, 0);

            External.NewConnectedPlayers.lock_write()
               ->push_back(std::make_unique<RemoteClient>(IOC, std::move(socket), username));
        }
    }
}

Region* GameServer::forceGetRegion(WorldId_t worldId, Pos::GlobalRegion pos) {
    auto worldIter = Expanse.Worlds.find(worldId);
    assert(worldIter != Expanse.Worlds.end());
    World &world = *worldIter->second;

    auto iterRegion = world.Regions.find(pos);
    if(iterRegion == world.Regions.end() || !iterRegion->second->IsLoaded) {
        std::unique_ptr<Region> &region = world.Regions[pos];

        if(!region)
            region = std::make_unique<Region>();

        std::string worldName = "world_"+std::to_string(worldId);
        if(SaveBackend.World->isExist(worldName, pos)) {
            SB_Region data;
            SaveBackend.World->load(worldName, pos, &data);

            region->IsLoaded = true;
            region->load(&data);
        } else {
            
            region->IsLoaded = true;
            if(pos.Y == 0) {
                for(int z = 0; z < 16; z++)
                    for(int x = 0; x < 16; x++) {
                        region->Voxels[x][0][z].push_back({0, {0, 0, 0}, {255, 255, 255},});
                    }
            }
        }

        std::fill(region->IsChunkChanged_Voxels, region->IsChunkChanged_Voxels+64, ~0);
        return region.get();
    } else {
        return iterRegion->second.get();
    }
}

void GameServer::init(fs::path worldPath) {
    Expanse.Worlds[0] = std::make_unique<World>(0);

    SaveBackends::Filesystem fsbc;

    SaveBackend.World = fsbc.createWorld(boost::json::parse("{\"Path\": \"data/world\"}").as_object());
    SaveBackend.Player = fsbc.createPlayer(boost::json::parse("{\"Path\": \"data/player\"}").as_object());
    SaveBackend.Auth = fsbc.createAuth(boost::json::parse("{\"Path\": \"data/auth\"}").as_object());
    SaveBackend.ModStorage = fsbc.createModStorage(boost::json::parse("{\"Path\": \"data/mod_storage\"}").as_object());

    RunThread = std::thread(&GameServer::prerun, this);
}

void GameServer::prerun() {
    try {
        auto useLock = UseLock.lock();
        run();

    } catch(...) {
    }

    IsAlive = false;
}

void GameServer::run() {

    while(true) {
        ((uint32_t&) Game.AfterStartTime) += (uint32_t) (CurrentTickDuration*256);

        std::chrono::steady_clock::time_point atTickStart = std::chrono::steady_clock::now();

        if(IsGoingShutdown) {
            // Отключить игроков
            for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
                cec->Remote->shutdown(EnumDisconnect::ByInterface, ShutdownReason);
            }
            
            // Сохранить данные
            save();

            {
                // Отключить вновь подключившихся
                auto lock = External.NewConnectedPlayers.lock_write();

                for(std::unique_ptr<RemoteClient> &client : *lock) {
                    client->shutdown(EnumDisconnect::ByInterface, ShutdownReason);
                }

                bool hasNewConnected = !lock->empty();
                lock.unlock();

                // Если были ещё подключившиеся сделать паузу на 1 секунду
                if(hasNewConnected)
                    std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Конец
            break;
        }

        // 
        stepContent();

        stepSyncWithAsync();

        // Принять события от игроков
        stepPlayers();
        
        // Обновить регионы
        stepWorlds();

        // Проверить видимый контент
        stepViewContent();

        // Отправить пакеты игрокам && Проверить контент необходимый для отправки
        stepSendPlayersPackets();

        // Выставить регионы на прогрузку
        stepLoadRegions();

        // Lua globalStep
        stepGlobal();

        // Сохранение
        stepSave();

        // Сон или подгонка длительности такта при высоких нагрузках
        std::chrono::steady_clock::time_point atTickEnd = std::chrono::steady_clock::now();
        float currentWastedTime = double((atTickEnd-atTickStart).count() * std::chrono::steady_clock::duration::period::num) / std::chrono::steady_clock::duration::period::den;
        GlobalTickLagTime += CurrentTickDuration-currentWastedTime;

        if(GlobalTickLagTime > 0) {
            CurrentTickDuration -= PerTickAdjustment;
            if(CurrentTickDuration < PerTickDuration)
                CurrentTickDuration = PerTickDuration;

            std::this_thread::sleep_for(std::chrono::milliseconds(uint32_t(1000*GlobalTickLagTime)));
            GlobalTickLagTime = 0;
        } else {
            CurrentTickDuration += PerTickAdjustment;
        }
    }

    LOG.info() << "Сервер завершил работу";
}

void GameServer::stepContent() {
    Content.TextureM.update(CurrentTickDuration);
    if(Content.TextureM.hasPreparedInformation()) {
        auto table = Content.TextureM.takePreparedInformation();
        for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
            cec->Remote->informateDefTexture(table);
        }
    }

    Content.ModelM.update(CurrentTickDuration);
    if(Content.ModelM.hasPreparedInformation()) {
        auto table = Content.ModelM.takePreparedInformation();
        for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
            cec->Remote->informateDefTexture(table);
        }
    }

    Content.SoundM.update(CurrentTickDuration);
    if(Content.SoundM.hasPreparedInformation()) {
        auto table = Content.SoundM.takePreparedInformation();
        for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
            cec->Remote->informateDefTexture(table);
        }
    }
}

void GameServer::stepSyncWithAsync() {
    for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
        assert(cec);

        for(const auto &[worldId, regions] : cec->ContentViewState) {
            for(const auto &[regionPos, chunkBitfield] : regions) {
                forceGetRegion(worldId, regionPos);
            }
        }

        // Подпись на регионы
        for(const auto &[worldId, newRegions] : cec->ContentView_NewView.Regions) {
            auto worldIter = Expanse.Worlds.find(worldId);
            assert(worldIter != Expanse.Worlds.end() && "TODO: Логика не определена");
            assert(worldIter->second);
            World &world = *worldIter->second;

            // Подписать наблюдателей на регионы миров
            world.onCEC_RegionsEnter(cec.get(), newRegions);
        }
    }
}

void GameServer::stepPlayers() {
    // Подключить новых игроков
    if(!External.NewConnectedPlayers.no_lock_readable().empty()) {
        auto lock = External.NewConnectedPlayers.lock_write();

        for(std::unique_ptr<RemoteClient> &client : *lock) {
            co_spawn(client->run());
            Game.CECs.push_back(std::make_unique<ContentEventController>(std::move(client)));
        }

        lock->clear();
    }

    // Обработка игроков
    for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
        // Убрать отключившихся
        if(!cec->Remote->isConnected()) {
            // Отписываем наблюдателя от миров
            for(auto wPair : cec->ContentViewState) {
                auto wIter = Expanse.Worlds.find(wPair.first);
                if(wIter == Expanse.Worlds.end())
                    continue;

                std::vector<Pos::GlobalRegion> regions;
                regions.reserve(wPair.second.size());
                for(const auto &[rPos, _] : wPair.second) {
                    regions.push_back(rPos);
                }

                wIter->second->onCEC_RegionsLost(cec.get(), regions);
            }

            std::string username = cec->Remote->Username;
            External.ConnectedPlayersSet.lock_write()->erase(username);
            
            cec.reset();
        }

        
    }

    // Вычистить невалидные ссылки на игроков
    Game.CECs.erase(std::remove_if(Game.CECs.begin(), Game.CECs.end(),
            [](const std::unique_ptr<ContentEventController>& ptr) { return !ptr; }), 
        Game.CECs.end());
}

void GameServer::stepWorlds() {
    for(auto &pair : Expanse.Worlds)
        pair.second->onUpdate(this, CurrentTickDuration);

    for(auto &pWorld : Expanse.Worlds) {
        World &wobj = *pWorld.second;
       
        assert(pWorld.first == 0 && "Требуется WorldManager");

        std::string worldStringId = "unexisten";

        std::vector<Pos::GlobalRegion> regionsToRemove;
        for(auto &pRegion : wobj.Regions) {
            Region &region = *pRegion.second;

            // Позиции исчисляются в целых числах
            // Вместо умножения на dtime, используется *dTimeMul/dTimeDiv
            int32_t dTimeDiv = Pos::Object_t::BS;
            int32_t dTimeMul = dTimeDiv*CurrentTickDuration;

            // Обновить сущностей
            for(size_t entityIndex = 0; entityIndex < region.Entityes.size(); entityIndex++) {
                Entity &entity = region.Entityes[entityIndex];

                if(entity.IsRemoved)
                    continue;

                // Если нет ни скорости, ни ускорения, то пропускаем расчёт
                // Ускорение свободного падения?
                if((entity.Speed.x != 0 || entity.Speed.y != 0 || entity.Speed.z != 0)
                        || (entity.Acceleration.x != 0 || entity.Acceleration.y != 0 || entity.Acceleration.z != 0))
                {
                    Pos::Object &eSpeed = entity.Speed;

                    // Ограничение на 256 м/с
                    static constexpr int32_t MAX_SPEED_PER_SECOND = 256*Pos::Object_t::BS;
                    {
                        uint32_t linearSpeed = std::sqrt(eSpeed.x*eSpeed.x + eSpeed.y*eSpeed.y + eSpeed.z*eSpeed.z);

                        if(linearSpeed > MAX_SPEED_PER_SECOND) {
                            eSpeed *= MAX_SPEED_PER_SECOND;
                            eSpeed /= linearSpeed;
                        }

                        Pos::Object &eAcc = entity.Acceleration;
                        linearSpeed = std::sqrt(eAcc.x*eAcc.x + eAcc.y*eAcc.y + eAcc.z*eAcc.z);

                        if(linearSpeed > MAX_SPEED_PER_SECOND/2) {
                            eAcc *= MAX_SPEED_PER_SECOND/2;
                            eAcc /= linearSpeed;
                        }
                    }

                    // Потенциальное изменение позиции сущности в пустой области
                    // vt+(at^2)/2 = (v+at/2)*t = (Скорость + Ускорение/2*dtime)*dtime
                    Pos::Object dpos = (eSpeed + entity.Acceleration/2*dTimeMul/dTimeDiv)*dTimeMul/dTimeDiv;
                    // Стартовая и конечная позиции
                    Pos::Object &startPos = entity.Pos, endPos = entity.Pos + dpos;
                    // Новая скорость
                    Pos::Object nSpeed = entity.Speed + entity.Acceleration*dTimeMul/dTimeDiv;

                    // Зона расчёта коллизии
                    AABB collideZone = {startPos, endPos};
                    collideZone.sortMinMax();
                    collideZone.VecMin -= Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);
                    collideZone.VecMax += Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);

                    // Сбор ближайших коробок
                    std::vector<CollisionAABB> Boxes;

                    {
                        glm::ivec3 beg = collideZone.VecMin >> 20, end = (collideZone.VecMax + 0xfffff) >> 20;

                        for(; beg.z <= end.z; beg.z++)
                        for(; beg.y <= end.y; beg.y++)
                        for(; beg.x <= end.x; beg.x++) {
                            Pos::GlobalRegion rPos(beg.x, beg.y, beg.z);
                            auto iterChunk = wobj.Regions.find(rPos);
                            if(iterChunk == wobj.Regions.end())
                                continue;

                            iterChunk->second->getCollideBoxes(rPos, collideZone, Boxes);
                        }
                    }

                    // Коробка сущности
                    AABB entityAABB = entity.aabbAtPos();

                    // Симулируем физику
                    // Оставшееся время симуляции
                    int32_t remainingSimulationTime = dTimeMul;
                    // Оси, по которым было пересечение
                    bool axis[3]; // x y z

                    // Симулируем пока не будет просчитано выделенное время
                    while(remainingSimulationTime > 0) {
                        if(nSpeed.x == 0 && nSpeed.y == 0 && nSpeed.z == 0)
                            break; // Скорости больше нет

                        entityAABB = entity.aabbAtPos();

                        // Ближайшее время пересечения с боксом
                        int32_t minSimulationTime = remainingSimulationTime;
                        // Ближайший бокс в пересечении
                        int nearest_boxindex = -1;

                        for(size_t index = 0; index < Boxes.size(); index++) {
                            CollisionAABB &caabb = Boxes[index];

                            if(caabb.Skip)
                                continue;

                            int32_t delta;
                            if(!entityAABB.collideWithDelta(caabb, nSpeed, delta, axis))
                                continue;

                            if(delta > remainingSimulationTime)
                                continue;

                            nearest_boxindex = index;
                            minSimulationTime = delta;
                        }

                        if(nearest_boxindex == -1) {
                            // Свободный ход
                            startPos += nSpeed*dTimeDiv/minSimulationTime;
                            remainingSimulationTime = 0;
                            break;
                        } else {
                            if(minSimulationTime == 0) {
                                // Уже где-то застряли
                                // Да и хрен бы с этим
                            } else {
                                // Где-то встрянем через minSimulationTime
                                startPos += nSpeed*dTimeDiv/minSimulationTime;
                                remainingSimulationTime -= minSimulationTime;

                                nSpeed.x = nSpeed.y = nSpeed.z = 0;
                                break;
                            }

                            if(axis[0] == 0) {
                                nSpeed.x = 0;
                            }
                            
                            if(axis[1] == 0) {
                                nSpeed.y = 0;
                            } 
                            
                            if(axis[2] == 0) {
                                nSpeed.z = 0;
                            }

                            CollisionAABB &caabb = Boxes[nearest_boxindex];
                            caabb.Skip = true;
                        }
                    }

                    // Симуляция завершена
                }

                // Сущность будет вычищена
                if(entity.NeedRemove) {
                    entity.NeedRemove = false;
                    entity.IsRemoved = true;
                }

                // Проверим необходимость перемещения сущности в другой регион
                // Вынести в отдельный этап обновления сервера, иначе будут происходить двойные симуляции
                // сущностей при пересечении регионов/миров
                {
                    Pos::Object temp = entity.Pos >> 20;
                    Pos::GlobalRegion rPos(temp.x, temp.y, temp.z);

                    if(rPos != pRegion.first || pWorld.first != entity.WorldId) {

                        Region *toRegion = forceGetRegion(entity.WorldId, rPos);
                        LocalEntityId_t newId = toRegion->pushEntity(entity);
                        // toRegion->Entityes[newId].WorldId = Если мир изменился

                        if(newId == LocalEntityId_t(-1)) {
                            // В другом регионе нет места
                        } else {
                            entity.IsRemoved = true;
                            // Сообщаем о перемещении сущности
                            for(ContentEventController *cec : region.CECs) {
                                cec->onEntitySwap(pWorld.first, pRegion.first, entityIndex, entity.WorldId, rPos, newId);
                            }
                        }
                    }
                }
            }

            // Проверить необходимость перерасчёта вертикальной проходимости света 
            std::unordered_map<Pos::Local16_u, const LightPrism*> ChangedLightPrism;
            {
                for(int big = 0; big < 64; big++) {
                    uint64_t bits = region.IsChunkChanged_Voxels[big] | region.IsChunkChanged_Nodes[big];

                    if(!bits)
                        continue;

                    for(int little = 0; little < 64; little++) {
                        if(((bits >> little) & 1) == 0)
                            continue;

                        
                    }
                }
            }

            // Сбор данных об изменившихся чанках
            std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> ChangedVoxels;
            std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> ChangedNodes;
            {

                for(int big = 0; big < 64; big++) {
                    uint64_t bits_voxels = region.IsChunkChanged_Voxels[big];
                    uint64_t bits_nodes = region.IsChunkChanged_Nodes[big];

                    if(!bits_voxels && !bits_nodes)
                        continue;

                    for(int little = 0; little < 64; little++) {
                        Pos::Local16_u pos(little & 0xf, ((big & 0x3) << 2) | (little >> 4), big >> 2);

                        if(((bits_voxels >> little) & 1) == 1) {
                            ChangedVoxels[pos] = &region.Voxels[pos.X][pos.Y][pos.Z];
                        }

                        if(((bits_nodes >> little) & 1) == 1) {
                            ChangedNodes[pos] = &region.Nodes[pos.X][pos.Y][pos.Z];
                        }
                    }
                }
            }

            // Об изменившихся сущностях
            {

            }

            if(++region.CEC_NextChunkAndEntityesViewCheck >= region.CECs.size())
                region.CEC_NextChunkAndEntityesViewCheck = 0;

            // Пробегаемся по всем наблюдателям
            {
                size_t cecIndex = 0;
                for(ContentEventController *cec : region.CECs) {
                    cecIndex++;

                    auto cvwIter = cec->ContentViewState.find(pWorld.first);
                    if(cvwIter == cec->ContentViewState.end())
                        // Мир не отслеживается
                        continue;


                    const ContentViewWorld &cvw = cvwIter->second;
                    auto chunkBitsetIter = cvw.find(pRegion.first);
                    if(chunkBitsetIter == cvw.end())
                        // Регион не отслеживается
                        continue;

                    // Наблюдаемые чанки
                    const std::bitset<4096> &chunkBitset = chunkBitsetIter->second;

                    // Пересылка изменений в регионе
                    if(!ChangedLightPrism.empty())
                        cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, ChangedLightPrism);

                    if(!ChangedVoxels.empty())
                        cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, ChangedVoxels);

                    if(!ChangedNodes.empty())
                        cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, ChangedNodes);

                    // Отправка полной информации о новых наблюдаемых чанках
                    {
                        const std::bitset<4096> *new_chunkBitset = nullptr;
                        try { new_chunkBitset = &cec->ContentView_NewView.View.at(pWorld.first).at(pRegion.first); } catch(...) {}

                        if(new_chunkBitset) {
                            std::unordered_map<Pos::Local16_u, const LightPrism*> newLightPrism;
                            std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> newVoxels;
                            std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> newNodes;

                            newLightPrism.reserve(new_chunkBitset->count());
                            newVoxels.reserve(new_chunkBitset->count());
                            newNodes.reserve(new_chunkBitset->count());

                            size_t bitPos = new_chunkBitset->_Find_first();
                            while(bitPos != new_chunkBitset->size()) {
                                Pos::Local16_u chunkPos;
                                chunkPos = bitPos;

                                newLightPrism.insert({chunkPos, &region.Lights[0][0][chunkPos.X][chunkPos.Y][chunkPos.Z]});
                                newVoxels.insert({chunkPos, &region.Voxels[chunkPos.X][chunkPos.Y][chunkPos.Z]});
                                newNodes.insert({chunkPos, &region.Nodes[chunkPos.X][chunkPos.Y][chunkPos.Z]});

                                bitPos = new_chunkBitset->_Find_next(bitPos);
                            }

                            cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, newLightPrism);
                            cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, newVoxels);
                            cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, newNodes);
                        }
                    }

                    // То, что уже отслеживает наблюдатель
                    const auto &subs = cec->getSubscribed();

                    // // Проверка отслеживания сущностей
                    // if(cecIndex-1 == region.CEC_NextChunkAndEntityesViewCheck) {
                    //     std::vector<LocalEntityId_t> newEntityes, lostEntityes;
                    //     for(size_t iter = 0; iter < region.Entityes.size(); iter++) {
                    //         Entity &entity = region.Entityes[iter];

                    //         if(entity.IsRemoved)
                    //             continue;

                    //         for(const ContentViewCircle &circle : cvc->second) {
                    //             int x = entity.ABBOX.x >> 17;
                    //             int y = entity.ABBOX.y >> 17;
                    //             int z = entity.ABBOX.z >> 17;

                    //             uint32_t size = 0;
                    //             if(circle.isIn(entity.Pos, x*x+y*y+z*z))
                    //                 newEntityes.push_back(iter);
                    //         }
                    //     }

                    //     std::unordered_set<LocalEntityId_t> newEntityesSet(newEntityes.begin(), newEntityes.end());

                    //     {
                    //         auto iterR_W = subs.Entities.find(pWorld.first);
                    //         if(iterR_W == subs.Entities.end())
                    //             // Если мир не отслеживается наблюдателем
                    //             goto doesNotObserveEntityes;

                    //         auto iterR_W_R = iterR_W->second.find(pRegion.first);
                    //         if(iterR_W_R == iterR_W->second.end())
                    //             // Если регион не отслеживается наблюдателем
                    //             goto doesNotObserveEntityes;

                    //         // Подходят ли уже наблюдаемые сущности под наблюдательные области
                    //         for(LocalEntityId_t eId : iterR_W_R->second) {
                    //             if(eId >= region.Entityes.size()) {
                    //                 lostEntityes.push_back(eId);
                    //                 break;
                    //             }

                    //             Entity &entity = region.Entityes[eId];

                    //             if(entity.IsRemoved) {
                    //                 lostEntityes.push_back(eId);
                    //                 break;
                    //             }

                    //             int x = entity.ABBOX.x >> 17;
                    //             int y = entity.ABBOX.y >> 17;
                    //             int z = entity.ABBOX.z >> 17;

                    //             for(const ContentViewCircle &circle : cvc->second) {
                    //                 if(!circle.isIn(entity.Pos, x*x+y*y+z*z))
                    //                     lostEntityes.push_back(eId);
                    //             }
                    //         }

                    //         // Удалим чанки которые наблюдатель уже видит
                    //         for(LocalEntityId_t eId : iterR_W_R->second)
                    //             newEntityesSet.erase(eId);
                    //     }

                    //     doesNotObserveEntityes:

                    //     cec->onEntityEnterLost(pWorld.first, pRegion.first, newEntityesSet, std::unordered_set<LocalEntityId_t>(lostEntityes.begin(), lostEntityes.end()));
                    //     // Отправить полную информацию о новых наблюдаемых сущностях наблюдателю
                    // }

                    if(!region.Entityes.empty())
                        cec->onEntityUpdates(pWorld.first, pRegion.first, region.Entityes);
                }
            }


            // Сохраняем регионы
            region.LastSaveTime += CurrentTickDuration;

            bool needToUnload = region.CECs.empty() && region.LastSaveTime > 60;
            bool needToSave = region.IsChanged && region.LastSaveTime > 15;

            if(needToUnload || needToSave) {
                region.LastSaveTime = 0;
                region.IsChanged = false;

                SB_Region data;
                convertChunkVoxelsToRegion((const std::vector<VoxelCube>*) region.Voxels, data.Voxels);
                SaveBackend.World->save(worldStringId, pRegion.first, &data);
            }

            // Выгрузим регионы
            if(needToUnload) {
                regionsToRemove.push_back(pRegion.first);
            }

            // Сброс информации об изменившихся данных

            std::fill(region.IsChunkChanged_Voxels, region.IsChunkChanged_Voxels+64, 0);
            std::fill(region.IsChunkChanged_Nodes, region.IsChunkChanged_Nodes+64, 0);
        }

        for(Pos::GlobalRegion regionPos : regionsToRemove) {
            auto iter = wobj.Regions.find(regionPos);
            if(iter == wobj.Regions.end())
                continue;

            wobj.Regions.erase(iter);
        }

        // Загрузить регионы
        if(wobj.NeedToLoad.empty())
            continue;
 
        for(Pos::GlobalRegion &regionPos : wobj.NeedToLoad) {
            if(!SaveBackend.World->isExist(worldStringId, regionPos)) {
                wobj.Regions[regionPos]->IsLoaded = true;
            } else {
                SB_Region data;
                SaveBackend.World->load(worldStringId, regionPos, &data);
                Region &robj = *wobj.Regions[regionPos];

                // TODO: Передефайнить идентификаторы нод

                convertRegionVoxelsToChunks(data.Voxels, (std::vector<VoxelCube>*) robj.Voxels);
            }
        }

        wobj.NeedToLoad.clear();
    }
    
    // Проверить отслеживание порталов

}

void GameServer::stepViewContent() {
    if(Game.CECs.empty())
        return;

    // Затереть изменения предыдущего такта
    for(auto &cecPtr : Game.CECs) {
        assert(cecPtr);
        cecPtr->ContentView_NewView = {};
        cecPtr->ContentView_LostView = {};
    }

    // Если наблюдаемая территория изменяется
    // -> Новая увиденная + Старая потерянная
    std::unordered_map<ContentEventController*, ContentViewGlobal_DiffInfo> lost_CVG;

    // Обновления поля зрения
    for(int iter = 0; iter < 1; iter++) {
        if(++Game.CEC_NextRebuildViewCircles >= Game.CECs.size())
            Game.CEC_NextRebuildViewCircles = 0;

        ContentEventController &cec = *Game.CECs[Game.CEC_NextRebuildViewCircles];
        ServerObjectPos oPos = cec.getPos();

        ContentViewCircle cvc;
        cvc.WorldId = oPos.WorldId;
        cvc.Pos = Pos::Object_t::asChunkPos(oPos.ObjectPos);
        cvc.Range = cec.getViewRangeActive();
        cvc.Range *= cvc.Range;

        std::vector<ContentViewCircle> newCVCs = Expanse.accumulateContentViewCircles(cvc);
        //size_t hash = (std::hash<std::vector<ContentViewCircle>>{})(newCVCs);
        if(/*hash != cec.CVCHash*/ true) {
            //cec.CVCHash = hash;
            ContentViewGlobal newCbg = Expanse_t::makeContentViewGlobal(newCVCs);
            ContentViewGlobal_DiffInfo newView = newCbg.calcDiffWith(cec.ContentViewState);
            ContentViewGlobal_DiffInfo lostView = cec.ContentViewState.calcDiffWith(newCbg);
            if(!newView.empty() || !lostView.empty()) {
                lost_CVG.insert({&cec, {newView}});
                cec.ContentViewState = std::move(newCbg);
                cec.ContentView_NewView = std::move(newView);
                cec.ContentView_LostView = std::move(lostView);
            }
        }
    }

    for(const auto &[cec, lostView] : lost_CVG) {
        // Отписать наблюдателей от регионов миров
        for(const auto &[worldId, lostList] : lostView.Regions) {
            auto worldIter = Expanse.Worlds.find(worldId);
            assert(worldIter != Expanse.Worlds.end() && "TODO: Логика не определена");
            assert(worldIter->second);

            World &world = *worldIter->second;
            world.onCEC_RegionsLost(cec, lostList);
        }

        cec->checkContentViewChanges();
    }
}

void GameServer::stepSendPlayersPackets() {
    ResourceRequest full;

    for(std::unique_ptr<ContentEventController> &cec : Game.CECs) {
        full.insert(cec->Remote->pushPreparedPackets());
    }

    full.uniq();

    if(!full.NewTextures.empty())
        Content.TextureM.needResourceResponse(full.NewTextures);

    if(!full.NewModels.empty())
        Content.ModelM.needResourceResponse(full.NewModels);

    if(!full.NewSounds.empty())
        Content.SoundM.needResourceResponse(full.NewSounds);

}

void GameServer::stepLoadRegions() {
    for(auto &iterWorld : Expanse.Worlds) {
        for(Pos::GlobalRegion pos : iterWorld.second->NeedToLoad) {
            forceGetRegion(iterWorld.first, pos);
        }

        iterWorld.second->NeedToLoad.clear();
    }
}

void GameServer::stepGlobal() {

}

void GameServer::stepSave() {

}

void GameServer::save() {

}

}