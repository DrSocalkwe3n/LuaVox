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

        // Здесь не хватает стадии работы с текстурами
        enum class EnumStage {
            // Постройка буфера команд на рисовку
            // В этот период не должно быть изменений в таблице,
            // хранящей указатели на данные для рендера ChunkMesh
            // Можно работать с миром
            // Здесь нужно дождаться завершения работы с ChunkMesh
            ComposingCommandBuffer,
            // В этот период можно менять ChunkMesh
            // Можно работать с миром
            Render,
            // В этот период нельзя работать с миром
            // Можно менять ChunkMesh
            // Здесь нужно дождаться завершения работы с миром, только в 
            // этом этапе могут приходить события изменения чанков и определений
            WorldUpdate,

            Shutdown
        } Stage = EnumStage::Render;

        volatile bool ChunkMesh_IsUse = false, ServerSession_InUse = false;

        struct ChunkObj_t {
            // Сортированный список уникальных значений
            std::vector<DefVoxelId_t> VoxelDefines;
            VertexPool<VoxelVertexPoint>::Pointer VoxelPointer;
            std::vector<DefNodeId_t> NodeDefines;
            VertexPool<NodeVertexStatic>::Pointer NodePointer;
        };

        std::unordered_map<WorldId_t,
            std::unordered_map<Pos::GlobalRegion, std::array<ChunkObj_t, 4*4*4>>
        > ChunkMesh;

        ThreadVertexObj_t(Vulkan* vkInst)
            :   VertexPool_Voxels(vkInst),
                VertexPool_Nodes(vkInst),
                Thread(&ThreadVertexObj_t::run, this)
        {}

        ~ThreadVertexObj_t() {
            Stage = EnumStage::Shutdown;
            Thread.join();
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
        void pushStage(EnumStage stage) {
            if(Stage == EnumStage::Shutdown)
                MAKE_ERROR("Остановка из-за ошибки ThreadVertex");

            assert(Stage != stage);

            Stage = stage;
            if(stage == EnumStage::ComposingCommandBuffer) {
                while(ChunkMesh_IsUse);
            } else if(stage == EnumStage::WorldUpdate) {
                while(ServerSession_InUse);
            }
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
            ThreadVertexObj.pushStage(ThreadVertexObj_t::EnumStage::Shutdown);
        }

        void onUpdate();
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

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube> cubes); 
    static std::vector<NodeVertexStatic> generateMeshForNodeChunks(const Node* nodes); 

private:
    void updateDescriptor_MainAtlas();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}