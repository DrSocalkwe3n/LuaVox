#include "VulkanRenderSession.hpp"
#include "Client/Vulkan/Vulkan.hpp"
#include "assets.hpp"
#include <glm/ext/matrix_transform.hpp>
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

        assert(!vkCreateDescriptorPool(VkInst->Graphics.Device, &descriptor_pool, nullptr,
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

    if(!MainAtlasDescriptor) {
        VkDescriptorSetAllocateInfo ciAllocInfo =
        {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &MainAtlasDescLayout
        };

        assert(!vkAllocateDescriptorSets(instance->Graphics.Device, &ciAllocInfo, &MainAtlasDescriptor));
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

        assert(!vkCreateDescriptorSetLayout( instance->Graphics.Device, &descriptorLayout, nullptr, &VoxelLightMapDescLayout));
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

        assert(!vkAllocateDescriptorSets(instance->Graphics.Device, &ciAllocInfo, &VoxelLightMapDescriptor));
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

        NodeVertexStatic *array = (NodeVertexStatic*) VKCTX->TestQuad.mapMemory();
        array[0] = {112, 114, 113, 0, 0, 0, 0, 0, 0};
        array[1] = {114, 114, 113, 0, 0, 0, 0, 65535, 0};
        array[2] = {114, 112, 113, 0, 0, 0, 0, 65535, 65535};
        array[3] = {112, 114, 113, 0, 0, 0, 0, 0, 0};
        array[4] = {114, 112, 113, 0, 0, 0, 0, 65535, 65535};
        array[5] = {112, 112, 113, 0, 0, 0, 0, 0, 65535};
        VKCTX->TestQuad.unMapMemory();
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
    glm::mat4 proj = glm::perspective<float>(75, float(VkInst->Screen.Width)/float(VkInst->Screen.Height), 0.5, std::pow(2, 17));
    PCO.ProjView = glm::mat4(Quat);
    //PCO.ProjView *= proj;
    PCO.Model = glm::mat4(1); //= glm::rotate(glm::mat4(1), float(gTime/10), glm::vec3(0, 1, 0));

    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, NodeStaticOpaquePipeline);
	vkCmdPushConstants(drawCmd, MainAtlas_LightMap_PipelineLayout, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(WorldPCO), &PCO);
    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        MainAtlas_LightMap_PipelineLayout,  0, 2, 
        (const VkDescriptorSet[]) {MainAtlasDescriptor, VoxelLightMapDescriptor}, 0, nullptr);

    VkDeviceSize vkOffsets = 0;
    VkBuffer vkBuffer = VKCTX->TestQuad;
    vkCmdBindVertexBuffers(drawCmd, 0, 1, &vkBuffer, &vkOffsets);
    vkCmdDraw(drawCmd, 6, 1, 0, 0);
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