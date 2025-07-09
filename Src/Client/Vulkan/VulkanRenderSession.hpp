#pragma once

#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan_core.h>
#include "Abstract.hpp"
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


class VulkanRenderSession : public IRenderSession, public IVulkanDependent {
    VK::Vulkan *VkInst = nullptr;
    // Доступ к миру на стороне клиента
    IServerSession *ServerSession = nullptr;

    // Положение камеры
    WorldId_t WorldId;
    Pos::Object Pos;
    glm::quat Quat;

    struct VulkanContext {
        AtlasImage MainTest, LightDummy;
        Buffer TestQuad;
        std::optional<Buffer> TestVoxel;


        VertexPool<VoxelVertexPoint> VertexPool_Voxels;
        VertexPool<NodeVertexStatic> VertexPool_Nodes;

        VulkanContext(Vulkan *vkInst)
            : MainTest(vkInst), LightDummy(vkInst),
                TestQuad(vkInst, sizeof(NodeVertexStatic)*6),
                VertexPool_Voxels(vkInst),
                VertexPool_Nodes(vkInst)
        {}
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
        std::unordered_map<WorldId_t, 
            std::unordered_map<Pos::GlobalChunk, 
                std::pair<VertexPool<VoxelVertexPoint>::Pointer, VertexPool<NodeVertexStatic>::Pointer> // Voxels, Nodes
            >
        > ChunkVoxelMesh;
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

    virtual void onBinaryResourceAdd(std::unordered_map<EnumBinResource, std::unordered_map<ResourceId_t, BinaryResource>>) override;
    virtual void onBinaryResourceLost(std::unordered_map<EnumBinResource, std::vector<ResourceId_t>>) override;
    virtual void onContentDefinesAdd(std::unordered_map<EnumDefContent, std::unordered_map<ResourceId_t, std::u8string>>) override;
    virtual void onContentDefinesLost(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) override;
    virtual void onChunksChange(WorldId_t worldId, const std::unordered_set<Pos::GlobalChunk>& changeOrAddList, const std::unordered_set<Pos::GlobalRegion>& remove) override;
    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube> cubes); 
    static std::vector<NodeVertexStatic> generateMeshForNodeChunks(const Node nodes[16][16][16]); 

private:
    void updateDescriptor_MainAtlas();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}