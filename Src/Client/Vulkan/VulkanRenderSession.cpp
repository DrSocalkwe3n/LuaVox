#include "VulkanRenderSession.hpp"
#include "Client/Abstract.hpp"
#include "Client/Vulkan/Abstract.hpp"
#include "Client/Vulkan/Vulkan.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"
#include "assets.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/scalar_constants.hpp"
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>

namespace std {
    template<>
    struct hash<LV::Client::VK::NodeVertexStatic> {
        size_t operator()(const LV::Client::VK::NodeVertexStatic& v) const {
            const uint32_t* ptr = reinterpret_cast<const uint32_t*>(&v);
            size_t h1 = std::hash<uint32_t>{}(ptr[0]);
            size_t h2 = std::hash<uint32_t>{}(ptr[1]);
            size_t h3 = std::hash<uint32_t>{}(ptr[2]);

            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

namespace LV::Client::VK {

void ChunkMeshGenerator::changeThreadsCount(uint8_t threads) {
    Sync.NeedShutdown = true;
    std::unique_lock lock(Sync.Mutex);
    Sync.CV_CountInRun.wait(lock, [&]() { return Sync.CountInRun == 0; });

    for(std::thread& thr : Threads)
        thr.join();

    Sync.NeedShutdown = false;

    Threads.resize(threads);
    for(int iter = 0; iter < threads; iter++)
        Threads[iter] = std::thread(&ChunkMeshGenerator::run, this, iter);

    Sync.CV_CountInRun.wait(lock, [&]() { return Sync.CountInRun == Threads.size() || Sync.NeedShutdown; });
    
    if(Sync.NeedShutdown)
        MAKE_ERROR("Ошибка обработчика вершин чанков");
}

void ChunkMeshGenerator::run(uint8_t id) {
    Logger LOG = "ChunkMeshGenerator<"+std::to_string(id)+'>';

    {
        std::unique_lock lock(Sync.Mutex);
        Sync.CountInRun += 1;
        Sync.CV_CountInRun.notify_all();
    }

    LOG.debug() << "Старт потока верширования чанков";
    int timeWait = 1;

    try {
        while(!Sync.NeedShutdown) {
            if(Sync.Stop) {
                // Мир клиента начинает обрабатывать такты
                std::unique_lock lock(Sync.Mutex);
                if(Sync.Stop) {
                    Sync.CountInRun -= 1;
                    Sync.CV_CountInRun.notify_all();
                    Sync.CV_CountInRun.wait(lock, [&](){ return !Sync.Stop; });
                    Sync.CountInRun += 1;
                }
            }

            // Если нет входных запросов - ожидаем
            if(Input.get_read().empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(timeWait));
                if(++timeWait > 20)
                    timeWait = 20;

                continue;
            }

            timeWait = 0;

            WorldId_t wId;
            Pos::GlobalChunk pos;
            uint32_t requestId;

            {
                auto lock = Input.lock();
                if(lock->empty())
                    continue;

                std::tuple<WorldId_t, Pos::GlobalChunk, uint32_t> v = lock->front();
                wId = std::get<0>(v);
                pos = std::get<1>(v);
                requestId = std::get<2>(v);
                lock->pop();
            }

            ChunkObj_t result;
            result.RequestId = requestId;
            result.WId = wId;
            result.Pos = pos;

            const std::array<Node, 16*16*16>* chunk;
            const std::vector<VoxelCube>* voxels;
            // Если на позиции полная нода, то она перекрывает стороны соседей
            uint8_t fullNodes[18][18][18];

            // Профиль, который используется если на стороне клиента отсутствует нужных профиль
            DefNode_t defaultProfileNode;
            // Кеш запросов профилей нод
            std::unordered_map<DefNodeId, const DefNode_t*> profilesNodeCache;
            auto getNodeProfile = [&](DefNodeId id) -> const DefNode_t* {
                auto iterCache = profilesNodeCache.find(id);
                if(iterCache == profilesNodeCache.end()) {
                    // Промах кеша
                    auto iterSS = SS->Profiles.DefNode.find(id);
                    if(iterSS != SS->Profiles.DefNode.end()) {
                        return (profilesNodeCache[id] = &iterSS->second);
                    } else {
                        // Профиль отсутствует на клиенте
                        return (profilesNodeCache[id] = &defaultProfileNode);
                    }
                } else {
                    return iterCache->second;
                }
            };

            // Воксели пока не рендерим
            if(auto iterWorld = SS->Content.Worlds.find(wId); iterWorld != SS->Content.Worlds.end()) {
                Pos::GlobalRegion rPos = pos >> 2;
                if(auto iterRegion = iterWorld->second.Regions.find(rPos); iterRegion != iterWorld->second.Regions.end()) {
                    auto& chunkPtr = iterRegion->second.Chunks[Pos::bvec4u(pos & 0x3).pack()];
                    chunk = &chunkPtr.Nodes;
                    voxels = &chunkPtr.Voxels;
                } else
                    goto end;

                // Собрать чанки с каждой стороны для face culling
                const std::array<Node, 16*16*16>* chunks[6] = {0};

                for(int var = 0; var < 6; var++) {
                    Pos::GlobalChunk chunkPos = pos;

                    if(var == 0)
                        chunkPos += Pos::GlobalChunk(1, 0, 0);
                    else if(var == 1)
                        chunkPos += Pos::GlobalChunk(-1, 0, 0);
                    else if(var == 2)
                        chunkPos += Pos::GlobalChunk(0, 1, 0);
                    else if(var == 3)
                        chunkPos += Pos::GlobalChunk(0, -1, 0);
                    else if(var == 4)
                        chunkPos += Pos::GlobalChunk(0, 0, 1);
                    else if(var == 5)
                        chunkPos += Pos::GlobalChunk(0, 0, -1);

                    rPos = chunkPos >> 2;

                    if(auto iterRegion = iterWorld->second.Regions.find(rPos); iterRegion != iterWorld->second.Regions.end()) {
                        auto& chunkPtr = iterRegion->second.Chunks[Pos::bvec4u(chunkPos & 0x3).pack()];
                        chunks[var] = &chunkPtr.Nodes;
                    }
                }

                std::fill(((uint8_t*) fullNodes), ((uint8_t*) fullNodes)+18*18*18, 0);

                std::unordered_map<DefNodeId, bool> nodeFullCuboidCache;
                auto nodeIsFull = [&](Node node) -> bool {
                    auto iterCache = nodeFullCuboidCache.find(node.Data);
                    if(iterCache == nodeFullCuboidCache.end()) {
                        const DefNode_t* profile = getNodeProfile(node.NodeId);
                        if(profile->TexId != 0) {
                            return (nodeFullCuboidCache[node.Data] = true);
                        }

                        return (nodeFullCuboidCache[node.Data] = false);
                    } else {
                        return iterCache->second;
                    }
                };

                {
                    const Node* n = chunk->data();
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++)
                            for(int x = 0; x < 16; x++) {
                                fullNodes[x+1][y+1][z+1] = (uint8_t) nodeIsFull(n[x+y*16+z*16*16]);
                        
                            }
                }

                if(chunks[0]) {
                    const Node* n = chunks[0]->data();
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++) {
                            fullNodes[17][y+1][z+1] = (uint8_t) nodeIsFull(n[y*16+z*16*16]);
                        }
                } else {
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++)
                            fullNodes[17][y+1][z+1] = 1;
                }

                if(chunks[1]) {
                    const Node* n = chunks[1]->data();
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++) {
                            fullNodes[0][y+1][z+1] = (uint8_t) nodeIsFull(n[15+y*16+z*16*16]);
                        }
                } else {
                    for(int z = 0; z < 16; z++)
                        for(int y = 0; y < 16; y++)
                            fullNodes[0][y+1][z+1] = 1;
                }

                if(chunks[2]) {
                    const Node* n = chunks[2]->data();
                    for(int z = 0; z < 16; z++)
                        for(int x = 0; x < 16; x++) {
                            fullNodes[x+1][17][z+1] = (uint8_t) nodeIsFull(n[x+0+z*16*16]);
                        }
                } else {
                    for(int z = 0; z < 16; z++)
                        for(int x = 0; x < 16; x++)
                            fullNodes[x+1][17][z+1] = 1;
                }

                if(chunks[3]) {
                    const Node* n = chunks[3]->data();
                    for(int z = 0; z < 16; z++)
                        for(int x = 0; x < 16; x++) {
                            fullNodes[x+1][0][z+1] = (uint8_t) nodeIsFull(n[x+15*16+z*16*16]);
                        }
                } else {
                    for(int z = 0; z < 16; z++)
                        for(int x = 0; x < 16; x++)
                            fullNodes[x+1][0][z+1] = 1;
                }

                if(chunks[4]) {
                    const Node* n = chunks[4]->data();
                    for(int y = 0; y < 16; y++)
                        for(int x = 0; x < 16; x++) {
                            fullNodes[x+1][y+1][17] = (uint8_t) nodeIsFull(n[x+y*16+0]);
                        }
                } else {
                    for(int y = 0; y < 16; y++)
                        for(int x = 0; x < 16; x++)
                            fullNodes[x+1][y+1][17] = 1;
                }

                if(chunks[5]) {
                    const Node* n = chunks[5]->data();
                    for(int y = 0; y < 16; y++)
                        for(int x = 0; x < 16; x++) {
                            fullNodes[x+1][y+1][0] = (uint8_t) nodeIsFull(n[x+y*16+15*16*16]);
                        }
                } else {
                    for(int y = 0; y < 16; y++)
                        for(int x = 0; x < 16; x++)
                            fullNodes[x+0][y+1][0] = 1;
                }
            } else 
                goto end;

            {
                result.VoxelDefines.reserve(voxels->size());
                for(const VoxelCube& cube : *voxels)
                    result.VoxelDefines.push_back(cube.VoxelId);

                std::sort(result.VoxelDefines.begin(), result.VoxelDefines.end());
                auto eraseIter = std::unique(result.VoxelDefines.begin(), result.VoxelDefines.end());
                result.VoxelDefines.erase(eraseIter, result.VoxelDefines.end());
                result.VoxelDefines.shrink_to_fit();
            }

            {
                result.NodeDefines.reserve(16*16*16);
                for(int iter = 0; iter < 16*16*16; iter++)
                    result.NodeDefines.push_back((*chunk)[iter].NodeId);
                std::sort(result.NodeDefines.begin(), result.NodeDefines.end());
                auto eraseIter = std::unique(result.NodeDefines.begin(), result.NodeDefines.end());
                result.NodeDefines.erase(eraseIter, result.NodeDefines.end());
                result.NodeDefines.shrink_to_fit();
            }

            // Генерация вершин вокселей
            {

            }

            // Генерация вершин нод
            {
                NodeVertexStatic v;
                std::memset(&v, 0, sizeof(v));

                // Сбор вершин
                for(int z = 0; z < 16; z++)
                for(int y = 0; y < 16; y++)
                for(int x = 0; x < 16; x++) {
                    int fullCovered = 0;

                    fullCovered |= fullNodes[x+1+1][y+1][z+1];
                    fullCovered |= fullNodes[x+1-1][y+1][z+1] << 1;
                    fullCovered |= fullNodes[x+1][y+1+1][z+1] << 2;
                    fullCovered |= fullNodes[x+1][y+1-1][z+1] << 3;
                    fullCovered |= fullNodes[x+1][y+1][z+1+1] << 4;
                    fullCovered |= fullNodes[x+1][y+1][z+1-1] << 5;

                    if(fullCovered == 0b111111)
                        continue;

                    const DefNode_t* node = getNodeProfile((*chunk)[x+y*16+z*16*16].NodeId);

                    v.Tex = node->TexId;

                    if(v.Tex == 0)
                        continue;

                    // Рендерим обычный кубоид
                    if(!(fullCovered & 0b000100)) {
                        v.FX = 224+x*16;
                        v.FY = 224+y*16+16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FZ -= 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX = 224+x*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.FZ -= 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX -= 16;
                        v.TU = 0;
                        result.NodeVertexs.push_back(v);
                    }

                    if(!(fullCovered & 0b001000)) {
                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FZ -= 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX = 224+x*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.FZ -= 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FZ += 16;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);
                    }

                    if(!(fullCovered & 0b000001)) {
                        v.FX = 224+x*16+16;
                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FZ -= 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.FZ -= 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FZ += 16;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);
                    }

                    if(!(fullCovered & 0b000010)) {
                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FZ -= 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.FZ -= 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY -= 16;
                        v.TU = 0;
                        result.NodeVertexs.push_back(v);
                    }

                    if(!(fullCovered & 0b010000)) {
                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.FZ = 224+z*16+16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.FY += 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX -= 16;
                        v.TU = 0;
                        result.NodeVertexs.push_back(v);
                    }

                    if(!(fullCovered & 0b100000)) {
                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.FZ = 224+z*16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FY += 16;
                        v.TV = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FX = 224+x*16;
                        v.FY = 224+y*16;
                        v.TU = 0;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);

                        v.FX += 16;
                        v.FY += 16;
                        v.TV = 65535;
                        v.TU = 65535;
                        result.NodeVertexs.push_back(v);

                        v.FY -= 16;
                        v.TV = 0;
                        result.NodeVertexs.push_back(v);
                    }
                }

                // Вычислить индексы и сократить вершины
                {
                    uint32_t nextIndex = 0;
                    std::vector<NodeVertexStatic> vertexes;
                    std::unordered_map<NodeVertexStatic, uint32_t> vertexTable;
                    std::vector<uint32_t> indexes;
                    
                    for(const NodeVertexStatic& vertex : result.NodeVertexs) {
                        auto iter = vertexTable.find(vertex);
                        if(iter == vertexTable.end()) {
                            vertexTable.insert({vertex, nextIndex});
                            vertexes.push_back(vertex);
                            indexes.push_back(nextIndex);
                            nextIndex += 1;
                        } else {
                            indexes.push_back(iter->second);
                        }
                    }

                    result.NodeVertexs = std::move(vertexes);

                    if(nextIndex <= (1 << 16)) {
                        std::vector<uint16_t> indexes16;
                        indexes16.reserve(indexes.size());
                        for(size_t iter = 0; iter < indexes.size(); iter++)
                            indexes16.push_back(indexes[iter]);

                        result.NodeIndexes = std::move(indexes16);
                    } else {
                        result.NodeIndexes = std::move(indexes);
                    }
                }
            }

            end:
            Output.lock()->emplace_back(std::move(result));

        }
    } catch(const std::exception& exc) {
        LOG.debug() << "Ошибка в работе потока:\n" << exc.what();
        Sync.NeedShutdown = true;
    }

    {
        std::unique_lock lock(Sync.Mutex);
        Sync.CountInRun -= 1;
        Sync.CV_CountInRun.notify_all();
    }

    LOG.debug() << "Завершение потока верширования чанков";
}


void ChunkPreparator::tickSync(const TickSyncData& data) {
    // Обработать изменения в чанках
    // Пересчёт соседних чанков
    // Проверить необходимость пересчёта чанков при изменении профилей

    // Добавляем к изменёным чанкам пересчёт соседей
    {
        std::vector<std::tuple<WorldId_t, Pos::GlobalChunk, uint32_t>> toBuild;
        for(auto& [wId, chunks] : data.ChangedChunks) {
            std::vector<Pos::GlobalChunk> list;
            for(const Pos::GlobalChunk& pos : chunks) {
                list.push_back(pos);
                list.push_back(pos+Pos::GlobalChunk(1, 0, 0));
                list.push_back(pos+Pos::GlobalChunk(-1, 0, 0));
                list.push_back(pos+Pos::GlobalChunk(0, 1, 0));
                list.push_back(pos+Pos::GlobalChunk(0, -1, 0));
                list.push_back(pos+Pos::GlobalChunk(0, 0, 1));
                list.push_back(pos+Pos::GlobalChunk(0, 0, -1));
            }

            std::sort(list.begin(), list.end());
            auto eraseIter = std::unique(list.begin(), list.end());
            list.erase(eraseIter, list.end());


            for(Pos::GlobalChunk& pos : list) {
                Pos::GlobalRegion rPos = pos >> 2;
                auto iterRegion = Requests[wId].find(rPos);
                if(iterRegion != Requests[wId].end())
                    toBuild.emplace_back(wId, pos, iterRegion->second);
                else
                    toBuild.emplace_back(wId, pos, Requests[wId][rPos] = NextRequest++);
            }
        }

        CMG.Input.lock()->push_range(toBuild);
    }

    // Чистим запросы и чанки
    {
        uint8_t frameRetirement = (FrameRoulette+FRAME_COUNT_RESOURCE_LATENCY) % FRAME_COUNT_RESOURCE_LATENCY;
        for(auto& [wId, regions] : data.LostRegions) {
            if(auto iterWorld = Requests.find(wId); iterWorld != Requests.end()) {
                for(const Pos::GlobalRegion& rPos : regions)
                    if(auto iterRegion = iterWorld->second.find(rPos); iterRegion != iterWorld->second.end())
                        iterWorld->second.erase(iterRegion);
            }

            if(auto iterWorld = ChunksMesh.find(wId); iterWorld != ChunksMesh.end()) {
                for(const Pos::GlobalRegion& rPos : regions)
                    if(auto iterRegion = iterWorld->second.find(rPos); iterRegion != iterWorld->second.end()) {
                        for(int iter = 0; iter < 4*4*4; iter++) {
                            auto& chunk = iterRegion->second[iter];
                            if(chunk.VoxelPointer)
                                VPV_ToFree[frameRetirement].emplace_back(std::move(chunk.VoxelPointer));
                            if(chunk.NodePointer) {
                                VPN_ToFree[frameRetirement].emplace_back(std::move(chunk.NodePointer), std::move(chunk.NodeIndexes));
                            }
                        }
                        
                        iterWorld->second.erase(iterRegion);
                    }
            }
        }
    }

    // Получаем готовые чанки
    {
        std::vector<ChunkMeshGenerator::ChunkObj_t> chunks = std::move(*CMG.Output.lock());
        for(auto& chunk : chunks) {
            auto iterWorld = Requests.find(chunk.WId);
            if(iterWorld == Requests.end())
                continue;

            auto iterRegion = iterWorld->second.find(chunk.Pos >> 2);
            if(iterRegion == iterWorld->second.end())
                continue;

            if(iterRegion->second != chunk.RequestId)
                continue;

            // Чанк ожидаем
            auto& rChunk = ChunksMesh[chunk.WId][chunk.Pos >> 2][Pos::bvec4u(chunk.Pos & 0x3).pack()];
            rChunk.Voxels = std::move(chunk.VoxelDefines);
            if(!chunk.VoxelVertexs.empty())
                rChunk.VoxelPointer = VertexPool_Voxels.pushVertexs(std::move(chunk.VoxelVertexs));
            rChunk.Nodes = std::move(chunk.NodeDefines);
            if(!chunk.NodeVertexs.empty())
                rChunk.NodePointer = VertexPool_Nodes.pushVertexs(std::move(chunk.NodeVertexs));

            if(std::vector<uint16_t>* ptr = std::get_if<std::vector<uint16_t>>(&chunk.NodeIndexes)) {
                if(!ptr->empty())
                    rChunk.NodeIndexes = IndexPool_Nodes_16.pushVertexs(std::move(*ptr));
            } else if(std::vector<uint32_t>* ptr = std::get_if<std::vector<uint32_t>>(&chunk.NodeIndexes)) {
                if(!ptr->empty())
                    rChunk.NodeIndexes = IndexPool_Nodes_32.pushVertexs(std::move(*ptr));
            }
        }
    }

    VertexPool_Voxels.update(CMDPool);
    VertexPool_Nodes.update(CMDPool);
    IndexPool_Nodes_16.update(CMDPool);
    IndexPool_Nodes_32.update(CMDPool);

    CMG.endTickSync();
}

void ChunkPreparator::pushFrame() {
    FrameRoulette = (FrameRoulette+1) % FRAME_COUNT_RESOURCE_LATENCY;

    for(auto pointer : VPV_ToFree[FrameRoulette]) {
        VertexPool_Voxels.dropVertexs(pointer);
    }

    VPV_ToFree[FrameRoulette].clear();

    for(auto& pointer : VPN_ToFree[FrameRoulette]) {
        VertexPool_Nodes.dropVertexs(std::get<0>(pointer));
        if(IndexPool<uint16_t>::Pointer* ind = std::get_if<IndexPool<uint16_t>::Pointer>(&std::get<1>(pointer))) {
            IndexPool_Nodes_16.dropVertexs(*ind);
        } else if(IndexPool<uint32_t>::Pointer* ind = std::get_if<IndexPool<uint32_t>::Pointer>(&std::get<1>(pointer))) {
            IndexPool_Nodes_32.dropVertexs(*ind);
        }
    }

    VPN_ToFree[FrameRoulette].clear();
}

std::pair<
    std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>>,
    std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>>
> ChunkPreparator::getChunksForRender(
    WorldId_t worldId, Pos::Object pos, uint8_t distance, glm::mat4 projView, Pos::GlobalRegion x64offset
) {
    Pos::GlobalChunk playerChunk = pos >> Pos::Object_t::BS_Bit >> 4;
    Pos::GlobalRegion center = playerChunk >> 2;

    std::vector<std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>> vertexVoxels;
    std::vector<std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>> vertexNodes;

    auto iterWorld = ChunksMesh.find(worldId);
    if(iterWorld == ChunksMesh.end())
        return {};

    Frustum fr(projView);

    for(int z = -distance; z <= distance; z++) {
        for(int y = -distance; y <= distance; y++) {
            for(int x = -distance; x <= distance; x++) {
                Pos::GlobalRegion region = center + Pos::GlobalRegion(x, y, z);
                glm::vec3 begin = glm::vec3(region - x64offset) * 64.f;
                glm::vec3 end = begin + glm::vec3(64.f);

                if(!fr.IsBoxVisible(begin, end))
                    continue;

                auto iterRegion = iterWorld->second.find(region);
                if(iterRegion == iterWorld->second.end()) 
                    continue;

                Pos::GlobalChunk local = Pos::GlobalChunk(region) << 2;

                for(size_t index = 0; index < iterRegion->second.size(); index++) {
                    Pos::bvec4u localPos;
                    localPos.unpack(index);

                    glm::vec3 chunkPos = begin+glm::vec3(localPos)*16.f;
                    if(!fr.IsBoxVisible(chunkPos, chunkPos+glm::vec3(16)))
                        continue;

                    auto &chunk = iterRegion->second[index];
                    
                    float distance;

                    if(chunk.VoxelPointer || chunk.NodePointer) {
                        Pos::GlobalChunk cp = local+Pos::GlobalChunk(localPos)-playerChunk;
                        distance = cp.x*cp.x+cp.y*cp.y+cp.z*cp.z;
                    }

                    if(chunk.VoxelPointer) {
                        vertexVoxels.emplace_back(distance, local+Pos::GlobalChunk(localPos), VertexPool_Voxels.map(chunk.VoxelPointer), chunk.VoxelPointer.VertexCount);
                    }

                    if(chunk.NodePointer) {
                        vertexNodes.emplace_back(
                            distance, local+Pos::GlobalChunk(localPos), 
                            VertexPool_Nodes.map(chunk.NodePointer), 
                            chunk.NodeIndexes.index() == 0
                                ? IndexPool_Nodes_16.map(std::get<0>(chunk.NodeIndexes))
                                : IndexPool_Nodes_32.map(std::get<1>(chunk.NodeIndexes))
                            , chunk.NodeIndexes.index() == 0, 
                            std::visit<uint32_t>([](const auto& val) -> uint32_t { return val.VertexCount; }, chunk.NodeIndexes));
                    }
                }
            }
        }
    }

    {
        auto sortByDistance = []
        (
            const std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>& a, 
            const std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>& b
        ) {
            return std::get<0>(a) < std::get<0>(b);
        };
        
        std::sort(vertexVoxels.begin(), vertexVoxels.end(), sortByDistance);
    }

    {
        auto sortByDistance = []
        (
            const std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>& a, 
            const std::tuple<float, Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>& b
        ) {
            return std::get<0>(a) < std::get<0>(b);
        };
        std::sort(vertexNodes.begin(), vertexNodes.end(), sortByDistance);
    }

    std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>> resVertexVoxels;
    std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>> resVertexNodes;

    resVertexVoxels.reserve(vertexVoxels.size());
    resVertexNodes.reserve(vertexNodes.size());

    for(auto& [d, pos, ptr, count] : vertexVoxels)
        resVertexVoxels.emplace_back(pos, std::move(ptr), count);

    for(auto& [d, pos, ptr, ptr2, type, count] : vertexNodes)
        resVertexNodes.emplace_back(pos, std::move(ptr), std::move(ptr2), type, count);

    return std::pair{std::move(resVertexVoxels), std::move(resVertexNodes)};
}

VulkanRenderSession::VulkanRenderSession(Vulkan *vkInst, IServerSession *serverSession)
    :   VkInst(vkInst),
        ServerSession(serverSession),
        CP(vkInst, serverSession),
        MainTest(vkInst), LightDummy(vkInst),
        TestQuad(vkInst, sizeof(NodeVertexStatic)*6*3*2)
{
    assert(vkInst);
    assert(serverSession);

    {
        std::vector<VkDescriptorPoolSize> poolSizes {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}
        };

        VkDescriptorPoolCreateInfo descriptorPool = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 8,
            .poolSizeCount = (uint32_t) poolSizes.size(),
            .pPoolSizes = poolSizes.data()
        };

        vkAssert(!vkCreateDescriptorPool(VkInst->Graphics.Device, &descriptorPool, nullptr,
                                        &DescriptorPool));
    }

    {
	    std::vector<VkDescriptorSetLayoutBinding> shaderLayoutBindings =
        {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            }, {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            }
        };

        const VkDescriptorSetLayoutCreateInfo descriptorLayout =
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t) shaderLayoutBindings.size(),
			.pBindings = shaderLayoutBindings.data()
		};

        vkAssert(!vkCreateDescriptorSetLayout(
            VkInst->Graphics.Device, &descriptorLayout, nullptr, &MainAtlasDescLayout));
    }

    {
        VkDescriptorSetAllocateInfo ciAllocInfo =
        {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &MainAtlasDescLayout
        };

        vkAssert(!vkAllocateDescriptorSets(VkInst->Graphics.Device, &ciAllocInfo, &MainAtlasDescriptor));
    }

    {
	    std::vector<VkDescriptorSetLayoutBinding> shaderLayoutBindings =
        {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            }, {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr
            }
        };

        const VkDescriptorSetLayoutCreateInfo descriptorLayout =
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t) shaderLayoutBindings.size(),
			.pBindings = shaderLayoutBindings.data()
		};

        vkAssert(!vkCreateDescriptorSetLayout(VkInst->Graphics.Device, &descriptorLayout, nullptr, &VoxelLightMapDescLayout));
    }

    {
        VkDescriptorSetAllocateInfo ciAllocInfo =
        {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &VoxelLightMapDescLayout
        };

        vkAssert(!vkAllocateDescriptorSets(VkInst->Graphics.Device, &ciAllocInfo, &VoxelLightMapDescriptor));
    }

    
    MainTest.atlasAddCallbackOnUniformChange([this]() -> bool {
        updateDescriptor_MainAtlas();
        return true;
    });

    LightDummy.atlasAddCallbackOnUniformChange([this]() -> bool {
        updateDescriptor_VoxelsLight();
        return true;
    });

    {
        uint16_t texId = MainTest.atlasAddTexture(2, 2);
        uint32_t colors[4] = {0xfffffffful, 0x00fffffful, 0xffffff00ul, 0xff00fffful};
        MainTest.atlasChangeTextureData(texId, (const uint32_t*) colors);
    }

    {
        int width, height;
        bool hasAlpha;
        for(const char *path : {
                "grass.png",
                "willow_wood.png",
                "tropical_rainforest_wood.png",
                "xnether_blue_wood.png",
                "xnether_purple_wood.png",
                "frame.png"
        }) {
            ByteBuffer image = VK::loadPNG(getResource(std::string("textures/") + path)->makeStream().Stream, width, height, hasAlpha);
            uint16_t texId = MainTest.atlasAddTexture(width, height);
            MainTest.atlasChangeTextureData(texId, (const uint32_t*) image.data());
        }
    }

    /*
    x left -1 ~ right 1
    y up 1 ~ down -1
    z near 0 ~ far -1

    glm

    */

    {
        NodeVertexStatic *array = (NodeVertexStatic*) TestQuad.mapMemory();
        array[0] = {135,    135,    135, 0, 0, 0, 0, 65535,      0};
        array[1] = {135,    135+16,   135, 0, 0, 0, 0, 0,  65535};
        array[2] = {135+16, 135+16, 135, 0, 0, 0, 0, 0,  65535};
        array[3] = {135,    135,    135, 0, 0, 0, 0, 65535,      0};
        array[4] = {135+16, 135+16, 135, 0, 0, 0, 0, 0,  65535};
        array[5] = {135+16, 135, 135, 0, 0, 0, 0, 0,      0};

        array[6] = {135,    135,    135+16, 0, 0, 0, 0, 0,      0};
        array[7] = {135+16,    135,   135+16, 0, 0, 0, 0, 65535,  0};
        array[8] = {135+16, 135+16, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[9] = {135,    135,    135+16, 0, 0, 0, 0, 0,      0};
        array[10] = {135+16, 135+16, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[11] = {135, 135+16, 135+16, 0, 0, 0, 0, 0,      65535};

        array[12] = {135,    135,    135, 0, 0, 0, 0, 0,      0};
        array[13] = {135,    135,   135+16, 0, 0, 0, 0, 65535,  0};
        array[14] = {135, 135+16, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[15] = {135,    135,    135, 0, 0, 0, 0, 0,      0};
        array[16] = {135, 135+16, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[17] = {135, 135+16, 135, 0, 0, 0, 0, 0,      65535};

        array[18] = {135+16,    135,    135+16, 0, 0, 0, 0, 0,      0};
        array[19] = {135+16,    135,   135, 0, 0, 0, 0, 65535,  0};
        array[20] = {135+16, 135+16, 135, 0, 0, 0, 0, 65535,  65535};
        array[21] = {135+16,    135,    135+16, 0, 0, 0, 0, 0,      0};
        array[22] = {135+16, 135+16, 135, 0, 0, 0, 0, 65535,  65535};
        array[23] = {135+16, 135+16, 135+16, 0, 0, 0, 0, 0,      65535};

        array[24] = {135,    135,    135, 0, 0, 0, 0, 0,      0};
        array[25] = {135+16,    135,   135, 0, 0, 0, 0, 65535,  0};
        array[26] = {135+16, 135, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[27] = {135,    135,    135, 0, 0, 0, 0, 0,      0};
        array[28] = {135+16, 135, 135+16, 0, 0, 0, 0, 65535,  65535};
        array[29] = {135, 135, 135+16, 0, 0, 0, 0, 0,      65535};

        array[30] = {135,    135+16,    135+16, 0, 0, 0, 0, 0,      0};
        array[31] = {135+16,    135+16,   135+16, 0, 0, 0, 0, 65535,  0};
        array[32] = {135+16, 135+16, 135, 0, 0, 0, 0, 65535,  65535};
        array[33] = {135,    135+16,    135+16, 0, 0, 0, 0, 0,      0};
        array[34] = {135+16, 135+16, 135, 0, 0, 0, 0, 65535,  65535};
        array[35] = {135, 135+16, 135, 0, 0, 0, 0, 0,      65535};

        for(int iter = 0; iter < 36; iter++) {
            array[iter].Tex = 6;
            if(array[iter].FX == 135)
                array[iter].FX--;
            else
                array[iter].FX++;

            if(array[iter].FY == 135)
                array[iter].FY--;
            else
                array[iter].FY++;

            if(array[iter].FZ == 135)
                array[iter].FZ--;
            else
                array[iter].FZ++;
        }

        TestQuad.unMapMemory();
    }

    {
        std::vector<VoxelCube> cubes;

        cubes.push_back({0, 0, Pos::bvec256u{0, 0, 0}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({1, 0, Pos::bvec256u{255, 0, 0}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({1, 0, Pos::bvec256u{0, 255, 0}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({1, 0, Pos::bvec256u{0, 0, 255}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({2, 0, Pos::bvec256u{255, 255, 0}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({2, 0, Pos::bvec256u{0, 255, 255}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({2, 0, Pos::bvec256u{255, 0, 255}, Pos::bvec256u{0, 0, 0}});
        cubes.push_back({3, 0, Pos::bvec256u{255, 255, 255}, Pos::bvec256u{0, 0, 0}});

        cubes.push_back({4, 0, Pos::bvec256u{64, 64, 64}, Pos::bvec256u{127, 127, 127}});

        std::vector<VoxelVertexPoint> vertexs = generateMeshForVoxelChunks(cubes);

        if(!vertexs.empty()) {
            TestVoxel.emplace(VkInst, vertexs.size()*sizeof(VoxelVertexPoint));
            std::copy(vertexs.data(), vertexs.data()+vertexs.size(), (VoxelVertexPoint*) TestVoxel->mapMemory());
            TestVoxel->unMapMemory();
        }
    }

    updateDescriptor_MainAtlas();
    updateDescriptor_VoxelsLight();
    updateDescriptor_ChunksLight();

    // Разметка графических конвейеров
    {
        std::vector<VkPushConstantRange> worldWideShaderPushConstants =
        {
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
                .offset = 0,
                .size = uint32_t(sizeof(WorldPCO))
            }
        };

        std::vector<VkDescriptorSetLayout> layouts =
        {
            MainAtlasDescLayout,
            VoxelLightMapDescLayout
        };

		const VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = (uint32_t) layouts.size(),
			.pSetLayouts = layouts.data(),
			.pushConstantRangeCount = (uint32_t) worldWideShaderPushConstants.size(),
			.pPushConstantRanges = worldWideShaderPushConstants.data()
		};
        
		vkAssert(!vkCreatePipelineLayout(VkInst->Graphics.Device, &pPipelineLayoutCreateInfo, nullptr, &MainAtlas_LightMap_PipelineLayout));
    }

    // Настройка мультисемплинга
    // Может нужно будет в будущем связать с настройками главного буфера
    VkPipelineMultisampleStateCreateInfo multisample =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = false,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = false,
        .alphaToOneEnable = false
    };


    // Конвейеры для вокселей и нод
    {
        VkPipelineCacheCreateInfo infoPipelineCache {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .initialDataSize = 0,
            .pInitialData = nullptr
        };
        
        // Для статичных непрозрачных и полупрозрачных вокселей
        if(!VoxelShaderVertex)
            VoxelShaderVertex = VkInst->createShader(getResource("shaders/chunk/voxel.vert.bin")->makeView());

        if(!VoxelShaderGeometry)
            VoxelShaderGeometry = VkInst->createShader(getResource("shaders/chunk/voxel.geom.bin")->makeView());

        if(!VoxelShaderFragmentOpaque)
            VoxelShaderFragmentOpaque = VkInst->createShader(getResource("shaders/chunk/voxel_opaque.frag.bin")->makeView());

        if(!VoxelShaderFragmentTransparent)
            VoxelShaderFragmentTransparent = VkInst->createShader(getResource("shaders/chunk/voxel_transparent.frag.bin")->makeView());

		// Конвейер шейдеров
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages =
        {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = *VoxelShaderVertex,
                .pName = "main",
                .pSpecializationInfo = nullptr
            }, {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
                .module = *VoxelShaderGeometry,
                .pName = "main",
                .pSpecializationInfo = nullptr
            }, {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = *VoxelShaderFragmentOpaque,
                .pName = "main",
                .pSpecializationInfo = nullptr
            }
        };

        // Вершины шейдера
        // Настройка формата вершин шейдера
        std::vector<VkVertexInputBindingDescription> shaderVertexBindings =
        {
            {
                .binding = 0,
                .stride = sizeof(VoxelVertexPoint),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };

		std::vector<VkVertexInputAttributeDescription> shaderVertexAttribute =
        {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_UINT,
                .offset = 0
            }
        };

        VkPipelineVertexInputStateCreateInfo createInfoVertexInput =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = (uint32_t) shaderVertexBindings.size(),
            .pVertexBindingDescriptions = shaderVertexBindings.data(),
            .vertexAttributeDescriptionCount = (uint32_t) shaderVertexAttribute.size(),
            .pVertexAttributeDescriptions = shaderVertexAttribute.data()
        };

	    // Топология вершин на входе (треугольники, линии, точки)
        VkPipelineInputAssemblyStateCreateInfo ia =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
            .primitiveRestartEnable = false
        };

        VkPipelineViewportStateCreateInfo vp =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr
        };

		// Настройки растеризатора
		VkPipelineRasterizationStateCreateInfo rasterization =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = false,
            .rasterizerDiscardEnable = false,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = false,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };

		// Тест буфера глубины и трафарета
		VkPipelineDepthStencilStateCreateInfo depthStencil =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = false,
            .stencilTestEnable = false,
            .front = VkStencilOpState
            {
                .failOp = VK_STENCIL_OP_KEEP,
                .passOp = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .compareMask = 0x0,
                .writeMask = 0x0,
                .reference = 0x0
            },
            .back = VkStencilOpState
            {
                .failOp = VK_STENCIL_OP_KEEP,
                .passOp = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .compareMask = 0x0,
                .writeMask = 0x0,
                .reference = 0x0
            },
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 0.0f
        };

        // Логика смешивания цветов
		std::vector<VkPipelineColorBlendAttachmentState> colorBlend =
        {
            {
                .blendEnable = false,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = 0xf
            }
        };

        VkPipelineColorBlendStateCreateInfo cb =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_CLEAR,
            .attachmentCount = (uint32_t) colorBlend.size(),
            .pAttachments = colorBlend.data(),
            .blendConstants = {0.f, 0.f, 0.f, 0.f}
        };

        // Настройки конвейера, которые могут быть изменены без пересоздания конвейера
		std::vector<VkDynamicState> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = (uint32_t) dynamicStates.size(),
            .pDynamicStates = dynamicStates.data(),
        };

        VkGraphicsPipelineCreateInfo pipeline =
        {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = (uint32_t) shaderStages.size(),
            .pStages = shaderStages.data(),
            .pVertexInputState = &createInfoVertexInput,
            .pInputAssemblyState = &ia,
            .pTessellationState = nullptr,
            .pViewportState = &vp,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisample,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &cb,
            .pDynamicState = &dynamicState,
            .layout = MainAtlas_LightMap_PipelineLayout,
            .renderPass = VkInst->Graphics.RenderPass,
            .subpass = 0,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = 0
        };

        if(!VoxelOpaquePipeline)
            vkAssert(!vkCreateGraphicsPipelines(VkInst->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelOpaquePipeline));

        if(!VoxelTransparentPipeline) {
            shaderStages[2].module = *VoxelShaderFragmentTransparent,

            colorBlend[0] =
            {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = 0xf
            };

           vkAssert(!vkCreateGraphicsPipelines(VkInst->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelTransparentPipeline));
        }

        // Для статичных непрозрачных и полупрозрачных нод
        if(!NodeShaderVertex)
            NodeShaderVertex = VkInst->createShader(getResource("shaders/chunk/node.vert.bin")->makeView());

        if(!NodeShaderGeometry)
            NodeShaderGeometry = VkInst->createShader(getResource("shaders/chunk/node.geom.bin")->makeView());

        if(!NodeShaderFragmentOpaque)
            NodeShaderFragmentOpaque = VkInst->createShader(getResource("shaders/chunk/node_opaque.frag.bin")->makeView());

        if(!NodeShaderFragmentTransparent)
            NodeShaderFragmentTransparent = VkInst->createShader(getResource("shaders/chunk/node_transparent.frag.bin")->makeView());

        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,

        shaderStages[0].module = *NodeShaderVertex;
        shaderStages[1].module = *NodeShaderGeometry;
        shaderStages[2].module = *NodeShaderFragmentOpaque;

        colorBlend[0] =
        {
            .blendEnable = false,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 0xf
        };

        if(!NodeStaticOpaquePipeline) {
            vkAssert(!vkCreateGraphicsPipelines(VkInst->Graphics.Device, VK_NULL_HANDLE,
                 1, &pipeline, nullptr, &NodeStaticOpaquePipeline));
        }

        if(!NodeStaticTransparentPipeline) {
            shaderStages[2].module = *NodeShaderFragmentTransparent;

            colorBlend[0] =
            {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = 0xf
            };

            vkAssert(!vkCreateGraphicsPipelines(VkInst->Graphics.Device, VK_NULL_HANDLE, 
                1, &pipeline, nullptr, &NodeStaticTransparentPipeline));
        }
    }
}

VulkanRenderSession::~VulkanRenderSession() {
    if(VoxelOpaquePipeline)
        vkDestroyPipeline(VkInst->Graphics.Device, VoxelOpaquePipeline, nullptr);
    if(VoxelTransparentPipeline)
        vkDestroyPipeline(VkInst->Graphics.Device, VoxelTransparentPipeline, nullptr);
    if(NodeStaticOpaquePipeline)
        vkDestroyPipeline(VkInst->Graphics.Device, NodeStaticOpaquePipeline, nullptr);
    if(NodeStaticTransparentPipeline)
        vkDestroyPipeline(VkInst->Graphics.Device, NodeStaticTransparentPipeline, nullptr);

    if(MainAtlas_LightMap_PipelineLayout)
        vkDestroyPipelineLayout(VkInst->Graphics.Device, MainAtlas_LightMap_PipelineLayout, nullptr);
        
    if(MainAtlasDescLayout)
        vkDestroyDescriptorSetLayout(VkInst->Graphics.Device, MainAtlasDescLayout, nullptr);
    if(VoxelLightMapDescLayout)
        vkDestroyDescriptorSetLayout(VkInst->Graphics.Device, VoxelLightMapDescLayout, nullptr);

    if(DescriptorPool)
        vkDestroyDescriptorPool(VkInst->Graphics.Device, DescriptorPool, nullptr);
}

void VulkanRenderSession::prepareTickSync() {
    CP.prepareTickSync();
}

void VulkanRenderSession::pushStageTickSync() {
    CP.pushStageTickSync();
}

void VulkanRenderSession::tickSync(const TickSyncData& data) {
    // Изменение ассетов
    // Профили
    // Чанки

    ChunkPreparator::TickSyncData mcpData;
    mcpData.ChangedChunks = data.Chunks_ChangeOrAdd;
    mcpData.LostRegions = data.Chunks_Lost;
    CP.tickSync(mcpData);

    {
        std::vector<std::tuple<ResourceId, Resource>> resources;
        std::vector<ResourceId> lost;

        for(const auto& [type, ids] : data.Assets_ChangeOrAdd) {
            if(type != EnumAssets::Model)
                continue;

            const auto& list = ServerSession->Assets[type];
            for(ResourceId id : ids) {
                auto iter = list.find(id);
                if(iter == list.end())
                    continue;

                resources.emplace_back(id, iter->second.Res);
            }
        }

        for(const auto& [type, ids] : data.Assets_Lost) {
            if(type != EnumAssets::Model)
                continue;

            lost.append_range(ids);
        }

        if(!resources.empty() || !lost.empty())
            MP.onModelChanges(std::move(resources), std::move(lost));
    }
}

void VulkanRenderSession::setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) {
    WorldId = worldId;
    Pos = pos;
    Quat = quat;

    WI = worldId;
    PlayerPos = pos;
    PlayerPos /= float(Pos::Object_t::BS);
}

void VulkanRenderSession::beforeDraw() {
    MainTest.atlasUpdateDynamicData();
    LightDummy.atlasUpdateDynamicData();
}

void VulkanRenderSession::drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd) {
    {
        X64Offset = Pos & ~((1 << Pos::Object_t::BS_Bit << 4 << 2)-1);
        X64Offset_f = glm::vec3(X64Offset >> Pos::Object_t::BS_Bit);
        X64Delta = glm::vec3(Pos-X64Offset) / float(Pos::Object_t::BS);
    }


    // vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, NodeStaticOpaquePipeline);
	// vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    // vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //     MainAtlas_LightMap_PipelineLayout,  0, 2, 
    //     (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    // VkDeviceSize vkOffsets = 0;
    // VkBuffer vkBuffer = VKCTX->TestQuad;
    // vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);

    // for(int i = 0; i < 16; i++) {
    //     PCO.Model = glm::rotate(PCO.Model, glm::half_pi<float>()/4, glm::vec3(0, 1, 0));
    //     vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    //     vkCmdDraw(drawCmd, 6, 1, 0, 0);
    // }

    // PCO.Model = glm::mat4(1);

    // // Проба рендера вокселей
    // vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VoxelOpaquePipeline);
	// vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    // vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //     MainAtlas_LightMap_PipelineLayout,  0, 2, 
    //     (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    // if(VKCTX->TestVoxel) {
    //     vkBuffer = *VKCTX->TestVoxel;
    //     vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    //     vkCmdDraw(drawCmd, VKCTX->TestVoxel->getSize() / sizeof(VoxelVertexPoint), 1, 0, 0);
    // }

    // {
    //     auto iterWorld = External.ChunkVoxelMesh.find(WorldId);
    //     if(iterWorld != External.ChunkVoxelMesh.end()) {
    //         glm::mat4 orig = PCO.Model;

    //         for(auto &pair : iterWorld->second) {
    //             if(auto& voxels = std::get<0>(pair.second)) {
    //                 glm::vec3 cpos(pair.first.x, pair.first.y, pair.first.z);
    //                 PCO.Model = glm::translate(orig, cpos*16.f);
    //                 auto [vkBuffer, offset] = VKCTX->VertexPool_Voxels.map(voxels);

    //                 vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
    //                 vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    //                 vkCmdDraw(drawCmd, voxels.VertexCount, 1, offset, 0);
    //             }
    //         }

    //         PCO.Model = orig;
    //     }
    // }

    // vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, NodeStaticOpaquePipeline);
	// vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    // vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //     MainAtlas_LightMap_PipelineLayout,  0, 2, 
    //     (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    // {
    //     auto iterWorld = External.ChunkVoxelMesh.find(WorldId);
    //     if(iterWorld != External.ChunkVoxelMesh.end()) {
    //         glm::mat4 orig = PCO.Model;

    //         for(auto &pair : iterWorld->second) {
    //             if(auto& nodes = std::get<1>(pair.second)) {
    //                 glm::vec3 cpos(pair.first.z, pair.first.y, pair.first.x);
    //                 PCO.Model = glm::translate(orig, cpos*16.f);
    //                 auto [vkBuffer, offset] = VKCTX->VertexPool_Nodes.map(nodes);

    //                 vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
    //                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
    //                 vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    //                 vkCmdDraw(drawCmd, nodes.VertexCount, 1, offset, 0);
    //             }
    //         }

    //         PCO.Model = orig;
    //     }
    // }

    static float Delta = 0;
    Delta += dTime;

    PCO.Model = glm::mat4(1);
    //PCO.Model = glm::translate(PCO.Model, -X64Offset_f);
    // glm::quat quat = glm::inverse(Quat);

    {

        // auto *srv = (class ServerSession*) ServerSession;

        glm::vec4 v = glm::mat4(glm::inverse(Quat))*glm::vec4(0, 0, -6, 1);

        Pos::GlobalNode pos = (Pos::GlobalNode) (glm::vec3) v;

        pos += (Pos-X64Offset) >> Pos::Object_t::BS_Bit;
        PCO.Model = glm::translate(PCO.Model, glm::vec3(pos));
    }
    

    {
        glm::mat4 proj = glm::perspective<float>(glm::radians(75.f), float(VkInst->Screen.Width)/float(VkInst->Screen.Height), 0.5, std::pow(2, 17));
        proj[1][1] *= -1;

        // Получили область рендера от левого верхнего угла
        // x -1 -> 1; y 1 -> -1; z 0 -> -1
        // Правило левой руки
        // Перед полигонов определяется обходом против часовой стрелки

        glm::mat4 view = glm::mat4(1);
        // Смещаем мир относительно позиции игрока, чтобы игрок в пространстве рендера оказался в нулевых координатах
        view = glm::translate(view, -X64Delta);
        // Поворачиваем мир обратно взгляду игрока, чтобы его взгляд стал по направлению оси -z
        view = glm::mat4(Quat)*view;

        // Сначала применяется матрица вида, потом проекции
        PCO.ProjView = proj*view;
    }

    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, NodeStaticOpaquePipeline);
	vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        MainAtlas_LightMap_PipelineLayout,  0, 2, 
        (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    {
        // glm::vec4 offset = glm::inverse(Quat)*glm::vec4(0, 0, -64, 1);
        // PCO.Model = glm::translate(glm::mat4(1), glm::vec3(offset));
        PCO.Model = glm::mat4(1);
    }
    VkBuffer vkBuffer = TestQuad;
    VkDeviceSize vkOffsets = 0;

    vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    vkCmdDraw(drawCmd, 6*3*2, 1, 0, 0);

    {
        Pos::GlobalChunk x64offset = X64Offset >> Pos::Object_t::BS_Bit >> 4;
        Pos::GlobalRegion x64offset_region = x64offset >> 2;

        auto [voxelVertexs, nodeVertexs] = CP.getChunksForRender(WorldId, Pos, 1, PCO.ProjView, x64offset_region);

        {
            static uint32_t l = TOS::Time::getSeconds();
            if(l != TOS::Time::getSeconds()) {
                l = TOS::Time::getSeconds();
                TOS::Logger("Test").debug() << nodeVertexs.size();
            }
        }

        size_t count = 0;

        glm::mat4 orig = PCO.Model;
        for(auto& [chunkPos, vertexs, indexes, type, vertexCount] : nodeVertexs) {
            count += vertexCount;
            
            glm::vec3 cpos(chunkPos-x64offset);
            PCO.Model = glm::translate(orig, cpos*16.f);
            auto [vkBufferV, offsetV] = vertexs;
            auto [vkBufferI, offsetI] = indexes;

            vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
            
            VkDeviceSize offset = offsetV*sizeof(NodeVertexStatic);
            vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBufferV, &offset);
            offset = offsetI * (type ? 2 : 4);
            vkCmdBindIndexBuffer(drawCmd, vkBufferI, offset, type ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(drawCmd, vertexCount, 1, 0, 0, 0);
            //vkCmdDraw(drawCmd, vertexCount, 1, 0, 0);
        }

        PCO.Model = orig;
    }

    CP.pushFrame();
}

void VulkanRenderSession::pushStage(EnumRenderStage stage) {
}

std::vector<VoxelVertexPoint> VulkanRenderSession::generateMeshForVoxelChunks(const std::vector<VoxelCube>& cubes) {
    std::vector<VoxelVertexPoint> out;
    out.reserve(cubes.size()*6);
    
    for(const VoxelCube &cube : cubes) {
        out.emplace_back(
            cube.Pos.x,
            cube.Pos.y,
            cube.Pos.z,
            0,
            0, 0,
            cube.Size.x,
            cube.Size.z,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Pos.x,
            cube.Pos.y,
            cube.Pos.z,
            1,
            0, 0,
            cube.Size.x,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Pos.x,
            cube.Pos.y,
            cube.Pos.z,
            2,
            0, 0,
            cube.Size.z,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Pos.x,
            cube.Pos.y+cube.Size.y+1,
            cube.Pos.z,
            3,
            0, 0,
            cube.Size.x,
            cube.Size.z,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Pos.x,
            cube.Pos.y,
            cube.Pos.z+cube.Size.z+1,
            4,
            0, 0,
            cube.Size.x,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Pos.x+cube.Size.x+1,
            cube.Pos.y,
            cube.Pos.z,
            5,
            0, 0,
            cube.Size.z,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );
    }

    return out;
}

void VulkanRenderSession::updateDescriptor_MainAtlas() {
    VkDescriptorBufferInfo bufferInfo = MainTest;
    VkDescriptorImageInfo imageInfo = MainTest;

    std::vector<VkWriteDescriptorSet> ciDescriptorSet =
    {
        {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = MainAtlasDescriptor,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfo
        }, {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = MainAtlasDescriptor,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &bufferInfo
        }
    };

    vkUpdateDescriptorSets(VkInst->Graphics.Device, ciDescriptorSet.size(), ciDescriptorSet.data(), 0, nullptr);
}

void VulkanRenderSession::updateDescriptor_VoxelsLight() {
    VkDescriptorBufferInfo bufferInfo = LightDummy;
    VkDescriptorImageInfo imageInfo = LightDummy;

    std::vector<VkWriteDescriptorSet> ciDescriptorSet =
    {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VoxelLightMapDescriptor,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo
        }, {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VoxelLightMapDescriptor,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &bufferInfo
        }
    };

    vkUpdateDescriptorSets(VkInst->Graphics.Device, ciDescriptorSet.size(), ciDescriptorSet.data(), 0, nullptr);
}

void VulkanRenderSession::updateDescriptor_ChunksLight() {

}

}