#include "GameServer.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Common/Packets.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include <algorithm>
#include <array>
#include <boost/json/parse.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <glm/geometric.hpp>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "SaveBackends/Filesystem.hpp"
#include "Server/SaveBackend.hpp"
#include "Server/World.hpp"
#include "TOSLib.hpp"
#include "glm/gtc/noise.hpp"
#include <fstream>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

namespace js = boost::json;

int luaPanic(lua_State *L)
{
    size_t length;
	const char *str = luaL_checklstring(L, -1, &length);
    MAKE_ERROR("LUA PANIC: unprotected error in call to Lua API (" << std::string_view(str, length) <<  ")");

	return 0;
}

namespace LV::Server {

GameServer::GameServer(asio::io_context &ioc, fs::path worldPath)
    : AsyncObject(ioc),
        Content(ioc)
{
    BackingChunkPressure.Threads.resize(4);
    BackingNoiseGenerator.Threads.resize(4);
    BackingAsyncLua.Threads.resize(4);

    init(worldPath);

    BackingChunkPressure.Worlds = &Expanse.Worlds;
    for(size_t iter = 0; iter < BackingChunkPressure.Threads.size(); iter++) {
        BackingChunkPressure.Threads[iter] = std::thread(&BackingChunkPressure_t::run, &BackingChunkPressure, iter);
    }

    for(size_t iter = 0; iter < BackingNoiseGenerator.Threads.size(); iter++) {
        BackingNoiseGenerator.Threads[iter] = std::thread(&BackingNoiseGenerator_t::run, &BackingNoiseGenerator, iter);
    }

    for(size_t iter = 0; iter < BackingAsyncLua.Threads.size(); iter++) {
        BackingAsyncLua.Threads[iter] = std::thread(&BackingAsyncLua_t::run, &BackingAsyncLua, iter);
    }

    // Тест луа


    lua_State *m_luastack = luaL_newstate();
	lua_atpanic(m_luastack, &luaPanic);
    
}
    
GameServer::~GameServer() {
    shutdown("on ~GameServer");
    BackingChunkPressure.NeedShutdown = true;
    BackingChunkPressure.Symaphore.notify_all();
    BackingNoiseGenerator.NeedShutdown = true;
    BackingAsyncLua.NeedShutdown = true;

    RunThread.join();
    WorkDeadline.cancel();
    UseLock.wait_no_use();

    BackingChunkPressure.stop();
    BackingNoiseGenerator.stop();
    BackingAsyncLua.stop();

    LOG.info() << "Сервер уничтожен";
}

void GameServer::BackingChunkPressure_t::run(int id) {
    // static thread_local int local_counter = -1;
    int iteration = 0;
    LOG.debug() << "Старт потока " << id;

    try {
        while(true) {
            // local_counter++;
            // LOG.debug() << "Ожидаю начала " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                Symaphore.wait(lock, [&](){ return iteration != Iteration || NeedShutdown; });
                if(NeedShutdown) {
                    LOG.debug() << "Завершение выполнения потока " << id;
                    break;
                }

                iteration = Iteration;
            }

            assert(RunCollect > 0);
            assert(RunCompress > 0);

            // Сбор данных
            size_t pullSize = Threads.size();
            size_t counter = 0;

            struct Dump {
                std::vector<std::shared_ptr<ContentEventController>> CECs, NewCECs;
                std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> Voxels;
                std::unordered_map<Pos::bvec4u, std::array<Node, 16*16*16>> Nodes;
                uint64_t IsChunkChanged_Nodes, IsChunkChanged_Voxels;
            };

            std::vector<std::pair<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, Dump>>>> dump;

            for(const auto& [worldId, world] : *Worlds) {
                const auto &worldObj = *world;
                std::vector<std::pair<Pos::GlobalRegion, Dump>> dumpWorld;

                for(const auto& [regionPos, region] : worldObj.Regions) {
                    auto& regionObj = *region;
                    if(counter++ % pullSize != id) {
                        continue;
                    }

                    Dump dumpRegion;

                    dumpRegion.CECs = regionObj.CECs;
                    dumpRegion.IsChunkChanged_Voxels = regionObj.IsChunkChanged_Voxels;
                    regionObj.IsChunkChanged_Voxels = 0;
                    dumpRegion.IsChunkChanged_Nodes = regionObj.IsChunkChanged_Nodes;
                    regionObj.IsChunkChanged_Nodes = 0;
                    
                    if(!regionObj.NewCECs.empty()) {
                        dumpRegion.NewCECs = std::move(regionObj.NewCECs);
                        dumpRegion.Voxels = regionObj.Voxels;

                        for(int z = 0; z < 4; z++)
                            for(int y = 0; y < 4; y++)
                                for(int x = 0; x < 4; x++) 
                            {
                                auto &toPtr = dumpRegion.Nodes[Pos::bvec4u(x, y, z)];
                                const Node *fromPtr = regionObj.Nodes[Pos::bvec4u(x, y, z).pack()].data();
                                std::copy(fromPtr, fromPtr+16*16*16, toPtr.data());
                            }
                    } else {
                        if(dumpRegion.IsChunkChanged_Voxels) {
                            for(int index = 0; index < 64; index++) {
                                if(((dumpRegion.IsChunkChanged_Voxels >> index) & 0x1) == 0)
                                    continue;

                                Pos::bvec4u chunkPos;
                                chunkPos.unpack(index);

                                auto voxelIter = regionObj.Voxels.find(chunkPos);
                                if(voxelIter != regionObj.Voxels.end()) {
                                    dumpRegion.Voxels[chunkPos] = voxelIter->second;
                                } else {
                                    dumpRegion.Voxels[chunkPos] = {};
                                }
                            }
                        }

                        if(dumpRegion.IsChunkChanged_Nodes) {
                            for(int index = 0; index < 64; index++) {
                                if(((dumpRegion.IsChunkChanged_Nodes >> index) & 0x1) == 0)
                                    continue;

                                Pos::bvec4u chunkPos;
                                chunkPos.unpack(index);

                                auto &toPtr = dumpRegion.Nodes[chunkPos];
                                const Node *fromPtr = regionObj.Nodes[chunkPos.pack()].data();
                                std::copy(fromPtr, fromPtr+16*16*16, toPtr.data());
                            }
                        }
                    }

                    if(!dumpRegion.CECs.empty()) {
                        dumpWorld.push_back({regionPos, std::move(dumpRegion)});
                    }
                }

                if(!dumpWorld.empty()) {
                    dump.push_back({worldId, std::move(dumpWorld)});
                }
            }

            // Синхронизация
            // LOG.debug() << "Синхронизирую " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                RunCollect -= 1;
                Symaphore.notify_all();
            }

            // Сжатие и отправка игрокам
            struct PostponedV {
                WorldId_t WorldId;
                Pos::GlobalChunk Chunk;
                CompressedVoxels Data;
            };

            struct PostponedN {
                WorldId_t WorldId;
                Pos::GlobalChunk Chunk;
                CompressedNodes Data;
            };

            std::list<std::pair<PostponedV, std::vector<ContentEventController*>>> postponedVoxels;
            std::list<std::pair<PostponedN, std::vector<ContentEventController*>>> postponedNodes;

            std::vector<ContentEventController*> cecs;

            for(auto& [worldId, world] : dump) {
                for(auto& [regionPos, region] : world) {
                    for(auto& [chunkPos, chunk] : region.Voxels) {
                        CompressedVoxels cmp = compressVoxels(chunk);
                        Pos::GlobalChunk chunkPosR = (Pos::GlobalChunk(regionPos) << 2) + chunkPos;

                        for(auto& ptr : region.NewCECs) {
                            bool accepted = ptr->Remote->maybe_prepareChunkUpdate_Voxels(worldId, 
                                    chunkPosR, cmp.Compressed, cmp.Defines);
                            if(!accepted) {
                                cecs.push_back(ptr.get());
                            }
                        }

                        if((region.IsChunkChanged_Voxels >> chunkPos.pack()) & 0x1) {
                            for(auto& ptr : region.CECs) {
                                bool skip = false;
                                for(auto& ptr2 : region.NewCECs) {
                                    if(ptr == ptr2) {
                                        skip = true;
                                        break;
                                    }
                                }

                                if(skip)
                                    continue;

                                bool accepted = ptr->Remote->maybe_prepareChunkUpdate_Voxels(worldId, 
                                        chunkPosR, cmp.Compressed, cmp.Defines);
                                if(!accepted) {
                                    cecs.push_back(ptr.get());
                                }
                            }
                        }

                        if(!cecs.empty()) {
                            postponedVoxels.push_back({{worldId, chunkPosR, std::move(cmp)}, cecs});
                            cecs.clear();
                        }
                    }

                    for(auto& [chunkPos, chunk] : region.Nodes) {
                        CompressedNodes cmp = compressNodes(chunk.data());
                        Pos::GlobalChunk chunkPosR = (Pos::GlobalChunk(regionPos) << 2) + chunkPos;

                        for(auto& ptr : region.NewCECs) {
                            bool accepted = ptr->Remote->maybe_prepareChunkUpdate_Nodes(worldId, 
                                    chunkPosR, cmp.Compressed, cmp.Defines);
                            if(!accepted) {
                                cecs.push_back(ptr.get());
                            }
                        }

                        if((region.IsChunkChanged_Nodes >> chunkPos.pack()) & 0x1) {
                            for(auto& ptr : region.CECs) {
                                bool skip = false;
                                for(auto& ptr2 : region.NewCECs) {
                                    if(ptr == ptr2) {
                                        skip = true;
                                        break;
                                    }
                                }

                                if(skip)
                                    continue;

                                bool accepted = ptr->Remote->maybe_prepareChunkUpdate_Nodes(worldId, 
                                        chunkPosR, cmp.Compressed, cmp.Defines);
                                if(!accepted) {
                                    cecs.push_back(ptr.get());
                                }
                            }
                        }

                        if(!cecs.empty()) {
                            postponedNodes.push_back({{worldId, chunkPosR, std::move(cmp)}, cecs});
                            cecs.clear();
                        }
                    }
                }
            }

            while(!postponedVoxels.empty() || !postponedNodes.empty()) {
                {
                    auto begin = postponedVoxels.begin(), end = postponedVoxels.end();
                    while(begin != end) {
                        auto& [worldId, chunkPos, cmp] = begin->first;
                        for(ContentEventController* cec : begin->second) {
                            bool accepted = cec->Remote->maybe_prepareChunkUpdate_Voxels(worldId, chunkPos, cmp.Compressed, cmp.Defines);
                            if(!accepted)
                                cecs.push_back(cec);
                        }

                        if(cecs.empty()) {
                            begin = postponedVoxels.erase(begin);
                        } else {
                            begin->second = cecs;
                            cecs.clear();
                            begin++;
                        }
                    }
                }

                {
                    auto begin = postponedNodes.begin(), end = postponedNodes.end();
                    while(begin != end) {
                        auto& [worldId, chunkPos, cmp] = begin->first;
                        for(ContentEventController* cec : begin->second) {
                            bool accepted = cec->Remote->maybe_prepareChunkUpdate_Nodes(worldId, chunkPos, cmp.Compressed, cmp.Defines);
                            if(!accepted)
                                cecs.push_back(cec);
                        }

                        if(cecs.empty()) {
                            begin = postponedNodes.erase(begin);
                        } else {
                            begin->second = cecs;
                            cecs.clear();
                            begin++;
                        }
                    }
                }
            }

            // Синхронизация
            // LOG.debug() << "Конец " << id << ' ' << local_counter;
            {
                std::unique_lock<std::mutex> lock(Mutex);
                RunCompress -= 1;
                Symaphore.notify_all();
            }
        }
    } catch(const std::exception& exc) {
        std::unique_lock<std::mutex> lock(Mutex);
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }

    Symaphore.notify_all();
}

void GameServer::BackingNoiseGenerator_t::run(int id) {

    LOG.debug() << "Старт потока " << id;

    try {
        while(true) {
            if(NeedShutdown) {
                LOG.debug() << "Завершение выполнения потока " << id;
                break;
            }

            if(Input.get_read().empty())
                TOS::Time::sleep3(50);

            NoiseKey key;

            {
                auto lock = Input.lock();
                if(lock->empty())
                    continue;

                key = lock->front();
                lock->pop();
            }

            Pos::GlobalNode posNode = key.RegionPos;
            posNode <<= 6;

            std::array<float, 64*64*64> data;
            float *ptr = &data[0];

            for(int z = 0; z < 64; z++)
                for(int y = 0; y < 64; y++)
                for(int x = 0; x < 64; x++, ptr++) {
                    // *ptr = TOS::genRand();
                    *ptr = glm::perlin(glm::vec3(posNode.x+x, posNode.y+y, posNode.z+z) / 16.13f);
                    //*ptr = std::pow(*ptr, 0.75f)*1.5f;
                }

            Output.lock()->push_back({key, std::move(data)});
        }
    } catch(const std::exception& exc) {
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }

}

void GameServer::BackingAsyncLua_t::run(int id) {
    LOG.debug() << "Старт потока " << id;

    BackingNoiseGenerator_t::NoiseKey key;
    std::array<float, 64*64*64> noise;
    World::RegionIn out;

    try {
        while(true) {
            if(NeedShutdown) {
                LOG.debug() << "Завершение выполнения потока " << id;
                break;
            }

            if(NoiseIn.get_read().empty())
                TOS::Time::sleep3(50);

            {
                auto lock = NoiseIn.lock();
                if(lock->empty())
                    continue;

                key = lock->front().first;
                noise = lock->front().second;
                lock->pop();
            }

            //if(key.RegionPos == Pos::GlobalRegion(0, 0, 0))
            {
                float *ptr = noise.data();
                for(int z = 0; z < 64; z++)
                    for(int y = 0; y < 64; y++)
                        for(int x = 0; x < 64; x++, ptr++) {
                            DefVoxelId_t id = std::clamp(*ptr, 0.f, 1.f) * 3; //> 0.9 ? 1 : 0;
                            Pos::bvec64u nodePos(x, y, z);
                            auto &node = out.Nodes[Pos::bvec4u(nodePos >> 4).pack()][Pos::bvec16u(nodePos & 0xf).pack()];
                            node.NodeId = id;
                            node.Meta = 0;
                            
                            if(x == 0 && z == 0)
                                node.NodeId = 1;
                            else if(y == 0 && z == 0)
                                node.NodeId = 2;
                            else if(x == 0 && y == 0)
                                node.NodeId = 3;

                            if(y == 1 && z == 0)
                                node.NodeId = 0;
                            else if(x == 0 && y == 1)
                                node.NodeId = 0;

                            // node.Meta = 0;
                        }
            } 
            // else {
            //     Node *ptr = (Node*) &out.Nodes[0][0];
            //     Node node;
            //     node.Data = 0;
            //     std::fill(ptr, ptr+64*64*64, node);
            // }

            RegionOut.lock()->push_back({key, out});
        }
    } catch(const std::exception& exc) {
        NeedShutdown = true;
        LOG.error() << "Ошибка выполнения потока " << id << ":\n" << exc.what();
    }
}

static thread_local std::vector<ContentViewCircle> TL_Circles;

std::vector<ContentViewCircle> GameServer::Expanse_t::accumulateContentViewCircles(ContentViewCircle circle, int depth)
{
    TL_Circles.clear();
    TL_Circles.reserve(64);
    TL_Circles.push_back(circle);
    _accumulateContentViewCircles(circle, depth);
    return TL_Circles;
}

void GameServer::Expanse_t::_accumulateContentViewCircles(ContentViewCircle circle, int depth) {
    for(const auto &pair : ContentBridges) {
        auto &br = pair.second;
        if(br.LeftWorld == circle.WorldId) {
            glm::i32vec3 vec = circle.Pos-br.LeftPos;
            ContentViewCircle circleNew = {br.RightWorld, br.RightPos, static_cast<int16_t>(circle.Range-int16_t(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z))};

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
            ContentViewCircle circleNew = {br.LeftWorld, br.LeftPos, static_cast<int16_t>(circle.Range-int16_t(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z))};

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


ContentViewInfo GameServer::Expanse_t::makeContentViewInfo(const std::vector<ContentViewCircle> &views) {
    ContentViewInfo cvi;

    for(const ContentViewCircle &circle : views) {
        std::vector<Pos::GlobalRegion> &cvw = cvi.Regions[circle.WorldId];
        int32_t regionRange = std::sqrt(circle.Range);

        cvw.reserve(cvw.size()+std::pow(regionRange*2+1, 3));

        for(int32_t z = -regionRange; z <= regionRange; z++)
            for(int32_t y = -regionRange; y <= regionRange; y++)
                for(int32_t x = -regionRange; x <= regionRange; x++)
                    cvw.push_back(Pos::GlobalRegion(x, y, z)+circle.Pos);
    }

    for(auto& [worldId, regions] : cvi.Regions) {
        std::sort(regions.begin(), regions.end());
        auto eraseIter = std::unique(regions.begin(), regions.end());
        regions.erase(eraseIter, regions.end());
        regions.shrink_to_fit();
    }

    return cvi;
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
            // Считываем ресурсы хранимые в кеше клиента
            uint32_t count = co_await Net::AsyncSocket::read<uint32_t>(socket);
            if(count > 262144)
                MAKE_ERROR("Не поддерживаемое количество ресурсов в кеше у клиента");
            
            std::vector<Hash_t> clientCache;
            clientCache.resize(count);
            co_await Net::AsyncSocket::read(socket, (std::byte*) clientCache.data(), count*32);
            std::sort(clientCache.begin(), clientCache.end());

            External.NewConnectedPlayers.lock_write()
               ->push_back(std::make_unique<RemoteClient>(IOC, std::move(socket), username, std::move(clientCache)));
        }
    }
}

TexturePipeline GameServer::buildTexturePipeline(const std::string& pl) {
    /* 
        ^ объединение текстур, вторая поверх первой. 
        При наложении текстуры будут автоматически увеличины до размера
        самой большой текстуры из участвующих. По умолчанию ближайший соседний
        default:dirt.png^our_tech:machine.png

        Текстурные команды описываются в [] <- предоставляет текстуру.
        Разделитель пробелом
        default:dirt.png^[create 2 2 r ffaabbcc]

        Если перед командой будет использован $, то первым аргументом будет
        предыдущая текстура, если это поддерживает команда
        default:dirt.png$[resize 16 16] или [resize default:dirt.png 16 16]

        Группировка ()
        default:empty^(default:dirt.png^our_tech:machine.png)
    */

    std::map<std::string, BinTextureId_t> stbt;
    std::unordered_set<BinTextureId_t> btis;
    std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    // Парсер группы. Возвращает позицию на которой закончил и скомпилированный код
    std::move_only_function<std::tuple<size_t, std::u8string>(size_t pos)> parse_obj;
    std::move_only_function<std::tuple<size_t, std::u8string>(size_t pos, std::u8string maybe)> parse_cmd;

    parse_cmd = [&](size_t pos, std::u8string maybe) -> std::tuple<size_t, std::u8string> {
        std::string cmd_name;
        std::vector<std::u8string> args;
        size_t startPos = pos;

        for(pos++; pos < pl.size(); pos++) {
            if(pl[pos] == ']') {
                // Команда завершилась
                // return {pos+1, cmd_name};
            } else if(pl[pos] == ' ') {
                // Аргументы
                // Здесь нужно получить либо кастомные значения, либо объект
                auto [next_pos, subcmd] = parse_obj(pos+1);
                args.push_back(subcmd);
                if(next_pos == pl.size())
                    MAKE_ERROR("Ожидался конец команды объявленной на " << startPos << ", наткнулись на конец потока");
            
                pos = next_pos-1;
            } else if(alpha.find(pl[pos]) == std::string::npos) {
                MAKE_ERROR("Ошибка в имени команды");
            } else {
                cmd_name += pl[pos];
            }
        }

        MAKE_ERROR("Ожидался конец команды объявленной на " << startPos << ", наткнулись на конец потока");
    };
    
    parse_obj = [&](size_t pos) -> std::pair<size_t, std::u8string> {
        std::u8string out;

        for(; pos < pl.size(); pos++) {
            if(pl[pos] == '[') {
                // Начало команды
                if(!out.empty()) {
                    MAKE_ERROR("Отсутствует связь между текстурой и текущей командой " << pos);
                }

                // out.push_back(TexturePipelineCMD::Combine);
                auto [next_size, subcmd] = parse_cmd(pos+1, {});
                pos = next_size-1;
                out = subcmd;
            } else if(pl[pos] == '^') {
                // Объединение
                if(out.empty()) {
                    MAKE_ERROR("Отсутствует текстура для комбинирования " << pos);

                    auto [next_pos, subcmd] = parse_obj(pos+1);
                    std::u8string cmd;
                    cmd.push_back(uint8_t(TexturePipelineCMD::Combine));
                    cmd.insert(cmd.end(), out.begin(), out.end());
                    cmd.insert(cmd.end(), subcmd.begin(), subcmd.end());

                    return {next_pos, cmd};
                }
            } else if(pl[pos] == '$') {
                // Готовый набор команд будет использован как аргумент
                pos++;
                if(pos >= pl.size() || pl[pos] != '[')
                    MAKE_ERROR("Ожидалось объявление команды " << pos);
                auto [next_pos, subcmd] = parse_cmd(pos, out);
                pos = next_pos-1;
                out = subcmd;
            } else if(pl[pos] == '(') {
                if(!out.empty()) {
                    MAKE_ERROR("Начато определение группы после текстуры, вероятно пропущен знак объединения ^ " << pos);
                }

                // Начало группы
                auto [next_pos, subcmd] = parse_obj(pos+1);
                pos = next_pos-1;
                out = subcmd;
            } else if(pl[pos] == ')') {
                return {pos+1, out};
            } else {
                // Это текстура, нужно её имя
                if(!out.empty())
                    MAKE_ERROR("Отсутствует связь между текстурой и текущим объявлением текстуры " << pos);

                out.push_back(uint8_t(TexturePipelineCMD::Texture));
                std::string texture_name;
                for(; pos < pl.size(); pos++) {
                    if(pl[pos] == '^' || pl[pos] == ')' || pl[pos] == ']')
                        break;
                    else if(pl[pos] != '.' && pl[pos] != ':' && alpha.find_first_of(pl[pos]) != std::string::npos)
                        MAKE_ERROR("Недействительные символы в объявлении текстуры " << pos);
                    else
                        texture_name += pl[pos];
                }

                BinTextureId_t id = stbt[texture_name];
                btis.insert(id);

                for(int iter = 0; iter < 4; iter++)
                    out.push_back((id >> (iter * 8)) & 0xff);

                if(pos < pl.size())
                    pos--;
            }
        }

        return {pos, out};
    };

    auto [pos, cmd] = parse_obj(0);

    if(pos < pl.size()) {
        MAKE_ERROR("Неожиданное продолжение " << pos);
    }

    return {std::vector<BinTextureId_t>(btis.begin(), btis.end()), cmd};
}

std::string GameServer::deBuildTexturePipeline(const TexturePipeline& pipeline) {
    return "";
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
            for(std::shared_ptr<ContentEventController> &cec : Game.CECs) {
                cec->Remote->shutdown(EnumDisconnect::ByInterface, ShutdownReason);
            }

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

        stepConnections();
        stepModInitializations();
        IWorldSaveBackend::TickSyncInfo_Out dat1 = stepDatabaseSync();
        stepGeneratorAndLuaAsync(std::move(dat1));
        stepPlayerProceed();
        stepWorldPhysic();
        stepGlobalStep();
        stepSyncContent();

        // Прочие моменты
        if(!IsGoingShutdown) {
            if(BackingChunkPressure.NeedShutdown
                || BackingNoiseGenerator.NeedShutdown)
            {
                LOG.error() << "Ошибка работы одного из модулей";
                IsGoingShutdown = true;
            }
        }

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

std::vector<GameServer::ModInfo> GameServer::readModDataPath(const fs::path& modsDir) {
    if(!fs::exists(modsDir))
        return {};
    
    std::vector<GameServer::ModInfo> infos;

    try {
        fs::directory_iterator begin(modsDir), end;
        for(; begin != end; begin++) {
            if(!begin->is_directory())
                continue;

            fs::path mod_conf = begin->path() / "mod.json";
            if(!fs::exists(mod_conf)) {
                LOG.debug() << "Директория в папке с модами не содержит файл mod.json: " << begin->path().filename();
                continue;
            }

            try {
                std::ifstream fd(mod_conf);
                js::object obj = js::parse(fd).as_object();

                GameServer::ModInfo info;

                info.Id = obj.at("Id").as_string();
                info.Title = obj.contains("Title") ? obj["Title"].as_string() : "";
                info.Description = obj.contains("Description") ? obj["Description"].as_string() : "";

                if(obj.contains("Dependencies")) {
                    js::array arr = obj["Dependencies"].as_array();
                    for(auto& iter : arr) {
                        info.Dependencies.push_back((std::string) iter.as_string());
                    }
                }

                if(obj.contains("OptionalDependencies")) {
                    js::array arr = obj["OptionalDependencies"].as_array();
                    for(auto& iter : arr) {
                        info.OptionalDependencies.push_back((std::string) iter.as_string());
                    }
                }

            } catch(const std::exception &exc) {
                LOG.warn() << "Не удалось прочитать " << mod_conf.string();
            }
        }
    } catch(const std::exception &exc) {
        LOG.warn() << "Не удалось прочитать моды из директории " << modsDir.string() << "\n" << exc.what();
    }


    return infos;
}

void GameServer::stepConnections() {
    // Подключить новых игроков
    if(!External.NewConnectedPlayers.no_lock_readable().empty()) {
        auto lock = External.NewConnectedPlayers.lock_write();

        for(std::unique_ptr<RemoteClient>& client : *lock) {
            co_spawn(client->run());
            Game.CECs.push_back(std::make_unique<ContentEventController>(std::move(client)));
        }

        lock->clear();
    }

    BackingChunkPressure.endCollectChanges();

    // Отключение игроков
    for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
        // Убрать отключившихся
        if(!cec->Remote->isConnected()) {
            // Отписываем наблюдателя от миров
            for(auto wPair : cec->ContentViewState.Regions) {
                auto wIter = Expanse.Worlds.find(wPair.first);
                assert(wIter != Expanse.Worlds.end());

                wIter->second->onCEC_RegionsLost(cec, wPair.second);
            }

            std::string username = cec->Remote->Username;
            External.ConnectedPlayersSet.lock_write()->erase(username);
            
            cec.reset();
        }
    }

    // Вычистить невалидные ссылки на игроков
    Game.CECs.erase(std::remove_if(Game.CECs.begin(), Game.CECs.end(),
            [](const std::shared_ptr<ContentEventController>& ptr) { return !ptr; }), 
        Game.CECs.end());
}

void GameServer::stepModInitializations() {
    BackingChunkPressure.endWithResults();
}

IWorldSaveBackend::TickSyncInfo_Out GameServer::stepDatabaseSync() {
    IWorldSaveBackend::TickSyncInfo_In toDB;
    
    for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
        assert(cec);
        // Пересчитать зоны наблюдения
        if(cec->CrossedBorder) {
            cec->CrossedBorder = false;
            
            // Пересчёт зон наблюдения
            ServerObjectPos oPos = cec->getPos();

            ContentViewCircle cvc;
            cvc.WorldId = oPos.WorldId;
            cvc.Pos = Pos::Object_t::asRegionsPos(oPos.ObjectPos);
            cvc.Range = 2;

            std::vector<ContentViewCircle> newCVCs = Expanse.accumulateContentViewCircles(cvc);
            ContentViewInfo newCbg = Expanse_t::makeContentViewInfo(newCVCs);

            ContentViewInfo_Diff diff = newCbg.diffWith(cec->ContentViewState);
            if(!diff.WorldsNew.empty()) {
                // Сообщить о новых мирах
                for(const WorldId_t id : diff.WorldsNew) {
                    auto iter = Expanse.Worlds.find(id);
                    assert(iter != Expanse.Worlds.end());

                    cec->onWorldUpdate(id, iter->second.get());
                }
            }

            cec->ContentViewState = newCbg;
            // Вычистка не наблюдаемых регионов
            cec->removeUnobservable(diff);

            // Подписываем игрока на наблюдение за регионами
            for(const auto& [worldId, regions] : diff.RegionsNew) {
                auto iterWorld = Expanse.Worlds.find(worldId);
                assert(iterWorld != Expanse.Worlds.end());

                std::vector<Pos::GlobalRegion> notLoaded = iterWorld->second->onCEC_RegionsEnter(cec, regions);
                if(!notLoaded.empty()) {
                    // Добавляем к списку на загрузку
                    std::vector<Pos::GlobalRegion> &tl = toDB.Load[worldId];
                    tl.insert(tl.end(), notLoaded.begin(), notLoaded.end());
                }
            }

            // Отписываем то, что игрок больше не наблюдает
            for(const auto& [worldId, regions] : diff.RegionsLost) {
                auto iterWorld = Expanse.Worlds.find(worldId);
                assert(iterWorld != Expanse.Worlds.end());

                iterWorld->second->onCEC_RegionsLost(cec, regions);
            }
        }
    }

    for(auto& [worldId, regions] : toDB.Load) {
        std::sort(regions.begin(), regions.end());
        auto eraseIter = std::unique(regions.begin(), regions.end());
        regions.erase(eraseIter, regions.end());
    }

    // Обзавелись списком на прогрузку регионов
    // Теперь узнаем что нужно сохранить и что из регионов было выгружено
    for(auto& [worldId, world] : Expanse.Worlds) {
        World::SaveUnloadInfo info = world->onStepDatabaseSync();
        
        if(!info.ToSave.empty()) {
            auto &obj = toDB.ToSave[worldId];
            obj.insert(obj.end(), std::make_move_iterator(info.ToSave.begin()), std::make_move_iterator(info.ToSave.end()));
        }

        if(!info.ToUnload.empty()) {
            auto &obj = toDB.Unload[worldId];
            obj.insert(obj.end(), info.ToUnload.begin(), info.ToUnload.end());
        }
    }

    // Синхронизируемся с базой
    return SaveBackend.World->tickSync(std::move(toDB));
}

void GameServer::stepGeneratorAndLuaAsync(IWorldSaveBackend::TickSyncInfo_Out db) {
    // 1. Получили сырые регионы и те регионы, что не существуют
    // 2.1 Те регионы, что не существуют отправляются на расчёт шума
    // 2.2 Далее в луа для обработки шума
    // 3.1 Нужно прогнать идентификаторы через обработчики lua
    // 3.2 Полученный регион связать с существующими профилями сервера
    // 4. Полученные регионы раздать мирам и попробовать по новой подписать к ним игроков, если они всё ещё должны наблюдать эти регионы


    // Синхронизация с генератором шума
    
    std::unordered_map<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, World::RegionIn>>> toLoadRegions;

    // Синхронизация с контроллером асинхронных обработчиков луа
    // 2.2 и 3.1
    // Обработка шума на стороне луа
    {
        std::vector<std::pair<BackingNoiseGenerator_t::NoiseKey, std::array<float, 64*64*64>>> calculatedNoise = BackingNoiseGenerator.tickSync(std::move(db.NotExisten));
        if(!calculatedNoise.empty()) {
            auto lock = BackingAsyncLua.NoiseIn.lock();

            for(auto& pair : calculatedNoise)
                lock->push(pair);
        }

        calculatedNoise.clear();

        if(!BackingAsyncLua.RegionOut.get_read().empty()) {
            std::vector<
                std::pair<BackingNoiseGenerator_t::NoiseKey, World::RegionIn>
            > toLoad = std::move(*BackingAsyncLua.RegionOut.lock());
            
            for(auto& [key, region] : toLoad) {
                toLoadRegions[key.WId].push_back({key.RegionPos, region});
            }
        }
    }

    // Обработка идентификаторов на стороне луа

    // Трансформация полученных ключей в профили сервера
    for(auto& [WorldId_t, regions] : db.LoadedRegions) {
        auto &list = toLoadRegions[WorldId_t];

        for(auto& [pos, region] : regions) {
            auto &obj = list.emplace_back(pos, World::RegionIn()).second;
            convertRegionVoxelsToChunks(region.Voxels, obj.Voxels);
            obj.Nodes = std::move(region.Nodes);
            obj.Entityes = std::move(region.Entityes);
        }
    }

    // Раздадим полученные регионы мирам и попробуем подписать на них наблюдателей
    for(auto& [worldId, regions] : toLoadRegions) {
        auto iterWorld = Expanse.Worlds.find(worldId);
        assert(iterWorld != Expanse.Worlds.end());

        std::vector<Pos::GlobalRegion> newRegions;
        newRegions.reserve(regions.size());
        for(auto& [pos, _] : regions)
            newRegions.push_back(pos);
        std::sort(newRegions.begin(), newRegions.end());

        std::unordered_map<std::shared_ptr<ContentEventController>, std::vector<Pos::GlobalRegion>> toSubscribe;
        
        for(auto& cec : Game.CECs) {
            auto iterViewWorld = cec->ContentViewState.Regions.find(worldId);
            if(iterViewWorld == cec->ContentViewState.Regions.end())
                continue;

            for(auto& pos : iterViewWorld->second) {
                if(std::binary_search(newRegions.begin(), newRegions.end(), pos))
                    toSubscribe[cec].push_back(pos);
            }
        }

        iterWorld->second->pushRegions(std::move(regions));
        for(auto& [cec, poses] : toSubscribe) {
            iterWorld->second->onCEC_RegionsEnter(cec, poses);
        }
    }
}

void GameServer::stepPlayerProceed() {

}

void GameServer::stepWorldPhysic() {
    // Максимальная скорость в обсчёте за такт половина максимального размера объекта
    // По всем объектам в регионе расчитывается максимальный размео по оси, делённый на линейную скорость
    // Выбирается наибольшая скорость. Если скорость превышает максимальную за раз, 
    // то физика в текущем такте рассчитывается в несколько проходов

    
    // for(auto &pWorld : Expanse.Worlds) {
    //     World &wobj = *pWorld.second;
       
    //     assert(pWorld.first == 0 && "Требуется WorldManager");

    //     std::string worldStringId = "unexisten";

    //     std::vector<Pos::GlobalRegion> regionsToRemove;
    //     for(auto &pRegion : wobj.Regions) {
    //         Region &region = *pRegion.second;

    //         // Позиции исчисляются в целых числах
    //         // Вместо умножения на dtime, используется *dTimeMul/dTimeDiv
    //         int32_t dTimeDiv = Pos::Object_t::BS;
    //         int32_t dTimeMul = dTimeDiv*CurrentTickDuration;

    //         // Обновить сущностей
    //         for(size_t entityIndex = 0; entityIndex < region.Entityes.size(); entityIndex++) {
    //             Entity &entity = region.Entityes[entityIndex];

    //             if(entity.IsRemoved)
    //                 continue;

    //             // Если нет ни скорости, ни ускорения, то пропускаем расчёт
    //             if((entity.Speed.x != 0 || entity.Speed.y != 0 || entity.Speed.z != 0)
    //                     || (entity.Acceleration.x != 0 || entity.Acceleration.y != 0 || entity.Acceleration.z != 0))
    //             {
    //                 Pos::Object &eSpeed = entity.Speed;

    //                 // Ограничение на 256 м/с
    //                 static constexpr int32_t MAX_SPEED_PER_SECOND = 256*Pos::Object_t::BS;
    //                 {
    //                     uint32_t linearSpeed = std::sqrt(eSpeed.x*eSpeed.x + eSpeed.y*eSpeed.y + eSpeed.z*eSpeed.z);

    //                     if(linearSpeed > MAX_SPEED_PER_SECOND) {
    //                         eSpeed *= MAX_SPEED_PER_SECOND;
    //                         eSpeed /= linearSpeed;
    //                     }

    //                     Pos::Object &eAcc = entity.Acceleration;
    //                     linearSpeed = std::sqrt(eAcc.x*eAcc.x + eAcc.y*eAcc.y + eAcc.z*eAcc.z);

    //                     if(linearSpeed > MAX_SPEED_PER_SECOND/2) {
    //                         eAcc *= MAX_SPEED_PER_SECOND/2;
    //                         eAcc /= linearSpeed;
    //                     }
    //                 }

    //                 // Потенциальное изменение позиции сущности в пустой области
    //                 // vt+(at^2)/2 = (v+at/2)*t = (Скорость + Ускорение/2*dtime)*dtime
    //                 Pos::Object dpos = (eSpeed + entity.Acceleration/2*dTimeMul/dTimeDiv)*dTimeMul/dTimeDiv;
    //                 // Стартовая и конечная позиции
    //                 Pos::Object &startPos = entity.Pos, endPos = entity.Pos + dpos;
    //                 // Новая скорость
    //                 Pos::Object nSpeed = entity.Speed + entity.Acceleration*dTimeMul/dTimeDiv;

    //                 // Зона расчёта коллизии
    //                 AABB collideZone = {startPos, endPos};
    //                 collideZone.sortMinMax();
    //                 collideZone.VecMin -= Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);
    //                 collideZone.VecMax += Pos::Object(entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z)/2+Pos::Object(1);

    //                 // Сбор ближайших коробок
    //                 std::vector<CollisionAABB> Boxes;

    //                 {
    //                     glm::ivec3 beg = collideZone.VecMin >> 20, end = (collideZone.VecMax + 0xfffff) >> 20;

    //                     for(; beg.z <= end.z; beg.z++)
    //                     for(; beg.y <= end.y; beg.y++)
    //                     for(; beg.x <= end.x; beg.x++) {
    //                         Pos::GlobalRegion rPos(beg.x, beg.y, beg.z);
    //                         auto iterChunk = wobj.Regions.find(rPos);
    //                         if(iterChunk == wobj.Regions.end())
    //                             continue;

    //                         iterChunk->second->getCollideBoxes(rPos, collideZone, Boxes);
    //                     }
    //                 }

    //                 // Коробка сущности
    //                 AABB entityAABB = entity.aabbAtPos();

    //                 // Симулируем физику
    //                 // Оставшееся время симуляции
    //                 int32_t remainingSimulationTime = dTimeMul;
    //                 // Оси, по которым было пересечение
    //                 bool axis[3]; // x y z

    //                 // Симулируем пока не будет просчитано выделенное время
    //                 while(remainingSimulationTime > 0) {
    //                     if(nSpeed.x == 0 && nSpeed.y == 0 && nSpeed.z == 0)
    //                         break; // Скорости больше нет

    //                     entityAABB = entity.aabbAtPos();

    //                     // Ближайшее время пересечения с боксом
    //                     int32_t minSimulationTime = remainingSimulationTime;
    //                     // Ближайший бокс в пересечении
    //                     int nearest_boxindex = -1;

    //                     for(size_t index = 0; index < Boxes.size(); index++) {
    //                         CollisionAABB &caabb = Boxes[index];

    //                         if(caabb.Skip)
    //                             continue;

    //                         int32_t delta;
    //                         if(!entityAABB.collideWithDelta(caabb, nSpeed, delta, axis))
    //                             continue;

    //                         if(delta > remainingSimulationTime)
    //                             continue;

    //                         nearest_boxindex = index;
    //                         minSimulationTime = delta;
    //                     }

    //                     if(nearest_boxindex == -1) {
    //                         // Свободный ход
    //                         startPos += nSpeed*dTimeDiv/minSimulationTime;
    //                         remainingSimulationTime = 0;
    //                         break;
    //                     } else {
    //                         if(minSimulationTime == 0) {
    //                             // Уже где-то застряли
    //                             // Да и хрен бы с этим
    //                         } else {
    //                             // Где-то встрянем через minSimulationTime
    //                             startPos += nSpeed*dTimeDiv/minSimulationTime;
    //                             remainingSimulationTime -= minSimulationTime;

    //                             nSpeed.x = nSpeed.y = nSpeed.z = 0;
    //                             break;
    //                         }

    //                         if(axis[0] == 0) {
    //                             nSpeed.x = 0;
    //                         }
                            
    //                         if(axis[1] == 0) {
    //                             nSpeed.y = 0;
    //                         } 
                            
    //                         if(axis[2] == 0) {
    //                             nSpeed.z = 0;
    //                         }

    //                         CollisionAABB &caabb = Boxes[nearest_boxindex];
    //                         caabb.Skip = true;
    //                     }
    //                 }

    //                 // Симуляция завершена
    //             }

    //             // Сущность будет вычищена
    //             if(entity.NeedRemove) {
    //                 entity.NeedRemove = false;
    //                 entity.IsRemoved = true;
    //             }

    //             // Проверим необходимость перемещения сущности в другой регион
    //             // Вынести в отдельный этап обновления сервера, иначе будут происходить двойные симуляции
    //             // сущностей при пересечении регионов/миров
    //             {
    //                 Pos::Object temp = entity.Pos >> 20;
    //                 Pos::GlobalRegion rPos(temp.x, temp.y, temp.z);

    //                 if(rPos != pRegion.first || pWorld.first != entity.WorldId) {

    //                     Region *toRegion = forceGetRegion(entity.WorldId, rPos);
    //                     RegionEntityId_t newId = toRegion->pushEntity(entity);
    //                     // toRegion->Entityes[newId].WorldId = Если мир изменился

    //                     if(newId == RegionEntityId_t(-1)) {
    //                         // В другом регионе нет места
    //                     } else {
    //                         entity.IsRemoved = true;
    //                         // Сообщаем о перемещении сущности
    //                         for(ContentEventController *cec : region.CECs) {
    //                             cec->onEntitySwap(pWorld.first, pRegion.first, entityIndex, entity.WorldId, rPos, newId);
    //                         }
    //                     }
    //                 }
    //             }
    //         }

    //         // Проверить необходимость перерасчёта вертикальной проходимости света 
    //         // std::unordered_map<Pos::bvec4u, const LightPrism*> ChangedLightPrism;
    //         // {
    //         //     for(int big = 0; big < 64; big++) {
    //         //         uint64_t bits = region.IsChunkChanged_Voxels[big] | region.IsChunkChanged_Nodes[big];

    //         //         if(!bits)
    //         //             continue;

    //         //         for(int little = 0; little < 64; little++) {
    //         //             if(((bits >> little) & 1) == 0)
    //         //                 continue;

                        
    //         //         }
    //         //     }
    //         // }

    //         // Сбор данных об изменившихся чанках
    //         std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> ChangedVoxels;
    //         std::unordered_map<Pos::bvec4u, const Node*> ChangedNodes;
    //         {
    //             if(!region.IsChunkChanged_Voxels && !region.IsChunkChanged_Nodes)
    //                 continue;

    //             for(int index = 0; index < 64; index++) {
    //                 Pos::bvec4u pos;
    //                 pos.unpack(index);

    //                 if(((region.IsChunkChanged_Voxels >> index) & 1) == 1) {
    //                     auto iter = region.Voxels.find(pos);
    //                     assert(iter != region.Voxels.end());
    //                     ChangedVoxels[pos] = &iter->second;
    //                 }

    //                 if(((region.IsChunkChanged_Nodes >> index) & 1) == 1) {
    //                     ChangedNodes[pos] = (Node*) &region.Nodes[0][0][0][pos.x][pos.y][pos.z];
    //                 }
    //             }
    //         }

    //         // Об изменившихся сущностях
    //         {

    //         }

    //         if(++region.CEC_NextChunkAndEntityesViewCheck >= region.CECs.size())
    //             region.CEC_NextChunkAndEntityesViewCheck = 0;

    //         // Пробегаемся по всем наблюдателям
    //         {
    //             size_t cecIndex = 0;
    //             for(ContentEventController *cec : region.CECs) {
    //                 cecIndex++;

    //                 auto cvwIter = cec->ContentViewState.find(pWorld.first);
    //                 if(cvwIter == cec->ContentViewState.end())
    //                     // Мир не отслеживается
    //                     continue;


    //                 const ContentViewWorld &cvw = cvwIter->second;
    //                 auto chunkBitsetIter = cvw.find(pRegion.first);
    //                 if(chunkBitsetIter == cvw.end())
    //                     // Регион не отслеживается
    //                     continue;

    //                 // Наблюдаемые чанки
    //                 const std::bitset<64> &chunkBitset = chunkBitsetIter->second;

    //                 // Пересылка изменений в регионе
    //                 // if(!ChangedLightPrism.empty())
    //                 //     cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, ChangedLightPrism);

    //                 if(!ChangedVoxels.empty())
    //                     cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, ChangedVoxels);

    //                 if(!ChangedNodes.empty())
    //                     cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, ChangedNodes);

    //                 // Отправка полной информации о новых наблюдаемых чанках
    //                 {
    //                         //std::unordered_map<Pos::bvec4u, const LightPrism*> newLightPrism;
    //                         std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> newVoxels;
    //                         std::unordered_map<Pos::bvec4u, const Node*> newNodes;

    //                         //newLightPrism.reserve(new_chunkBitset->count());
    //                         newVoxels.reserve(new_chunkBitset->count());
    //                         newNodes.reserve(new_chunkBitset->count());

    //                         size_t bitPos = new_chunkBitset->_Find_first();
    //                         while(bitPos != new_chunkBitset->size()) {
    //                             Pos::bvec4u chunkPos;
    //                             chunkPos = bitPos;

    //                             //newLightPrism.insert({chunkPos, &region.Lights[0][0][chunkPos.X][chunkPos.Y][chunkPos.Z]});
    //                             newVoxels.insert({chunkPos, &region.Voxels[chunkPos]});
    //                             newNodes.insert({chunkPos, &region.Nodes[0][0][0][chunkPos.x][chunkPos.y][chunkPos.z]});

    //                             bitPos = new_chunkBitset->_Find_next(bitPos);
    //                         }

    //                         //cec->onChunksUpdate_LightPrism(pWorld.first, pRegion.first, newLightPrism);
    //                         cec->onChunksUpdate_Voxels(pWorld.first, pRegion.first, newVoxels);
    //                         cec->onChunksUpdate_Nodes(pWorld.first, pRegion.first, newNodes);
                        
    //                 }

    //                 // То, что уже отслеживает наблюдатель
    //                 const auto &subs = cec->getSubscribed();

    //                 // // Проверка отслеживания сущностей
    //                 // if(cecIndex-1 == region.CEC_NextChunkAndEntityesViewCheck) {
    //                 //     std::vector<LocalEntityId_t> newEntityes, lostEntityes;
    //                 //     for(size_t iter = 0; iter < region.Entityes.size(); iter++) {
    //                 //         Entity &entity = region.Entityes[iter];

    //                 //         if(entity.IsRemoved)
    //                 //             continue;

    //                 //         for(const ContentViewCircle &circle : cvc->second) {
    //                 //             int x = entity.ABBOX.x >> 17;
    //                 //             int y = entity.ABBOX.y >> 17;
    //                 //             int z = entity.ABBOX.z >> 17;

    //                 //             uint32_t size = 0;
    //                 //             if(circle.isIn(entity.Pos, x*x+y*y+z*z))
    //                 //                 newEntityes.push_back(iter);
    //                 //         }
    //                 //     }

    //                 //     std::unordered_set<LocalEntityId_t> newEntityesSet(newEntityes.begin(), newEntityes.end());

    //                 //     {
    //                 //         auto iterR_W = subs.Entities.find(pWorld.first);
    //                 //         if(iterR_W == subs.Entities.end())
    //                 //             // Если мир не отслеживается наблюдателем
    //                 //             goto doesNotObserveEntityes;

    //                 //         auto iterR_W_R = iterR_W->second.find(pRegion.first);
    //                 //         if(iterR_W_R == iterR_W->second.end())
    //                 //             // Если регион не отслеживается наблюдателем
    //                 //             goto doesNotObserveEntityes;

    //                 //         // Подходят ли уже наблюдаемые сущности под наблюдательные области
    //                 //         for(LocalEntityId_t eId : iterR_W_R->second) {
    //                 //             if(eId >= region.Entityes.size()) {
    //                 //                 lostEntityes.push_back(eId);
    //                 //                 break;
    //                 //             }

    //                 //             Entity &entity = region.Entityes[eId];

    //                 //             if(entity.IsRemoved) {
    //                 //                 lostEntityes.push_back(eId);
    //                 //                 break;
    //                 //             }

    //                 //             int x = entity.ABBOX.x >> 17;
    //                 //             int y = entity.ABBOX.y >> 17;
    //                 //             int z = entity.ABBOX.z >> 17;

    //                 //             for(const ContentViewCircle &circle : cvc->second) {
    //                 //                 if(!circle.isIn(entity.Pos, x*x+y*y+z*z))
    //                 //                     lostEntityes.push_back(eId);
    //                 //             }
    //                 //         }

    //                 //         // Удалим чанки которые наблюдатель уже видит
    //                 //         for(LocalEntityId_t eId : iterR_W_R->second)
    //                 //             newEntityesSet.erase(eId);
    //                 //     }

    //                 //     doesNotObserveEntityes:

    //                 //     cec->onEntityEnterLost(pWorld.first, pRegion.first, newEntityesSet, std::unordered_set<LocalEntityId_t>(lostEntityes.begin(), lostEntityes.end()));
    //                 //     // Отправить полную информацию о новых наблюдаемых сущностях наблюдателю
    //                 // }

    //                 if(!region.Entityes.empty()) {
    //                     std::unordered_map<RegionEntityId_t, Entity*> entities;
    //                     for(size_t iter = 0; iter < region.Entityes.size(); iter++)
    //                         entities[iter] = &region.Entityes[iter];
    //                     cec->onEntityUpdates(pWorld.first, pRegion.first, entities);
    //                 }
    //             }
    //         }


    //         // Сохраняем регионы
    //         region.LastSaveTime += CurrentTickDuration;

    //         bool needToUnload = region.CECs.empty() && region.LastSaveTime > 60;
    //         bool needToSave = region.IsChanged && region.LastSaveTime > 15;

    //         if(needToUnload || needToSave) {
    //             region.LastSaveTime = 0;
    //             region.IsChanged = false;

    //             SB_Region data;
    //             convertChunkVoxelsToRegion(region.Voxels, data.Voxels);
    //             SaveBackend.World->save(worldStringId, pRegion.first, &data);
    //         }

    //         // Выгрузим регионы
    //         if(needToUnload) {
    //             regionsToRemove.push_back(pRegion.first);
    //         }

    //         // Сброс информации об изменившихся данных
    //         region.IsChunkChanged_Voxels = 0;
    //         region.IsChunkChanged_Nodes = 0;
    //     }

    //     for(Pos::GlobalRegion regionPos : regionsToRemove) {
    //         auto iter = wobj.Regions.find(regionPos);
    //         if(iter == wobj.Regions.end())
    //             continue;

    //         wobj.Regions.erase(iter);
    //     }

    //     // Загрузить регионы
    //     if(wobj.NeedToLoad.empty())
    //         continue;
 
    //     for(Pos::GlobalRegion &regionPos : wobj.NeedToLoad) {
    //         if(!SaveBackend.World->isExist(worldStringId, regionPos)) {
    //             wobj.Regions[regionPos]->IsLoaded = true;
    //         } else {
    //             SB_Region data;
    //             SaveBackend.World->load(worldStringId, regionPos, &data);
    //             Region &robj = *wobj.Regions[regionPos];

    //             // TODO: Передефайнить идентификаторы нод

    //             convertRegionVoxelsToChunks(data.Voxels, robj.Voxels);
    //         }
    //     }

    //     wobj.NeedToLoad.clear();
    // }
    
    // // Проверить отслеживание порталов
}

void GameServer::stepGlobalStep() {
    for(auto &pair : Expanse.Worlds)
        pair.second->onUpdate(this, CurrentTickDuration);
}

void GameServer::stepSyncContent() {
    for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
        cec->onUpdate();

        // Это для пробы строительства и ломания нод
        while(!cec->Build.empty()) {
            Pos::GlobalNode node = cec->Build.front();
            cec->Build.pop();

            Pos::GlobalRegion rPos = node >> 6;
            Pos::bvec4u cPos = (node >> 4) & 0x3;
            Pos::bvec16u nPos = node & 0xf;

            auto &region = Expanse.Worlds[0]->Regions[rPos];
            region->Nodes[cPos.pack()][nPos.pack()].NodeId = 4;
            region->IsChunkChanged_Nodes |= 1ull << cPos.pack();
        }

        while(!cec->Break.empty()) {
            Pos::GlobalNode node = cec->Break.front();
            cec->Break.pop();

            Pos::GlobalRegion rPos = node >> 6;
            Pos::bvec4u cPos = (node >> 4) & 0x3;
            Pos::bvec16u nPos = node & 0xf;

            auto &region = Expanse.Worlds[0]->Regions[rPos];
            region->Nodes[cPos.pack()][nPos.pack()].NodeId = 0;
            region->IsChunkChanged_Nodes |= 1ull << cPos.pack();
        }
    }


    // Сбор запросов на ресурсы и профили + отправка пакетов игрокам
    ResourceRequest full = std::move(Content.OnContentChanges);
    for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
        full.insert(cec->Remote->pushPreparedPackets());
    }


    full.uniq();
    // Запрашиваем двоичные ресурсы
    Content.BRM.needResourceResponse(full);
    Content.BRM.update(CurrentTickDuration);
    // Получаем готовую информацию
    {
        BinaryResourceManager::OutObj_t out = Content.BRM.takePreparedInformation();
        bool hasData = false;
        for(int iter = 0; iter < (int) EnumBinResource::MAX_ENUM; iter++)
            hasData |= !out.BinToHash[iter].empty();

        if(hasData) {
            for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
                cec->Remote->informateIdToHash(out.BinToHash);
            }
        }

        if(!out.HashToResource.empty())
            for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
                cec->Remote->informateBinary(out.HashToResource);
            }
    }

    // Оповещаем об игровых профилях
    if(!full.Voxel.empty()) {
        std::unordered_map<DefVoxelId_t, DefVoxel_t*> defines;

        for(DefVoxelId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_Voxel.find(id);
            if(iter != Content.ContentIdToDef_Voxel.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefVoxel(defines);
        }
    }

    if(!full.Node.empty()) {
        std::unordered_map<DefNodeId_t, DefNode_t*> defines;

        for(DefNodeId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_Node.find(id);
            if(iter != Content.ContentIdToDef_Node.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefNode(defines);
        }
    }

    if(!full.World.empty()) {
        std::unordered_map<DefWorldId_t, DefWorld_t*> defines;

        for(DefWorldId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_World.find(id);
            if(iter != Content.ContentIdToDef_World.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefWorld(defines);
        }
    }

    if(!full.Portal.empty()) {
        std::unordered_map<DefPortalId_t, DefPortal_t*> defines;

        for(DefPortalId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_Portal.find(id);
            if(iter != Content.ContentIdToDef_Portal.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefPortal(defines);
        }
    }

    if(!full.Entity.empty()) {
        std::unordered_map<DefEntityId_t, DefEntity_t*> defines;

        for(DefEntityId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_Entity.find(id);
            if(iter != Content.ContentIdToDef_Entity.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefEntity(defines);
        }
    }

    if(!full.Item.empty()) {
        std::unordered_map<DefItemId_t, DefItem_t*> defines;

        for(DefItemId_t id : full.Node) {
            auto iter = Content.ContentIdToDef_Item.find(id);
            if(iter != Content.ContentIdToDef_Item.end()) {
                defines[id] = &iter->second;
            }
        }

        for(std::shared_ptr<ContentEventController>& cec : Game.CECs) {
            cec->Remote->informateDefItem(defines);
        }
    }

    BackingChunkPressure.startCollectChanges();
}


}