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
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>

namespace LV::Client::VK {

void VulkanRenderSession::ThreadVertexObj_t::run() {
    Logger LOG = "ThreadVertex";
    LOG.debug() << "Старт потока подготовки чанков к рендеру";

    // Контейнеры событий
    std::vector<DefVoxelId_t> changedDefines_Voxel;
    std::vector<DefNodeId_t> changedDefines_Node;
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> changedContent_Chunk;
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> changedContent_RegionRemove;

    // Контейнер новых мешей
    std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_map<Pos::bvec4u, ChunkObj_t>>> chunksUpdate;

    // std::vector<VertexPool<VoxelVertexPoint>::Pointer> ToDelete_Voxels;
    // std::vector<VertexPool<VoxelVertexPoint>::Pointer> ToDelete_Nodes;


    try {
        while(State.get_read().Stage != EnumRenderStage::Shutdown) {
            bool hasWork = false, hasVertexChanges = false;
            auto lock = State.lock();

            // uint64_t now = 0;

            if((!changedContent_RegionRemove.empty() || !chunksUpdate.empty())
                && (lock->Stage == EnumRenderStage::Render || lock->Stage == EnumRenderStage::WorldUpdate))
            {
                // Здесь можно выгрузить готовые данные в ChunkMesh
                lock->ChunkMesh_IsUse = true;
                lock.unlock();
                hasWork = true;
                // now = Time::nowSystem();

                // Удаляем регионы
                std::vector<WorldId_t> toRemove;
                for(auto& [worldId, regions] : changedContent_RegionRemove) {
                    auto iterWorld = ChunkMesh.find(worldId);
                    if(iterWorld == ChunkMesh.end())
                        continue;

                    for(Pos::GlobalRegion regionPos : regions) {
                        auto iterRegion = iterWorld->second.find(regionPos);
                        if(iterRegion == iterWorld->second.end())
                            continue;

                        for(size_t index = 0; index < iterRegion->second.size(); index++) {
                            auto &chunk = iterRegion->second[index];
                            chunk.NodeDefines.clear();
                            chunk.VoxelDefines.clear();

                            VertexPool_Voxels.dropVertexs(chunk.VoxelPointer);
                            VertexPool_Nodes.dropVertexs(chunk.NodePointer);
                        }

                        iterWorld->second.erase(iterRegion);
                    }

                    if(iterWorld->second.empty())
                        toRemove.push_back(worldId);
                }

                for(WorldId_t worldId : toRemove)
                    ChunkMesh.erase(ChunkMesh.find(worldId));

                // Добавляем обновлённые меши
                for(auto& [worldId, regions] : chunksUpdate) {
                    auto &world = ChunkMesh[worldId];

                    for(auto& [regionPos, chunks] : regions) {
                        auto &region = world[regionPos];
                        
                        for(auto& [chunkPos, chunk] : chunks) {
                            auto &drawChunk = region[chunkPos.pack()];
                            VertexPool_Voxels.dropVertexs(drawChunk.VoxelPointer);
                            VertexPool_Nodes.dropVertexs(drawChunk.NodePointer);
                            drawChunk = std::move(chunk);
                        }
                    }
                }


                State.lock()->ChunkMesh_IsUse = false;
                chunksUpdate.clear();

                // LOG.debug() << "ChunkMesh_IsUse: " << Time::nowSystem() - now;

                lock = State.lock();
            }

            if((!changedContent_Chunk.empty())
                && (lock->Stage == EnumRenderStage::ComposingCommandBuffer || lock->Stage == EnumRenderStage::Render))
            {
                // Здесь можно обработать события, и подготовить меши по данным с мира
                lock->ServerSession_InUse = true;
                lock.unlock();
                // now = Time::nowSystem();
                hasWork = true;
                // changedContent_Chunk

                std::vector<WorldId_t> toRemove;
                for(auto& [worldId, chunks] : changedContent_Chunk) {
                    while(!chunks.empty() && (State.get_read().Stage == EnumRenderStage::ComposingCommandBuffer || State.get_read().Stage == EnumRenderStage::Render)) {
                        auto& chunkPos = chunks.back();
                        Pos::GlobalRegion regionPos = chunkPos >> 2;
                        Pos::bvec4u localPos = chunkPos & 0x3;
                        
                        auto& drawChunk = chunksUpdate[worldId][regionPos][localPos];
                        {
                            auto iterWorld = SSession->Data.Worlds.find(worldId);
                            if(iterWorld == SSession->Data.Worlds.end())
                                goto skip;

                            auto iterRegion = iterWorld->second.Regions.find(regionPos);
                            if(iterRegion == iterWorld->second.Regions.end())
                                goto skip;

                            auto& chunk = iterRegion->second.Chunks[localPos.pack()];

                            {
                                drawChunk.VoxelDefines.resize(chunk.Voxels.size());
                                for(size_t index = 0; index < chunk.Voxels.size(); index++)
                                    drawChunk.VoxelDefines[index] = chunk.Voxels[index].VoxelId;
                                std::sort(drawChunk.VoxelDefines.begin(), drawChunk.VoxelDefines.end());
                                auto last = std::unique(drawChunk.VoxelDefines.begin(), drawChunk.VoxelDefines.end());
                                drawChunk.VoxelDefines.erase(last, drawChunk.VoxelDefines.end());
                                drawChunk.VoxelDefines.shrink_to_fit();
                            }

                            {
                                drawChunk.NodeDefines.resize(chunk.Nodes.size());
                                for(size_t index = 0; index < chunk.Nodes.size(); index++)
                                    drawChunk.NodeDefines[index] = chunk.Nodes[index].NodeId;
                                std::sort(drawChunk.NodeDefines.begin(), drawChunk.NodeDefines.end());
                                auto last = std::unique(drawChunk.NodeDefines.begin(), drawChunk.NodeDefines.end());
                                drawChunk.NodeDefines.erase(last, drawChunk.NodeDefines.end());
                                drawChunk.NodeDefines.shrink_to_fit();
                            }

                            {
                                std::vector<VoxelVertexPoint> voxels = generateMeshForVoxelChunks(chunk.Voxels);
                                if(!voxels.empty()) {
                                    drawChunk.VoxelPointer = VertexPool_Voxels.pushVertexs(std::move(voxels));
                                    hasVertexChanges = true;
                                }
                            }

                            {
                                std::vector<NodeVertexStatic> nodes = generateMeshForNodeChunks(chunk.Nodes.data());
                                if(!nodes.empty()) {
                                    drawChunk.NodePointer = VertexPool_Nodes.pushVertexs(std::move(nodes));
                                    hasVertexChanges = true;
                                }
                            }
                        }

                        skip:

                        chunks.pop_back();
                    }

                    if(chunks.empty())
                        toRemove.push_back(worldId);
                }

                State.lock()->ServerSession_InUse = false;

                for(WorldId_t worldId : toRemove)
                    changedContent_Chunk.erase(changedContent_Chunk.find(worldId));

                // LOG.debug() << "ServerSession_InUse: " << Time::nowSystem() - now;
                lock = State.lock();
            }

            if((!ChangedContent_Chunk.empty() || !ChangedContent_RegionRemove.empty()
                || !ChangedDefines_Voxel.empty() || !ChangedDefines_Node.empty())
                    && (lock->Stage != EnumRenderStage::WorldUpdate))
            {
                // Переносим все события в локальные хранилища
                lock->ServerSession_InUse = true;
                lock.unlock();
                // now = Time::nowSystem();
                hasWork = true;

                if(!ChangedContent_Chunk.empty()) {
                    for(auto& [worldId, chunks] : ChangedContent_Chunk) {
                        auto &list = changedContent_Chunk[worldId];
                        list.insert(list.end(), chunks.begin(), chunks.end());
                    }

                    ChangedContent_Chunk.clear();
                }

                if(!ChangedContent_RegionRemove.empty()) {
                    for(auto& [worldId, chunks] : ChangedContent_RegionRemove) {
                        auto &list = changedContent_RegionRemove[worldId];
                        list.insert(list.end(), chunks.begin(), chunks.end());
                    }

                    ChangedContent_RegionRemove.clear();
                }

                if(!ChangedDefines_Voxel.empty()) {
                    changedDefines_Voxel.insert(changedDefines_Voxel.end(), ChangedDefines_Voxel.begin(), ChangedDefines_Voxel.end());
                    ChangedDefines_Voxel.clear();

                    std::sort(changedDefines_Voxel.begin(), changedDefines_Voxel.end());
                    auto last = std::unique(changedDefines_Voxel.begin(), changedDefines_Voxel.end());
                    changedDefines_Voxel.erase(last, changedDefines_Voxel.end());
                }

                if(!ChangedDefines_Node.empty()) {
                    changedDefines_Node.insert(changedDefines_Node.end(), ChangedDefines_Node.begin(), ChangedDefines_Voxel.end());
                    ChangedDefines_Node.clear();

                    std::sort(changedDefines_Node.begin(), changedDefines_Node.end());
                    auto last = std::unique(changedDefines_Node.begin(), changedDefines_Node.end());
                    changedDefines_Node.erase(last, changedDefines_Node.end());
                }

                State.lock()->ServerSession_InUse = false;

                // Ищем чанки, которые нужно перерисовать
                if(!changedDefines_Voxel.empty() || !changedDefines_Node.empty()) {
                    for(auto& [worldId, regions] : ChunkMesh) {
                        for(auto& [regionPos, chunks] : regions) {
                            for(size_t index = 0; index < chunks.size(); index++) {
                                if(!changedDefines_Voxel.empty() && !chunks[index].VoxelDefines.empty()) {
                                    bool hasIntersection = false;
                                    {
                                        size_t i = 0, j = 0;
                                        while(i < changedDefines_Voxel.size() && j < chunks[index].VoxelDefines.size()) {
                                            if (changedDefines_Voxel[i] == chunks[index].VoxelDefines[j]) {
                                                hasIntersection = true;
                                                break;
                                            } else if (changedDefines_Voxel[i] < chunks[index].VoxelDefines[j])
                                                ++i;
                                            else
                                                ++j;
                                        }
                                    }

                                    if(hasIntersection) {
                                        changedContent_Chunk[worldId].push_back((Pos::GlobalChunk(regionPos) << 2) + Pos::GlobalChunk(Pos::bvec4u().unpack(index)));
                                    }
                                }
                                    
                                if(!changedDefines_Node.empty() && !chunks[index].NodeDefines.empty()) {
                                    bool hasIntersection = false;
                                    {
                                        size_t i = 0, j = 0;
                                        while(i < changedDefines_Node.size() && j < chunks[index].NodeDefines.size()) {
                                            if (changedDefines_Node[i] == chunks[index].NodeDefines[j]) {
                                                hasIntersection = true;
                                                break;
                                            } else if (changedDefines_Node[i] < chunks[index].NodeDefines[j])
                                                ++i;
                                            else
                                                ++j;
                                        }
                                    }

                                    if(hasIntersection) {
                                        changedContent_Chunk[worldId].push_back((Pos::GlobalChunk(regionPos) << 2) + Pos::GlobalChunk(Pos::bvec4u().unpack(index)));
                                    }
                                }
                            }
                        }
                    }
                }

                changedDefines_Voxel.clear();
                changedDefines_Node.clear();

                // Уникализируем
                for(auto& [worldId, chunks] : changedContent_Chunk) {
                    std::sort(chunks.begin(), chunks.end());
                    auto last = std::unique(chunks.begin(), chunks.end());
                    chunks.erase(last, chunks.end());
                }

                for(auto& [worldId, regions] : changedContent_RegionRemove) {
                    std::sort(regions.begin(), regions.end());
                    auto last = std::unique(regions.begin(), regions.end());
                    regions.erase(last, regions.end());
                }

                // LOG.debug() << "WorldUpdate: " << Time::nowSystem() - now;
                lock = State.lock();
            }

            lock.unlock();

            if(hasVertexChanges) {
                VertexPool_Voxels.update(CMDPool);
                VertexPool_Nodes.update(CMDPool);
            }

            if(!hasWork)
                Time::sleep3(3);
        }
    } catch(const std::exception &exc) {
        LOG.error() << exc.what();
    }

    auto lock = State.lock();
    lock->Stage = EnumRenderStage::Shutdown;
    lock->ChunkMesh_IsUse = false;
    lock->ServerSession_InUse = false;

    LOG.debug() << "Завершение потока подготовки чанков к рендеру";
}

void VulkanRenderSession::VulkanContext::onUpdate() {
    // {
    // Сделать отдельный пул комманд?
    //     auto lock = ThreadVertexObj.lock();
    //     lock->VertexPool_Voxels.update(VkInst->Graphics.Pool);
    //     lock->VertexPool_Nodes.update(VkInst->Graphics.Pool);
    // }

    MainTest.atlasUpdateDynamicData();
    LightDummy.atlasUpdateDynamicData();
}

VulkanRenderSession::VulkanRenderSession()
{
}

VulkanRenderSession::~VulkanRenderSession() {
}

void VulkanRenderSession::free(Vulkan *instance) {
	if(instance && instance->Graphics.Device)
	{
		if(VoxelOpaquePipeline)
			vkDestroyPipeline(instance->Graphics.Device, VoxelOpaquePipeline, nullptr);
		if(VoxelTransparentPipeline)
			vkDestroyPipeline(instance->Graphics.Device, VoxelTransparentPipeline, nullptr);
		if(NodeStaticOpaquePipeline)
			vkDestroyPipeline(instance->Graphics.Device, NodeStaticOpaquePipeline, nullptr);
		if(NodeStaticTransparentPipeline)
			vkDestroyPipeline(instance->Graphics.Device, NodeStaticTransparentPipeline, nullptr);

		if(MainAtlas_LightMap_PipelineLayout)
			vkDestroyPipelineLayout(instance->Graphics.Device, MainAtlas_LightMap_PipelineLayout, nullptr);
            
		if(MainAtlasDescLayout)
			vkDestroyDescriptorSetLayout(instance->Graphics.Device, MainAtlasDescLayout, nullptr);
		if(VoxelLightMapDescLayout)
			vkDestroyDescriptorSetLayout(instance->Graphics.Device, VoxelLightMapDescLayout, nullptr);
    
        if(DescriptorPool)
            vkDestroyDescriptorPool(instance->Graphics.Device, DescriptorPool, nullptr);
    }

    VoxelOpaquePipeline = VK_NULL_HANDLE;
    VoxelTransparentPipeline = VK_NULL_HANDLE;
    NodeStaticOpaquePipeline = VK_NULL_HANDLE;
    NodeStaticTransparentPipeline = VK_NULL_HANDLE;

    MainAtlas_LightMap_PipelineLayout = VK_NULL_HANDLE;

    MainAtlasDescLayout = VK_NULL_HANDLE;
    VoxelLightMapDescLayout = VK_NULL_HANDLE;

    DescriptorPool = VK_NULL_HANDLE;
    MainAtlasDescriptor = VK_NULL_HANDLE;
    VoxelLightMapDescriptor = VK_NULL_HANDLE;

    VKCTX = nullptr;
}

void VulkanRenderSession::init(Vulkan *instance) {
    if(VkInst != instance) {
        VkInst = instance;
    }

    // Разметка дескрипторов
    if(!DescriptorPool) {
        std::vector<VkDescriptorPoolSize> pool_sizes = {
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 3},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}
        };

        VkDescriptorPoolCreateInfo descriptor_pool = {};
        descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptor_pool.maxSets = 8;
        descriptor_pool.poolSizeCount = (uint32_t) pool_sizes.size();
        descriptor_pool.pPoolSizes = pool_sizes.data();

        vkAssert(!vkCreateDescriptorPool(VkInst->Graphics.Device, &descriptor_pool, nullptr,
                                     &DescriptorPool));
    }

    if(!MainAtlasDescLayout) {
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
            instance->Graphics.Device, &descriptorLayout, nullptr, &MainAtlasDescLayout));
    }

    if(!MainAtlasDescriptor) {
        VkDescriptorSetAllocateInfo ciAllocInfo =
        {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &MainAtlasDescLayout
        };

        vkAssert(!vkAllocateDescriptorSets(instance->Graphics.Device, &ciAllocInfo, &MainAtlasDescriptor));
    }

    if(!VoxelLightMapDescLayout) {
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

        vkAssert(!vkCreateDescriptorSetLayout( instance->Graphics.Device, &descriptorLayout, nullptr, &VoxelLightMapDescLayout));
    }

    if(!VoxelLightMapDescriptor) {
        VkDescriptorSetAllocateInfo ciAllocInfo =
        {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &VoxelLightMapDescLayout
        };

        vkAssert(!vkAllocateDescriptorSets(instance->Graphics.Device, &ciAllocInfo, &VoxelLightMapDescriptor));
    }

    std::vector<VkPushConstantRange> worldWideShaderPushConstants =
    {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
            .offset = 0,
            .size = uint32_t(sizeof(WorldPCO))
        }
    };

    if(!VKCTX) {
        VKCTX = std::make_shared<VulkanContext>(VkInst);
        
        VKCTX->MainTest.atlasAddCallbackOnUniformChange([this]() -> bool {
            updateDescriptor_MainAtlas();
            return true;
        });

        VKCTX->LightDummy.atlasAddCallbackOnUniformChange([this]() -> bool {
            updateDescriptor_VoxelsLight();
            return true;
        });

        {
            uint16_t texId = VKCTX->MainTest.atlasAddTexture(2, 2);
            uint32_t colors[4] = {0xfffffffful, 0x00fffffful, 0xffffff00ul, 0xff00fffful};
            VKCTX->MainTest.atlasChangeTextureData(texId, (const uint32_t*) colors);
        }

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
            uint16_t texId = VKCTX->MainTest.atlasAddTexture(width, height);
            VKCTX->MainTest.atlasChangeTextureData(texId, (const uint32_t*) image.data());
        }

        /*
        x left -1 ~ right 1
        y up 1 ~ down -1
        z near 0 ~ far -1

        glm

        */

        {
            NodeVertexStatic *array = (NodeVertexStatic*) VKCTX->TestQuad.mapMemory();
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

            VKCTX->TestQuad.unMapMemory();
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
                VKCTX->TestVoxel.emplace(VkInst, vertexs.size()*sizeof(VoxelVertexPoint));
                std::copy(vertexs.data(), vertexs.data()+vertexs.size(), (VoxelVertexPoint*) VKCTX->TestVoxel->mapMemory());
                VKCTX->TestVoxel->unMapMemory();
            }
        }
    }

    VKCTX->setServerSession(ServerSession);

    updateDescriptor_MainAtlas();
    updateDescriptor_VoxelsLight();
    updateDescriptor_ChunksLight();

    // Разметка графических конвейеров
    if(!MainAtlas_LightMap_PipelineLayout) {
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
        
		vkAssert(!vkCreatePipelineLayout(instance->Graphics.Device, &pPipelineLayoutCreateInfo, nullptr, &MainAtlas_LightMap_PipelineLayout));
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

    VkPipelineCacheCreateInfo infoPipelineCache;
    memset(&infoPipelineCache, 0, sizeof(infoPipelineCache));
    infoPipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    // Конвейеры для вокселей
    if(!VoxelOpaquePipeline || !VoxelTransparentPipeline
            || !NodeStaticOpaquePipeline || !NodeStaticTransparentPipeline)
    {
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
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
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
            .renderPass = instance->Graphics.RenderPass,
            .subpass = 0,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = 0
        };

        if(!VoxelOpaquePipeline)
            vkAssert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelOpaquePipeline));

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

           vkAssert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelTransparentPipeline));
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
            vkAssert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE,
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

            vkAssert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 
                1, &pipeline, nullptr, &NodeStaticTransparentPipeline));
        }
    }
}

void VulkanRenderSession::onBinaryResourceAdd(std::vector<Hash_t>) {

}

void VulkanRenderSession::onContentDefinesAdd(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) {

}

void VulkanRenderSession::onContentDefinesLost(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) {

}

void VulkanRenderSession::onChunksChange(WorldId_t worldId, const std::unordered_set<Pos::GlobalChunk>& changeOrAddList, const std::unordered_set<Pos::GlobalRegion>& remove) {
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> chunkChanges;
    if(!changeOrAddList.empty())
        chunkChanges[worldId] = std::vector<Pos::GlobalChunk>(changeOrAddList.begin(), changeOrAddList.end());
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> regionRemove;
    if(!remove.empty())
        regionRemove[worldId] = std::vector<Pos::GlobalRegion>(remove.begin(), remove.end());
    VKCTX->ThreadVertexObj.onContentChunkChange(chunkChanges, regionRemove);
    
        // if(chunk.Voxels.empty()) {
        //     VKCTX->VertexPool_Voxels.dropVertexs(std::get<0>(buffers));
        // } else {
        //     std::vector<VoxelVertexPoint> vertexs = generateMeshForVoxelChunks(chunk.Voxels);
        //     auto &voxels = std::get<0>(buffers);
        //     VKCTX->VertexPool_Voxels.relocate(voxels, std::move(vertexs));
        // }

        // std::vector<NodeVertexStatic> vertexs2 = generateMeshForNodeChunks(chunk.Nodes.data());

        // if(vertexs2.empty()) {
        //     VKCTX->VertexPool_Nodes.dropVertexs(std::get<1>(buffers));
        // } else {
        //     auto &nodes = std::get<1>(buffers);
        //     VKCTX->VertexPool_Nodes.relocate(nodes, std::move(vertexs2));
        // }
        
        // if(!std::get<0>(buffers) && !std::get<1>(buffers)) {
        //     auto iter = table.find(pos);
        //     if(iter != table.end())
        //         table.erase(iter);
        // }
}


void VulkanRenderSession::setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) {
    WorldId = worldId;
    Pos = pos;
    Quat = quat;
}

void VulkanRenderSession::beforeDraw() {
    if(VKCTX) {
        VKCTX->onUpdate();
    }
}

void VulkanRenderSession::drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd) {
    {
        X64Offset = Pos & ~((1 << Pos::Object_t::BS_Bit << 4 << 2)-1);
        X64Offset_f = glm::vec3(X64Offset >> Pos::Object_t::BS_Bit);
        X64Delta = glm::vec3(Pos-X64Offset) / float(Pos::Object_t::BS);
    }

    // Сместить в координаты игрока, повернуть относительно взгляда проецировать на экран
    // Изначально взгляд в z-1
    // PCO.ProjView = glm::mat4(1);
    // PCO.ProjView = glm::translate(PCO.ProjView, -glm::vec3(Pos.x, Pos.y, Pos.z)/float(Pos::Object_t::BS));
    // PCO.ProjView = proj*glm::mat4(Quat)*PCO.ProjView;
    // PCO.Model = glm::mat4(1);

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

    PCO.Model = glm::mat4(1);
    VkBuffer vkBuffer = VKCTX->TestQuad;
    VkDeviceSize vkOffsets = 0;

    vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    vkCmdDraw(drawCmd, 6*3*2, 1, 0, 0);

    {
        Pos::GlobalChunk x64offset = X64Offset >> Pos::Object_t::BS_Bit >> 4;
        Pos::GlobalRegion x64offset_region = x64offset >> 2;

        auto [voxelVertexs, nodeVertexs] = VKCTX->ThreadVertexObj.getChunksForRender(WorldId, Pos, 2, PCO.ProjView, x64offset_region);

        glm::mat4 orig = PCO.Model;
        for(auto& [chunkPos, vertexs, vertexCount] : nodeVertexs) {
            glm::vec3 cpos(chunkPos-x64offset);
            PCO.Model = glm::translate(orig, cpos*16.f);
            auto [vkBuffer, offset] = vertexs;

            vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
            vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
            vkCmdDraw(drawCmd, vertexCount, 1, offset, 0);
            
        }

        PCO.Model = orig;

        // auto iterWorld = External.ChunkVoxelMesh.find(WorldId);
        // if(iterWorld != External.ChunkVoxelMesh.end()) {
        //     glm::mat4 orig = PCO.Model;

        //     for(auto &pair : iterWorld->second) {
        //         if(auto& nodes = std::get<1>(pair.second)) {
        //             glm::vec3 cpos(pair.first-x64offset);
        //             PCO.Model = glm::translate(orig, cpos*16.f);
        //             auto [vkBuffer, offset] = VKCTX->VertexPool_Nodes.map(nodes);

        //             vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
        //                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
        //             vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
        //             vkCmdDraw(drawCmd, nodes.VertexCount, 1, offset, 0);
        //         }
        //     }

        //     PCO.Model = orig;
        // }
    }
}

void VulkanRenderSession::pushStage(EnumRenderStage stage) {
    if(!VKCTX)
        return;

    VKCTX->ThreadVertexObj.pushStage(stage);
}

std::vector<VoxelVertexPoint> VulkanRenderSession::generateMeshForVoxelChunks(const std::vector<VoxelCube> cubes) {
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

std::vector<NodeVertexStatic> VulkanRenderSession::generateMeshForNodeChunks(const Node* nodes) {
    std::vector<NodeVertexStatic> out;
    NodeVertexStatic v;

    for(int z = 0; z < 16; z++)
        for(int y = 0; y < 16; y++)
            for(int x = 0; x < 16; x++)
        {
            size_t index = Pos::bvec16u(x, y, z).pack();
            if(nodes[index].Data == 0)
                continue;

            v.Tex = nodes[index].NodeId;

            if((y+1) >= 16 || nodes[Pos::bvec16u(x, y+1, z).pack()].NodeId == 0) {
                v.FX = 135+x*16;
                v.FY = 135+y*16+16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FZ -= 16;
                v.TV = 65535;
                out.push_back(v);

                v.FX = 135+x*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.FZ -= 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FX -= 16;
                v.TU = 0;
                out.push_back(v);
            }

            if((y-1) < 0 || nodes[Pos::bvec16u(x, y-1, z).pack()].NodeId == 0) {
                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FZ -= 16;
                v.TV = 65535;
                out.push_back(v);

                v.FX += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FX = 135+x*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.FZ -= 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FZ += 16;
                v.TV = 0;
                out.push_back(v);
            }

            if((x+1) >= 16 || nodes[Pos::bvec16u(x+1, y, z).pack()].NodeId == 0) {
                v.FX = 135+x*16+16;
                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FZ -= 16;
                v.TV = 65535;
                out.push_back(v);

                v.FY += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FY += 16;
                v.FZ -= 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FZ += 16;
                v.TV = 0;
                out.push_back(v);
            }

            if((x-1) < 0 || nodes[Pos::bvec16u(x-1, y, z).pack()].NodeId == 0) {
                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FY += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FZ -= 16;
                v.TV = 65535;
                out.push_back(v);

                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FY += 16;
                v.FZ -= 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FY -= 16;
                v.TU = 0;
                out.push_back(v);
            }

            if((z+1) >= 16 || nodes[Pos::bvec16u(x, y, z+1).pack()].NodeId == 0) {
                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.FZ = 135+z*16+16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FY += 16;
                v.TV = 65535;
                out.push_back(v);

                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.FY += 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FX -= 16;
                v.TU = 0;
                out.push_back(v);
            }

            if((z-1) < 0 || nodes[Pos::bvec16u(x, y, z-1).pack()].NodeId == 0) {
                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.FZ = 135+z*16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FY += 16;
                v.TV = 65535;
                out.push_back(v);

                v.FX += 16;
                v.TU = 65535;
                out.push_back(v);

                v.FX = 135+x*16;
                v.FY = 135+y*16;
                v.TU = 0;
                v.TV = 0;
                out.push_back(v);

                v.FX += 16;
                v.FY += 16;
                v.TV = 65535;
                v.TU = 65535;
                out.push_back(v);

                v.FY -= 16;
                v.TV = 0;
                out.push_back(v);
            }
        }

    return out;
}

void VulkanRenderSession::updateDescriptor_MainAtlas() {
    VkDescriptorBufferInfo bufferInfo = VKCTX->MainTest;
    VkDescriptorImageInfo imageInfo = VKCTX->MainTest;

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
    VkDescriptorBufferInfo bufferInfo = VKCTX->LightDummy;
    VkDescriptorImageInfo imageInfo = VKCTX->LightDummy;

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