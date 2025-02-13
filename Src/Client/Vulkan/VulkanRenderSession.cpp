#include "VulkanRenderSession.hpp"
#include <vulkan/vulkan_core.h>

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
		if(LightMapDescLayout)
			vkDestroyDescriptorSetLayout(instance->Graphics.Device, LightMapDescLayout, nullptr);
    }

    VoxelOpaquePipeline = VK_NULL_HANDLE;
    VoxelTransparentPipeline = VK_NULL_HANDLE;
    NodeStaticOpaquePipeline = VK_NULL_HANDLE;
    NodeStaticTransparentPipeline = VK_NULL_HANDLE;

    MainAtlas_LightMap_PipelineLayout = VK_NULL_HANDLE;

    MainAtlasDescLayout = VK_NULL_HANDLE;
    LightMapDescLayout = VK_NULL_HANDLE;
}

void VulkanRenderSession::init(Vulkan *instance) {
    if(VkInst != instance) {
        VkInst = instance;
    }

    // Разметка дескрипторов
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
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
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

        assert(!vkCreateDescriptorSetLayout(
            instance->Graphics.Device, &descriptorLayout, nullptr, &MainAtlasDescLayout));
    }

    if(!LightMapDescLayout) {
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
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
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

        assert(!vkCreateDescriptorSetLayout( instance->Graphics.Device, &descriptorLayout, nullptr, &LightMapDescLayout));
    }
        

    std::vector<VkPushConstantRange> worldWideShaderPushConstants =
    {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
            .offset = 0,
            .size = uint32_t(sizeof(WorldPCO))
        }
    };

    // Разметка графических конвейеров
    if(!MainAtlas_LightMap_PipelineLayout) {
        std::vector<VkDescriptorSetLayout> layouts =
        {
            MainAtlasDescLayout,
            LightMapDescLayout
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
        
		assert(!vkCreatePipelineLayout(instance->Graphics.Device, &pPipelineLayoutCreateInfo, nullptr, &MainAtlas_LightMap_PipelineLayout));
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
            VoxelShaderVertex = VkInst->createShaderFromFile("assets/shaders/chunk/voxel.vert.bin");

        if(!VoxelShaderGeometry)
            VoxelShaderGeometry = VkInst->createShaderFromFile("assets/shaders/chunk/voxel.geom.bin");

        if(!VoxelShaderFragmentOpaque)
            VoxelShaderFragmentOpaque = VkInst->createShaderFromFile("assets/shaders/chunk/voxel_opaque.frag.bin");

        if(!VoxelShaderFragmentTransparent)
            VoxelShaderFragmentTransparent = VkInst->createShaderFromFile("assets/shaders/chunk/voxel_transparent.frag.bin");

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
            assert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelOpaquePipeline));

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

           assert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &VoxelTransparentPipeline));
        }

        // Для статичных непрозрачных и полупрозрачных нод
        if(!NodeShaderVertex)
            NodeShaderVertex = VkInst->createShaderFromFile("assets/shaders/chunk/node.vert.bin");

        if(!NodeShaderGeometry)
            NodeShaderGeometry = VkInst->createShaderFromFile("assets/shaders/chunk/node.geom.bin");

        if(!NodeShaderFragmentOpaque)
            NodeShaderFragmentOpaque = VkInst->createShaderFromFile("assets/shaders/chunk/node_opaque.frag.bin");

        if(!NodeShaderFragmentTransparent)
            NodeShaderFragmentTransparent = VkInst->createShaderFromFile("assets/shaders/chunk/node_transparent.frag.bin");

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
            assert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE,
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

            assert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 
                1, &pipeline, nullptr, &NodeStaticTransparentPipeline));
        }
    }
}

void VulkanRenderSession::onDefTexture(TextureId_c id, std::vector<std::byte> &&info) {

}

void VulkanRenderSession::onDefTextureLost(const std::vector<TextureId_c> &&lost) {

}

void VulkanRenderSession::onDefModel(ModelId_c id, std::vector<std::byte> &&info) {

}

void VulkanRenderSession::onDefModelLost(const std::vector<ModelId_c> &&lost) {

}

void VulkanRenderSession::onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) {

}

void VulkanRenderSession::onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) {

}

void VulkanRenderSession::onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) {

}

void VulkanRenderSession::onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) {

}

void VulkanRenderSession::onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) {

}

void VulkanRenderSession::onChunksChange(WorldId_c worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) {

}

void VulkanRenderSession::setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) {
    WorldId = worldId;
    Pos = pos;
    Quat = quat;
}

void VulkanRenderSession::beforeDraw() {

}

void VulkanRenderSession::drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd) {

}

}