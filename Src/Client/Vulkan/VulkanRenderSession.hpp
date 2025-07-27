#pragma once

#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#include "Abstract.hpp"
#include "TOSLib.hpp"
#include "VertexPool.hpp"
#include "glm/fwd.hpp"
#include "../FrustumCull.h"

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
    Модуль, рисующий то, что предоставляет IServerSession
*/
class VulkanRenderSession : public IRenderSession, public IVulkanDependent {
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

    /*
        Поток, занимающийся генерацией меша на основе нод и вокселей
        Требует доступ к профилям в ServerSession (ServerSession должен быть заблокирован только на чтение)
        Также доступ к идентификаторам текстур в VulkanRenderSession (только на чтение)
        Должен оповещаться об изменениях профилей и событий чанков
        Удалённые мешы хранятся в памяти N количество кадров
    */
    struct ThreadVertexObj_t {
        // Сессия будет выдана позже
        // Предполагается что события будут только после того как сессия будет установлена,
        // соответственно никто не попытаеся сюда обратится без событий
        IServerSession *SSession = nullptr;
        Vulkan *VkInst;
        VkCommandPool CMDPool = nullptr;

        // Здесь не хватает стадии работы с текстурами
        struct StateObj_t {
            EnumRenderStage Stage = EnumRenderStage::Render;
            volatile bool ChunkMesh_IsUse = false, ServerSession_InUse = false;
        };

        SpinlockObject<StateObj_t> State;

        struct ChunkObj_t {
            // Сортированный список уникальных значений
            std::vector<DefVoxelId_t> VoxelDefines;
            VertexPool<VoxelVertexPoint>::Pointer VoxelPointer;
            std::vector<DefNodeId_t> NodeDefines;
            VertexPool<NodeVertexStatic>::Pointer NodePointer;
        };

        ThreadVertexObj_t(Vulkan* vkInst)
            :   VkInst(vkInst),
                VertexPool_Voxels(vkInst),
                VertexPool_Nodes(vkInst),
                Thread(&ThreadVertexObj_t::run, this)
        {
            const VkCommandPoolCreateInfo infoCmdPool =
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = VkInst->getSettings().QueueGraphics
            };

            vkAssert(!vkCreateCommandPool(VkInst->Graphics.Device, &infoCmdPool, nullptr, &CMDPool));
        }

        ~ThreadVertexObj_t() {
            State.lock()->Stage = EnumRenderStage::Shutdown;
            Thread.join();

            if(CMDPool)
                vkDestroyCommandPool(VkInst->Graphics.Device, CMDPool, nullptr);
        }

        // Сюда входят добавленные/изменённые/удалённые определения нод и вокселей
        // Чтобы перерисовать чанки, связанные с ними
        void onContentDefinesChange(const std::vector<DefVoxelId_t>& voxels, const std::vector<DefNodeId_t>& nodes) {
            ChangedDefines_Voxel.insert(ChangedDefines_Voxel.end(), voxels.begin(), voxels.end());
            ChangedDefines_Node.insert(ChangedDefines_Node.end(), nodes.begin(), nodes.end());
        }

        // Изменение/удаление чанков
        void onContentChunkChange(const std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>>& chunkChanges, const std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>>& regionRemove) {
            for(auto& [worldId, chunks] : chunkChanges) {
                auto &list = ChangedContent_Chunk[worldId];
                list.insert(list.end(), chunks.begin(), chunks.end());
            }

            for(auto& [worldId, regions] : regionRemove) {
                auto &list = ChangedContent_RegionRemove[worldId];
                list.insert(list.end(), regions.begin(), regions.end());
            }
        }

        // Синхронизация потока рендера мира
        void pushStage(EnumRenderStage stage) {
            auto lock = State.lock();

            if(lock->Stage == EnumRenderStage::Shutdown)
                MAKE_ERROR("Остановка из-за ошибки ThreadVertex");

            assert(lock->Stage != stage);

            lock->Stage = stage;

            if(stage == EnumRenderStage::ComposingCommandBuffer) {
                if(lock->ChunkMesh_IsUse) {
                    lock.unlock();
                    while(State.get_read().ChunkMesh_IsUse);
                } else
                    lock.unlock();
            } else if(stage == EnumRenderStage::WorldUpdate) {
                if(lock->ServerSession_InUse) {
                    lock.unlock();
                    while(State.get_read().ServerSession_InUse);
                } else
                    lock.unlock();
            } else if(stage == EnumRenderStage::Shutdown) {
                if(lock->ServerSession_InUse || lock->ChunkMesh_IsUse) {
                    lock.unlock();
                    while(State.get_read().ServerSession_InUse);
                    while(State.get_read().ChunkMesh_IsUse);
                } else
                    lock.unlock();
            }

        }

        std::pair<
            std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>>,
            std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>>
        > getChunksForRender(WorldId_t worldId, Pos::Object pos, uint8_t distance, glm::mat4 projView, Pos::GlobalRegion x64offset) {
            Pos::GlobalRegion center = pos >> Pos::Object_t::BS_Bit >> 4 >> 2;

            std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>> vertexVoxels;
            std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>> vertexNodes;

            auto iterWorld = ChunkMesh.find(worldId);
            if(iterWorld == ChunkMesh.end())
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

                            if(chunk.VoxelPointer)
                                vertexVoxels.emplace_back(local+Pos::GlobalChunk(localPos), VertexPool_Voxels.map(chunk.VoxelPointer), chunk.VoxelPointer.VertexCount);
                            if(chunk.NodePointer)
                                vertexNodes.emplace_back(local+Pos::GlobalChunk(localPos), VertexPool_Nodes.map(chunk.NodePointer), chunk.NodePointer.VertexCount);
                        }
                    }
                }
            }

            return std::pair{vertexVoxels, vertexNodes};
        }

    private:
        // Буферы для хранения вершин
        VertexPool<VoxelVertexPoint> VertexPool_Voxels;
        VertexPool<NodeVertexStatic> VertexPool_Nodes;
        // Списки изменённых определений
        std::vector<DefVoxelId_t> ChangedDefines_Voxel;
        std::vector<DefNodeId_t> ChangedDefines_Node;
        // Список чанков на перерисовку
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> ChangedContent_Chunk; 
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> ChangedContent_RegionRemove;
        // Меши
        std::unordered_map<WorldId_t,
            std::unordered_map<Pos::GlobalRegion, std::array<ChunkObj_t, 4*4*4>>
        > ChunkMesh;
        // Внешний поток
        std::thread Thread;

        void run();
    };

    struct VulkanContext {
        VK::Vulkan *VkInst;
        AtlasImage MainTest, LightDummy;
        Buffer TestQuad;
        std::optional<Buffer> TestVoxel;

        ThreadVertexObj_t ThreadVertexObj;

        VulkanContext(Vulkan* vkInst)
            :   VkInst(vkInst),
                MainTest(vkInst), LightDummy(vkInst),
                TestQuad(vkInst, sizeof(NodeVertexStatic)*6*3*2),
                ThreadVertexObj(vkInst)
        {}

        ~VulkanContext() {
        }

        void onUpdate();

        void setServerSession(IServerSession* ssession) {
            ThreadVertexObj.SSession = ssession;
        }
    };

    std::shared_ptr<VulkanContext> VKCTX;

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

    std::map<BinTextureId_t, uint16_t> ServerToAtlas;

    struct {
    } External;

    virtual void free(Vulkan *instance) override;
    virtual void init(Vulkan *instance) override;

public:
    WorldPCO PCO;

public:
    VulkanRenderSession();
    virtual ~VulkanRenderSession();

    void setServerSession(IServerSession *serverSession) {
        ServerSession = serverSession;
        if(VKCTX)
            VKCTX->setServerSession(serverSession);
        assert(serverSession);
    }

    virtual void onBinaryResourceAdd(std::vector<Hash_t>) override;
    virtual void onContentDefinesAdd(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) override;
    virtual void onContentDefinesLost(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) override;
    virtual void onChunksChange(WorldId_t worldId, const std::unordered_set<Pos::GlobalChunk>& changeOrAddList, const std::unordered_set<Pos::GlobalRegion>& remove) override;
    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);
    void pushStage(EnumRenderStage stage);

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube> cubes); 
    static std::vector<NodeVertexStatic> generateMeshForNodeChunks(const Node* nodes); 

private:
    void updateDescriptor_MainAtlas();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}