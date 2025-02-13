#pragma once
#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <glm/ext/matrix_transform.hpp>

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
    Воксели рендерятся точками, которые распаковываются в квадратные плоскости

    В чанке по оси 256 вокселей, и 257 позиций вершин (включая дальнюю границу чанка)
    9 бит на позицию *3 оси = 27 бит
    Указание материала 16 бит
*/

struct VoxelVertexPoint {
    uint32_t 
        FX : 9, FY : 9, FZ : 9, // Позиция
        Place : 3,              // Положение распространения xz, xy, zy, и обратные
        N1 : 1,                 // Не занято
        LS : 1,                 // Масштаб карты освещения (1м/16 или 1м)
        TX : 8, TY : 8,         // Размер+1
        VoxMtl : 16,            // Материал вокселя DefVoxelId_t
        LU : 14, LV : 14,       // Позиция на карте освещения
        N2 : 2;                 // Не занято
};

/*
    Из-за карт освещения индексов не будет
    Максимальный размер меша 14^3 м от центра ноды
    Координатное пространство то же, что и у вокселей + 8 позиций с двух сторон
    Рисуется полигонами
*/

struct NodeVertexStatic {
    uint32_t
        FX : 9, FY : 9, FZ : 9, // Позиция -112 ~ 369 / 16
        N1 : 4,                 // Не занято
        LS : 1,                 // Масштаб карты освещения (1м/16 или 1м)
        Tex : 18,               // Текстура
        N2 : 14,                // Не занято
        TU : 16, TV : 16;       // UV на текстуре
};

class VulkanRenderSession : public IRenderSession, public IVulkanDependent {
    VK::Vulkan *VkInst = nullptr;
    // Доступ к миру на стороне клиента
    IServerSession *ServerSession = nullptr;

    // Положение камеры
    WorldId_c WorldId;
    Pos::Object Pos;
    glm::quat Quat;

    /*
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    Текстурный атлас
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            Данные к атласу
    */
	VkDescriptorSetLayout MainAtlasDescLayout = VK_NULL_HANDLE;
    /*
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    Карта освещения
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            Информация о размерах карты для приведения размеров
    */
	VkDescriptorSetLayout LightMapDescLayout = VK_NULL_HANDLE;

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


    std::map<TextureId_c, uint16_t> ServerToAtlas;

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

    virtual void onDefTexture(TextureId_c id, std::vector<std::byte> &&info) override;
    virtual void onDefTextureLost(const std::vector<TextureId_c> &&lost) override;
    virtual void onDefModel(ModelId_c id, std::vector<std::byte> &&info) override;
    virtual void onDefModelLost(const std::vector<ModelId_c> &&lost) override;

    virtual void onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) override;
    virtual void onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) override;
    virtual void onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) override;
    virtual void onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) override;
    virtual void onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) override;

    virtual void onChunksChange(WorldId_c worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) override;
    virtual void setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);
};

}