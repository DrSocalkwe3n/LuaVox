#include "GameServer.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include <boost/json/parse.hpp>
#include <chrono>
#include <glm/geometric.hpp>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "SaveBackends/Filesystem.hpp"

namespace LV::Server {

GameServer::~GameServer() {
    shutdown("on ~GameServer");
    RunThread.join();
    WorkDeadline.cancel();
    UseLock.wait_no_use();
    LOG.info() << "Сервер уничтожен";
}

static thread_local std::vector<ContentViewCircle> TL_Circles;

std::vector<ContentViewCircle> GameServer::WorldObj::calcCVCs(ContentViewCircle circle, int depth)
{
    TL_Circles.reserve(4096);
    TL_Circles.push_back(circle);
    _calcContentViewCircles(TL_Circles.front(), depth);
    return TL_Circles;
}

void GameServer::WorldObj::_calcContentViewCircles(ContentViewCircle circle, int depth) {
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
                    _calcContentViewCircles(circleNew, depth-1);
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
                    _calcContentViewCircles(circleNew, depth-1);
            }
        }
    }
}

std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> GameServer::WorldObj::remapCVCsByWorld(const std::vector<ContentViewCircle> &list) {
    std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> out;

    for(const ContentViewCircle &circle : list) {
        out[circle.WorldId].push_back(circle);
    }

    return out;
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
                cec->Remote->shutdown(ShutdownReason);
            }
            
            // Сохранить данные
            save();

            {
                // Отключить вновь подключившихся
                auto lock = External.NewConnectedPlayers.lock_write();

                for(std::unique_ptr<RemoteClient> &client : *lock) {
                    client->shutdown(ShutdownReason);
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
        float freeTime = CurrentTickDuration-currentWastedTime-GlobalTickLagTime;

        if(freeTime > 0) {
            CurrentTickDuration -= PerTickAdjustment;
            if(CurrentTickDuration < PerTickDuration)
                CurrentTickDuration = PerTickDuration;

            std::this_thread::sleep_for(std::chrono::milliseconds(uint32_t(1000*freeTime)));
        } else {
            GlobalTickLagTime = freeTime;
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

                        Region *toRegion = Expanse.Worlds[entity.WorldId]->forceLoadOrGetRegion(rPos);
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

            // Пробегаемся по всем наблюдателям
            for(ContentEventController *cec : region.CECs) {
                // То, что уже отслеживает наблюдатель
                const auto &subs = cec->getSubscribed();
                
                auto cvc = cec->ContentViewCircles.find(pWorld.first);
                if(cvc == cec->ContentViewCircles.end())
                    // Ничего не должно отслеживаться
                    continue;

                // Проверка отслеживания чанков
                {
                    // Чанки, которые игрок уже не видит и которые только что увидел
                    
                    std::vector<Pos::Local16_u> lostChunks, newChunks;
                    
                    // Проверим чанки которые наблюдатель может наблюдать
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++)
                            for(int x = 0; x < 16; x++) {
                                Pos::GlobalChunk gcPos((pRegion.first.X << 4) | x,
                                    (pRegion.first.Y << 4) | y,
                                    (pRegion.first.Z << 4) | z);

                                for(const ContentViewCircle &circle : cvc->second) {
                                    if(circle.isIn(gcPos))
                                        newChunks.push_back(Pos::Local16_u(x, y, z));
                                }
                            }

                    std::unordered_set<Pos::Local16_u> newChunksSet(newChunks.begin(), newChunks.end());

                    {
                        auto iterR_W = subs.Chunks.find(pWorld.first);
                        if(iterR_W == subs.Chunks.end())
                            // Если мир не отслеживается наблюдателем
                            goto doesNotObserve;

                        auto iterR_W_R = iterR_W->second.find(pRegion.first);
                        if(iterR_W_R == iterR_W->second.end())
                            // Если регион не отслеживается наблюдателем
                            goto doesNotObserve;

                        // Подходят ли уже наблюдаемые чанки под наблюдательные области
                        for(Pos::Local16_u cPos : iterR_W_R->second) {
                            Pos::GlobalChunk gcPos((pRegion.first.X << 4) | cPos.X,
                                (pRegion.first.Y << 4) | cPos.Y,
                                (pRegion.first.Z << 4) | cPos.Z);

                            for(const ContentViewCircle &circle : cvc->second) {
                                if(!circle.isIn(gcPos))
                                    lostChunks.push_back(cPos);
                            }
                        }

                        // Удалим чанки которые наблюдатель уже видит
                        for(Pos::Local16_u cPos : iterR_W_R->second)
                            newChunksSet.erase(cPos);
                    }

                    doesNotObserve:

                    if(!newChunksSet.empty() || !lostChunks.empty())
                        cec->onChunksEnterLost(pWorld.first, pWorld.second.get(), pRegion.first, newChunksSet, std::unordered_set<Pos::Local16_u>(lostChunks.begin(), lostChunks.end()));

                    // Нужно отправить полную информацию о новых наблюдаемых чанках наблюдателю
                    if(!newChunksSet.empty()) {
                        std::unordered_map<Pos::Local16_u, const LightPrism*> newLightPrism;
                        std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> newVoxels;
                        std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> newNodes;

                        for(Pos::Local16_u cPos : newChunksSet) {
                            newLightPrism[cPos] = &region.Lights[0][0][cPos.X][cPos.Y][cPos.Z];
                            newVoxels[cPos] = &region.Voxels[cPos.X][cPos.Y][cPos.Z];
                            newNodes[cPos] = &region.Nodes[cPos.X][cPos.Y][cPos.Z];
                        }

                        cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, newLightPrism);
                        cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, newVoxels);
                        cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, newNodes);
                    }

                    if(!ChangedLightPrism.empty())
                        cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, ChangedLightPrism);

                    if(!ChangedVoxels.empty())
                        cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, ChangedVoxels);

                    if(!ChangedNodes.empty())
                        cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, ChangedNodes);
                }

                // Проверка отслеживания сущностей
                {
                    std::vector<LocalEntityId_t> newEntityes, lostEntityes;
                    for(size_t iter = 0; iter < region.Entityes.size(); iter++) {
                        Entity &entity = region.Entityes[iter];

                        if(entity.IsRemoved)
                            continue;

                        for(const ContentViewCircle &circle : cvc->second) {
                            int x = entity.ABBOX.x >> 17;
                            int y = entity.ABBOX.y >> 17;
                            int z = entity.ABBOX.z >> 17;

                            uint32_t size = 0;
                            if(circle.isIn(entity.Pos, x*x+y*y+z*z))
                                newEntityes.push_back(iter);
                        }
                    }

                    std::unordered_set<LocalEntityId_t> newEntityesSet(newEntityes.begin(), newEntityes.end());

                    {
                        auto iterR_W = subs.Entities.find(pWorld.first);
                        if(iterR_W == subs.Entities.end())
                            // Если мир не отслеживается наблюдателем
                            goto doesNotObserveEntityes;

                        auto iterR_W_R = iterR_W->second.find(pRegion.first);
                        if(iterR_W_R == iterR_W->second.end())
                            // Если регион не отслеживается наблюдателем
                            goto doesNotObserveEntityes;

                        // Подходят ли уже наблюдаемые сущности под наблюдательные области
                        for(LocalEntityId_t eId : iterR_W_R->second) {
                            if(eId >= region.Entityes.size()) {
                                lostEntityes.push_back(eId);
                                break;
                            }

                            Entity &entity = region.Entityes[eId];

                            if(entity.IsRemoved) {
                                lostEntityes.push_back(eId);
                                break;
                            }

                            int x = entity.ABBOX.x >> 17;
                            int y = entity.ABBOX.y >> 17;
                            int z = entity.ABBOX.z >> 17;

                            for(const ContentViewCircle &circle : cvc->second) {
                                if(!circle.isIn(entity.Pos, x*x+y*y+z*z))
                                    lostEntityes.push_back(eId);
                            }
                        }

                        // Удалим чанки которые наблюдатель уже видит
                        for(LocalEntityId_t eId : iterR_W_R->second)
                            newEntityesSet.erase(eId);
                    }

                    doesNotObserveEntityes:

                    cec->onEntityEnterLost(pWorld.first, pRegion.first, newEntityesSet, std::unordered_set<LocalEntityId_t>(lostEntityes.begin(), lostEntityes.end()));
                    // Отправить полную информацию о новых наблюдаемых сущностях наблюдателю
                }

                if(!region.Entityes.empty())
                    cec->onEntityUpdates(pWorld.first, pRegion.first, region.Entityes);
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

    // Обновления поля зрения
    for(int iter = 0; iter < 1; iter++) {
        if(++Game.CEC_NextRebuildViewCircles >= Game.CECs.size())
            Game.CEC_NextRebuildViewCircles = 0;

        ContentEventController &cec = *Game.CECs[Game.CEC_NextRebuildViewCircles];
        ServerObjectPos oPos = cec.getPos();

        ContentViewCircle cvc;
        cvc.WorldId = oPos.WorldId;
        cvc.Pos = {oPos.ObjectPos.x >> (Pos::Object_t::BS_Bit+4), oPos.ObjectPos.y >> (Pos::Object_t::BS_Bit+4), oPos.ObjectPos.z >> (Pos::Object_t::BS_Bit+4)};
        cvc.Range = cec.getViewRange();

        cec.ContentViewCircles = Expanse.calcAndRemapCVC(cvc);
    }

    // Прогрузить то, что видят игроки
    for(int iter = 0; iter < 1; iter++) {
        if(++Game.CEC_NextCheckRegions >= Game.CECs.size())
            Game.CEC_NextCheckRegions = 0;

        ContentEventController &cec = *Game.CECs[Game.CEC_NextRebuildViewCircles];
        ServerObjectPos oLPos = cec.getLastPos(), oPos = cec.getPos();

        std::vector<Pos::GlobalRegion> lost;

        // Снимаем подписки с регионов
        for(auto &pairSR : cec.SubscribedRegions) {
            auto CVCs = cec.ContentViewCircles.find(pairSR.first);

            if(CVCs == cec.ContentViewCircles.end()) {
                lost = pairSR.second;
            } else {
                for(Pos::GlobalRegion &region : pairSR.second) {
                    for(ContentViewCircle &circle : CVCs->second) {
                        if(!circle.isIn(region)) {
                            lost.push_back(region);
                            break;
                        }
                    }
                }
            }
            
            cec.onRegionsLost(pairSR.first, lost);

            auto world = Expanse.Worlds.find(pairSR.first);
            if(world != Expanse.Worlds.end())
                world->second->onCEC_RegionsLost(&cec, lost);

            lost.clear();
        }
        

        // Проверяем отслеживаемые регионы
        std::vector<Pos::GlobalRegion> regionsResult;
        for(auto &pair : cec.ContentViewCircles) {
            auto world = Expanse.Worlds.find(pair.first);
            if(world == Expanse.Worlds.end())
                continue;

            std::vector<Pos::GlobalRegion> regionsLeft;

            for(ContentViewCircle &circle : pair.second) {
                int16_t offset = (circle.Range >> __builtin_popcount(circle.Range))+1;
                glm::i16vec3 left = (circle.Pos >> int16_t(4))-int16_t(offset);
                glm::i16vec3 right = (circle.Pos >> int16_t(4))+int16_t(offset);
                for(int x = left.x; x <= right.x; x++)
                for(int y = left.y; y <= right.y; y++)
                for(int z = left.z; z <= right.z; z++) {
                    if(circle.isIn(Pos::GlobalRegion(x, y, z)))
                        regionsLeft.emplace_back(x, y, z);
                }
            }

            std::sort(regionsLeft.begin(), regionsLeft.end());
            auto last = std::unique(regionsLeft.begin(), regionsLeft.end());
            regionsLeft.erase(last, regionsLeft.end());

            std::vector<Pos::GlobalRegion> &regionsRight = cec.SubscribedRegions[pair.first];
            std::sort(regionsRight.begin(), regionsRight.end());

            std::set_difference(regionsLeft.begin(), regionsLeft.end(),
                regionsRight.begin(), regionsRight.end(),
                std::back_inserter(regionsResult));
            
            if(!regionsResult.empty()) {
                regionsRight.insert(regionsRight.end(), regionsResult.begin(), regionsResult.end());
                world->second->onCEC_RegionsEnter(&cec, regionsResult);
                regionsResult.clear();
            }
        }
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

}

void GameServer::stepGlobal() {

}

void GameServer::stepSave() {

}

void GameServer::save() {

}

}