#pragma once

#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vulkan/vulkan_core.h>
#include "Abstract.hpp"
#include "TOSLib.hpp"
#include "VertexPool.hpp"
#include "glm/fwd.hpp"
#include "../FrustumCull.h"
#include "glm/geometric.hpp"

/*
    У движка есть один текстурный атлас VK_IMAGE_VIEW_TYPE_2D_ARRAY(RGBA_UINT) и к нему Storage с инфой о положении текстур
    Это общий для всех VkDescriptorSetLayout и VkDescriptorSet

    Для отрисовки вокселей используется карта освещения VK_IMAGE_VIEW_TYPE_2D_ARRAY(RGB_UINT), разделённая по прямоугольным плоскостям
    Разрешение у этих карт 1м/16

    -- Для всего остального 3д карты освещённости по чанкам в разрешении 1м. 16^3 = 4096 текселей --
*/

/*
    Самые критичные случаи
        Для вокселей это чередование в шахматном порядке присутствия и отсутствия вокселей
        Это 8'388'608 вокселя в чанке, общей площадью на картах освещения (256^3/2 *4 *6) 201'326'592 текселей или текстура размером 16к^2
        Понадобится переиспользование одинаковых участков освещённости
        Если чанк заполнен слоями вокселей в шахматном порядке по вертикале ((257^2+257*2*4)*128) 8'717'440 текселей или текстура размером 4к^2

*/


namespace LV::Client::VK {

struct WorldPCO {
    glm::mat4 ProjView, Model;
};

static_assert(sizeof(WorldPCO) == 128);

/*
    Объект, занимающийся генерацией меша на основе нод и вокселей
    Требует доступ к профилям в ServerSession (ServerSession должен быть заблокирован только на чтение)
    Также доступ к идентификаторам текстур в VulkanRenderSession и моделей по состояниям
    Очередь чанков, ожидающих перерисовку. Возвращает готовые вершинные данные.
*/
struct ChunkMeshGenerator {
    // Данные рендера чанка
    struct ChunkObj_t {
        // Идентификатор запроса (на случай если запрос просрочился и чанк уже был удалён)
        uint32_t RequestId = 0;
        // Мир
        WorldId_t WId;
        // Позиция чанка в мире
        Pos::GlobalChunk Pos;
        // Сортированный список уникальных значений
        std::vector<DefVoxelId> VoxelDefines;
        // Вершины
        std::vector<VoxelVertexPoint> VoxelVertexs;
        // Ноды
        std::vector<DefNodeId> NodeDefines;
        // Вершины нод
        std::vector<NodeVertexStatic> NodeVertexs;
        // Индексы
        std::variant<std::vector<uint16_t>, std::vector<uint32_t>> NodeIndexes;
    };

    // Очередь чанков на перерисовку
    TOS::SpinlockObject<std::queue<std::tuple<WorldId_t, Pos::GlobalChunk, uint32_t>>> Input;
    // Выход
    TOS::SpinlockObject<std::vector<ChunkObj_t>> Output;


public:
    ChunkMeshGenerator(IServerSession* serverSession)
        :   SS(serverSession) 
    {
        assert(serverSession);
    }

    ~ChunkMeshGenerator() {
        assert(Threads.empty());
    }

    // Меняет количество обрабатывающих потоков
    void changeThreadsCount(uint8_t threads) {
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

    void prepareTickSync() {
        Sync.Stop = true;
    }

    void pushStageTickSync() {
        std::unique_lock lock(Sync.Mutex);
        Sync.CV_CountInRun.wait(lock, [&]() { return Sync.CountInRun == 0; });
    }

    void endTickSync() {
        Sync.Stop = false;
        Sync.CV_CountInRun.notify_all();
    }


private:
    struct {
        std::mutex Mutex;
        // Если нужно остановить пул потоков, вызывается NeedShutdown
        volatile bool NeedShutdown = false, Stop = false;
        volatile uint8_t CountInRun = 0;
        std::condition_variable CV_CountInRun;
    } Sync;

    IServerSession *SS;

    // Потоки
    std::vector<std::thread> Threads;

    void run(uint8_t id);
};

/*
    Модуль обрабатывает рендер чанков
*/
class ChunkPreparator {
public:
    struct TickSyncData {
        // Профили на которые повлияли изменения, по ним нужно пересчитать чанки
        std::vector<DefVoxelId> ChangedVoxels;
        std::vector<DefNodeId> ChangedNodes;

        std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> ChangedChunks;
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> LostRegions;
    };

public:
    ChunkPreparator(Vulkan* vkInst, IServerSession* serverSession) 
        :   VkInst(vkInst),
            CMG(serverSession),
            VertexPool_Voxels(vkInst),
            VertexPool_Nodes(vkInst),
            IndexPool_Nodes_16(vkInst),
            IndexPool_Nodes_32(vkInst)
    {
        assert(vkInst);
        assert(serverSession);

        CMG.changeThreadsCount(1);

        const VkCommandPoolCreateInfo infoCmdPool =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = VkInst->getSettings().QueueGraphics
        };

        vkAssert(!vkCreateCommandPool(VkInst->Graphics.Device, &infoCmdPool, nullptr, &CMDPool));
    }

    ~ChunkPreparator() {
        CMG.changeThreadsCount(0);

        if(CMDPool)
            vkDestroyCommandPool(VkInst->Graphics.Device, CMDPool, nullptr);
    }


    void prepareTickSync() {
        CMG.prepareTickSync();
    }

    void pushStageTickSync() {
        CMG.pushStageTickSync();
    }

    void tickSync(const TickSyncData& data) {
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

    // Готовность кадров определяет когда можно удалять ненужные ресурсы, которые ещё используются в рендере
    void pushFrame() {
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

    // Выдаёт буферы для рендера в порядке от ближнего к дальнему. distance - радиус в регионах
    std::pair<
        std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>>,
        std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>>
    > getChunksForRender(WorldId_t worldId, Pos::Object pos, uint8_t distance, glm::mat4 projView, Pos::GlobalRegion x64offset);

private:
    static constexpr uint8_t FRAME_COUNT_RESOURCE_LATENCY = 6;

    Vulkan* VkInst;
    VkCommandPool CMDPool = nullptr;

    // Генератор вершин чанков
    ChunkMeshGenerator CMG;

    // Буферы для хранения вершин
    VertexPool<VoxelVertexPoint> VertexPool_Voxels;
    VertexPool<NodeVertexStatic> VertexPool_Nodes;
    IndexPool<uint16_t> IndexPool_Nodes_16;
    IndexPool<uint32_t> IndexPool_Nodes_32;

    struct ChunkObj_t {
        std::vector<DefVoxelId> Voxels;
        VertexPool<VoxelVertexPoint>::Pointer VoxelPointer;
        std::vector<DefNodeId> Nodes;
        VertexPool<NodeVertexStatic>::Pointer NodePointer;
        std::variant<IndexPool<uint16_t>::Pointer, IndexPool<uint32_t>::Pointer> NodeIndexes;
    };

    // Склад указателей на вершины чанков
    std::unordered_map<WorldId_t,
        std::unordered_map<Pos::GlobalRegion, std::array<ChunkObj_t, 4*4*4>>
    > ChunksMesh;

    uint8_t FrameRoulette = 0;
    // Вершины, ожидающие удаления по прошествию какого-то количества кадров
    std::vector<VertexPool<VoxelVertexPoint>::Pointer> VPV_ToFree[FRAME_COUNT_RESOURCE_LATENCY];
    std::vector<std::tuple<
        VertexPool<NodeVertexStatic>::Pointer,
        std::variant<IndexPool<uint16_t>::Pointer, IndexPool<uint32_t>::Pointer>
    >> VPN_ToFree[FRAME_COUNT_RESOURCE_LATENCY];

    // Следующий идентификатор запроса
    uint32_t NextRequest = 0;
    // Список ожидаемых чанков. Если регион был потерян, следующая его запись получит
    // новый идентификатор (при отсутствии записи готовые чанки с MCMG будут проигнорированы)
    std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, uint32_t>> Requests;
};

/*
    Модуль, рисующий то, что предоставляет IServerSession
*/
class VulkanRenderSession : public IRenderSession {
    VK::Vulkan *VkInst = nullptr;
    // Доступ к миру на стороне клиента
    IServerSession *ServerSession = nullptr;

    // Положение камеры
    WorldId_t WorldId;
    Pos::Object Pos;
    /*
        Графический конвейер оперирует числами с плавающей запятой
        Для сохранения точности матрица модели хранит смещения близкие к нулю (X64Delta)
        глобальные смещения на уровне региона исключаются из смещения ещё при задании матрицы модели

        X64Offset = позиция игрока на уровне регионов
        X64Delta = позиция игрока в рамках региона

        Внутри графического конвейера будут числа приблежённые к 0
    */
    // Смещение дочерних объекто на стороне хоста перед рендером
    Pos::Object X64Offset;
    glm::vec3 X64Offset_f, X64Delta;        // Смещение мира относительно игрока в матрице вида (0 -> 64)
    glm::quat Quat;

    ChunkPreparator CP;

    AtlasImage MainTest, LightDummy;
    Buffer TestQuad;
    std::optional<Buffer> TestVoxel;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

    /*
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    Текстурный атлас
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            Данные к атласу
    */
	VkDescriptorSetLayout MainAtlasDescLayout = VK_NULL_HANDLE;
    VkDescriptorSet MainAtlasDescriptor = VK_NULL_HANDLE;
    /*
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    Воксельная карта освещения
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            Информация о размерах карты для приведения размеров
    */
	VkDescriptorSetLayout VoxelLightMapDescLayout = VK_NULL_HANDLE;
    VkDescriptorSet VoxelLightMapDescriptor = VK_NULL_HANDLE;

    // Для отрисовки с использованием текстурного атласа и карты освещения
    VkPipelineLayout MainAtlas_LightMap_PipelineLayout = VK_NULL_HANDLE;

    // Для отрисовки вокселей
	std::shared_ptr<ShaderModule> VoxelShaderVertex, VoxelShaderGeometry, VoxelShaderFragmentOpaque, VoxelShaderFragmentTransparent;
    VkPipeline
        VoxelOpaquePipeline = VK_NULL_HANDLE,         // Альфа канал может быть либо 255, либо 0
        VoxelTransparentPipeline = VK_NULL_HANDLE;    // Допускается полупрозрачность и смешивание

    // Для отрисовки статичных, не анимированных нод
	std::shared_ptr<ShaderModule> NodeShaderVertex, NodeShaderGeometry, NodeShaderFragmentOpaque, NodeShaderFragmentTransparent;
    VkPipeline
        NodeStaticOpaquePipeline = VK_NULL_HANDLE,
        NodeStaticTransparentPipeline = VK_NULL_HANDLE;

    std::map<AssetsTexture, uint16_t> ServerToAtlas;

public:
    WorldPCO PCO;
    WorldId_t WI = 0;
    glm::vec3 PlayerPos = glm::vec3(0);

public:
    VulkanRenderSession(Vulkan *vkInst, IServerSession *serverSession);
    virtual ~VulkanRenderSession();

    virtual void prepareTickSync() override;
    virtual void pushStageTickSync() override;
    virtual void tickSync(const TickSyncData& data) override;

    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);
    void pushStage(EnumRenderStage stage);

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube>& cubes);

private:
    void updateDescriptor_MainAtlas();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}