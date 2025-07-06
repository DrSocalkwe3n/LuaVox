#include "VulkanRenderSession.hpp"
#include "Client/Abstract.hpp"
#include "Client/Vulkan/Vulkan.hpp"
#include "Common/Abstract.hpp"
#include "assets.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include <cstddef>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>

namespace LV::Client::VK {


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
          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 3}
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

        int width, height;
        bool hasAlpha;
        for(const char *path : {
                "grass.png", 
                "tropical_rainforest_wood.png", 
                "willow_wood.png", 
                "xnether_blue_wood.png",
                "xnether_purple_wood.png"
        }) {
            ByteBuffer image = VK::loadPNG(getResource(std::string("textures/") + path)->makeStream().Stream, width, height, hasAlpha);
            uint16_t texId = VKCTX->MainTest.atlasAddTexture(width, height);
            VKCTX->MainTest.atlasChangeTextureData(texId, (const uint32_t*) image.data());
        }

        /*
        x left -1 ~ right 1
        y up -1 ~ down 1
        z far 1 ~ near 0

        glm

        */

        {
            NodeVertexStatic *array = (NodeVertexStatic*) VKCTX->TestQuad.mapMemory();
            array[0] = {112, 114, 50, 0, 0, 0, 0, 0, 0};
            array[1] = {114, 114, 50, 0, 0, 0, 0, 65535, 0};
            array[2] = {114, 112, 50, 0, 0, 0, 0, 65535, 65535};
            array[3] = {112, 114, 50, 0, 0, 0, 0, 0, 0};
            array[4] = {114, 112, 50, 0, 0, 0, 0, 65535, 65535};
            array[5] = {112, 112, 50, 0, 0, 0, 0, 0, 65535};
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

void VulkanRenderSession::onBinaryResourceAdd(std::unordered_map<EnumBinResource, std::unordered_map<ResourceId_t, BinaryResource>>) {

}

void VulkanRenderSession::onBinaryResourceLost(std::unordered_map<EnumBinResource, std::vector<ResourceId_t>>) {

}

void VulkanRenderSession::onContentDefinesAdd(std::unordered_map<EnumDefContent, std::unordered_map<ResourceId_t, std::u8string>>) {

}

void VulkanRenderSession::onContentDefinesLost(std::unordered_map<EnumDefContent, std::vector<ResourceId_t>>) {

}

void VulkanRenderSession::onChunksChange(WorldId_t worldId, const std::unordered_set<Pos::GlobalChunk>& changeOrAddList, const std::unordered_set<Pos::GlobalRegion>& remove) {
auto &table = External.ChunkVoxelMesh[worldId];

    for(Pos::GlobalChunk pos : changeOrAddList) {
        Pos::GlobalRegion rPos = pos >> 4;
        Pos::bvec16u cPos = pos & 0xf;

        const auto &voxels = ServerSession->Data.Worlds[worldId].Regions[rPos].Chunks[cPos.x][cPos.y][cPos.z].Voxels;

        if(voxels.empty()) {
            auto iter = table.find(pos);
            if(iter != table.end())
                table.erase(iter);
        } else {
            std::vector<VoxelVertexPoint> vertexs = generateMeshForVoxelChunks(voxels);

            if(!vertexs.empty()) {
                auto &buffer = table[pos] = std::make_unique<Buffer>(VkInst, vertexs.size()*sizeof(VoxelVertexPoint));
                std::copy(vertexs.data(), vertexs.data()+vertexs.size(), (VoxelVertexPoint*) buffer->mapMemory());
                buffer->unMapMemory();
            } else {
                auto iter = table.find(pos);
                if(iter != table.end())
                    table.erase(iter);
            }
        }
    }

    for(Pos::GlobalRegion pos : remove) {
        for(int z = 0; z < 4; z++)
        for(int y = 0; y < 4; y++)
        for(int x = 0; x < 4; x++) {
            auto iter = table.find((Pos::GlobalChunk(pos) << 2) + Pos::GlobalChunk(x, y, z));
            if(iter != table.end())
                table.erase(iter);
        }
    }

    if(table.empty())
        External.ChunkVoxelMesh.erase( External.ChunkVoxelMesh.find(worldId));
}

void VulkanRenderSession::setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) {
    WorldId = worldId;
    Pos = pos;
    Quat = quat;
}

void VulkanRenderSession::beforeDraw() {
    if(VKCTX) {
        VKCTX->MainTest.atlasUpdateDynamicData();
        VKCTX->LightDummy.atlasUpdateDynamicData();
    }
}

void VulkanRenderSession::drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd) {
    glm::mat4 proj = glm::perspective<float>(glm::radians(75.f), float(VkInst->Screen.Width)/float(VkInst->Screen.Height), 0.5, std::pow(2, 17));
    
    for(int i = 0; i < 4; i++) {
        proj[1][i] *= -1;
        proj[2][i] *= -1;
    }
    
    
    // Сместить в координаты игрока, повернуть относительно взгляда проецировать на экран
    // Изначально взгляд в z-1
    PCO.ProjView = glm::mat4(1);
    PCO.ProjView = glm::translate(PCO.ProjView, -glm::vec3(Pos)/float(Pos::Object_t::BS));
    PCO.ProjView = proj*glm::mat4(Quat)*PCO.ProjView;
    PCO.Model = glm::mat4(1);

    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, NodeStaticOpaquePipeline);
	vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        MainAtlas_LightMap_PipelineLayout,  0, 2, 
        (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    VkDeviceSize vkOffsets = 0;
    VkBuffer vkBuffer = VKCTX->TestQuad;
    vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);

    for(int i = 0; i < 16; i++) {
        PCO.Model = glm::rotate(PCO.Model, glm::half_pi<float>()/4, glm::vec3(0, 1, 0));
        vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
        vkCmdDraw(drawCmd, 6, 1, 0, 0);
    }

    PCO.Model = glm::mat4(1);

    // Проба рендера вокселей
    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VoxelOpaquePipeline);
	vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        MainAtlas_LightMap_PipelineLayout,  0, 2, 
        (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    if(VKCTX->TestVoxel) {
        vkBuffer = *VKCTX->TestVoxel;
        vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
        vkCmdDraw(drawCmd, VKCTX->TestVoxel->getSize() / sizeof(VoxelVertexPoint), 1, 0, 0);
    }

    {
        auto iterWorld = External.ChunkVoxelMesh.find(WorldId);
        if(iterWorld != External.ChunkVoxelMesh.end()) {
            glm::mat4 orig = PCO.Model;

            for(auto &pair : iterWorld->second) {
                glm::vec3 cpos(pair.first.x, pair.first.y, pair.first.z);
                PCO.Model = glm::translate(orig, cpos*16.f);
                vkBuffer = *pair.second;

                vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, offsetof(WorldPCO, Model), sizeof(WorldPCO::Model), &PCO.Model);
                vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
                vkCmdDraw(drawCmd, pair.second->getSize() / sizeof(VoxelVertexPoint), 1, 0, 0);
            }

            PCO.Model = orig;
        }
    }
}

std::vector<VoxelVertexPoint> VulkanRenderSession::generateMeshForVoxelChunks(const std::vector<VoxelCube> cubes) {
    std::vector<VoxelVertexPoint> out;
    out.reserve(cubes.size()*6);
    
    for(const VoxelCube &cube : cubes) {
        out.emplace_back(
            cube.Left.x,
            cube.Left.y,
            cube.Left.z,
            0,
            0, 0,
            cube.Size.x,
            cube.Size.z,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Left.x,
            cube.Left.y,
            cube.Left.z,
            1,
            0, 0,
            cube.Size.x,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Left.x,
            cube.Left.y,
            cube.Left.z,
            2,
            0, 0,
            cube.Size.z,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Left.x,
            cube.Left.y+cube.Size.y+1,
            cube.Left.z,
            3,
            0, 0,
            cube.Size.x,
            cube.Size.z,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Left.x,
            cube.Left.y,
            cube.Left.z+cube.Size.z+1,
            4,
            0, 0,
            cube.Size.x,
            cube.Size.y,
            cube.VoxelId,
            0, 0,
            0
        );

        out.emplace_back(
            cube.Left.x+cube.Size.x+1,
            cube.Left.y,
            cube.Left.z,
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