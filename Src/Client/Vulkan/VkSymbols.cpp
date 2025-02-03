#include <stdexcept>
#define VK_NO_PROTOTYPES
#define TOS_VULKAN_NO_VIDEO
#include <vulkan/vulkan.h>

void TOS_nullFunc() { throw std::runtime_error("Символ не привязан"); }

void* TOS_LINK_vkCreateInstance = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyInstance = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumeratePhysicalDevices = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFeatures = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFormatProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceImageFormatProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceMemoryProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetInstanceProcAddr = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceProcAddr = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDevice = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDevice = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumerateInstanceExtensionProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumerateDeviceExtensionProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumerateInstanceLayerProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumerateDeviceLayerProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceQueue = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueSubmit = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueWaitIdle = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDeviceWaitIdle = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAllocateMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkFreeMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkMapMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUnmapMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkFlushMappedMemoryRanges = (void*) &TOS_nullFunc;
void* TOS_LINK_vkInvalidateMappedMemoryRanges = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceMemoryCommitment = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindBufferMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindImageMemory = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageSparseMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueBindSparse = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateFence = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyFence = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetFences = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetFenceStatus = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWaitForFences = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSemaphore = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySemaphore = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetEventStatus = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateQueryPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyQueryPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetQueryPoolResults = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateBufferView = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyBufferView = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageSubresourceLayout = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateImageView = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyImageView = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateShaderModule = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyShaderModule = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreatePipelineCache = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyPipelineCache = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPipelineCacheData = (void*) &TOS_nullFunc;
void* TOS_LINK_vkMergePipelineCaches = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateGraphicsPipelines = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateComputePipelines = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyPipeline = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreatePipelineLayout = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyPipelineLayout = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSampler = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySampler = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDescriptorSetLayout = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDescriptorSetLayout = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDescriptorPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDescriptorPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetDescriptorPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAllocateDescriptorSets = (void*) &TOS_nullFunc;
void* TOS_LINK_vkFreeDescriptorSets = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUpdateDescriptorSets = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateFramebuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyFramebuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateRenderPass = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyRenderPass = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRenderAreaGranularity = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateCommandPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyCommandPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetCommandPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAllocateCommandBuffers = (void*) &TOS_nullFunc;
void* TOS_LINK_vkFreeCommandBuffers = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBeginCommandBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEndCommandBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetCommandBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindPipeline = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewport = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetScissor = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLineWidth = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBias = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBounds = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilCompareMask = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilWriteMask = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilReference = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindDescriptorSets = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindIndexBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindVertexBuffers = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDraw = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndexed = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndirect = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndexedIndirect = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDispatch = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDispatchIndirect = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBlitImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBufferToImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImageToBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdUpdateBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdFillBuffer = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdClearColorImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdClearDepthStencilImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdClearAttachments = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResolveImage = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResetEvent = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWaitEvents = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPipelineBarrier = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginQuery = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndQuery = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResetQueryPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteTimestamp = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyQueryPoolResults = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPushConstants = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginRenderPass = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdNextSubpass = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndRenderPass = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdExecuteCommands = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumerateInstanceVersion = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindBufferMemory2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindImageMemory2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceGroupPeerMemoryFeatures = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDeviceMask = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDispatchBase = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumeratePhysicalDeviceGroups = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageMemoryRequirements2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferMemoryRequirements2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageSparseMemoryRequirements2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFeatures2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFormatProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceMemoryProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkTrimCommandPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceQueue2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSamplerYcbcrConversion = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySamplerYcbcrConversion = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDescriptorUpdateTemplate = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDescriptorUpdateTemplate = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUpdateDescriptorSetWithTemplate = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalBufferProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalFenceProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalSemaphoreProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetLayoutSupport = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndirectCount = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndexedIndirectCount = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateRenderPass2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginRenderPass2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdNextSubpass2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndRenderPass2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetQueryPool = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSemaphoreCounterValue = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWaitSemaphores = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSignalSemaphore = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferDeviceAddress = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceToolProperties = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreatePrivateDataSlot = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyPrivateDataSlot = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetPrivateData = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPrivateData = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetEvent2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResetEvent2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWaitEvents2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPipelineBarrier2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteTimestamp2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueSubmit2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBuffer2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImage2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBufferToImage2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImageToBuffer2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBlitImage2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResolveImage2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginRendering = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndRendering = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCullMode = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetFrontFace = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPrimitiveTopology = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportWithCount = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetScissorWithCount = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindVertexBuffers2 = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthTestEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthWriteEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthCompareOp = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBoundsTestEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilTestEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilOp = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRasterizerDiscardEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBiasEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPrimitiveRestartEnable = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceBufferMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceImageMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceImageSparseMemoryRequirements = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySurfaceKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceSupportKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceFormatsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfacePresentModesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSwapchainKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySwapchainKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSwapchainImagesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAcquireNextImageKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueuePresentKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceGroupPresentCapabilitiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceGroupSurfacePresentModesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDevicePresentRectanglesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAcquireNextImage2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceDisplayPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceDisplayPlanePropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDisplayPlaneSupportedDisplaysKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDisplayModePropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDisplayModeKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDisplayPlaneCapabilitiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDisplayPlaneSurfaceKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSharedSwapchainsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceVideoCapabilitiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceVideoFormatPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateVideoSessionKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyVideoSessionKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetVideoSessionMemoryRequirementsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindVideoSessionMemoryKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateVideoSessionParametersKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUpdateVideoSessionParametersKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyVideoSessionParametersKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginVideoCodingKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndVideoCodingKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdControlVideoCodingKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDecodeVideoKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginRenderingKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndRenderingKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFeatures2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFormatProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceMemoryProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceGroupPeerMemoryFeaturesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDeviceMaskKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDispatchBaseKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkTrimCommandPoolKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumeratePhysicalDeviceGroupsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalBufferPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetMemoryFdKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetMemoryFdPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkImportSemaphoreFdKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSemaphoreFdKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPushDescriptorSetKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPushDescriptorSetWithTemplateKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDescriptorUpdateTemplateKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDescriptorUpdateTemplateKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUpdateDescriptorSetWithTemplateKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateRenderPass2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginRenderPass2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdNextSubpass2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndRenderPass2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSwapchainStatusKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalFencePropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkImportFenceFdKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetFenceFdKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAcquireProfilingLockKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkReleaseProfilingLockKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceFormats2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceDisplayProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceDisplayPlaneProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDisplayModeProperties2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDisplayPlaneCapabilities2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageMemoryRequirements2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferMemoryRequirements2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageSparseMemoryRequirements2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateSamplerYcbcrConversionKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroySamplerYcbcrConversionKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindBufferMemory2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindImageMemory2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetLayoutSupportKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndirectCountKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndexedIndirectCountKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSemaphoreCounterValueKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWaitSemaphoresKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSignalSemaphoreKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceFragmentShadingRatesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWaitForPresentKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferDeviceAddressKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDeferredOperationKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDeferredOperationKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeferredOperationResultKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDeferredOperationJoinKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPipelineExecutablePropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPipelineExecutableStatisticsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPipelineExecutableInternalRepresentationsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetEvent2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResetEvent2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWaitEvents2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPipelineBarrier2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteTimestamp2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueSubmit2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteBufferMarker2AMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetQueueCheckpointData2NV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBuffer2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImage2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyBufferToImage2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyImageToBuffer2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBlitImage2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdResolveImage2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdTraceRaysIndirect2KHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceBufferMemoryRequirementsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceImageMemoryRequirementsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceImageSparseMemoryRequirementsKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDebugReportCallbackEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDebugReportCallbackEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDebugReportMessageEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDebugMarkerSetObjectTagEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDebugMarkerSetObjectNameEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDebugMarkerBeginEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDebugMarkerEndEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDebugMarkerInsertEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindTransformFeedbackBuffersEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginTransformFeedbackEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndTransformFeedbackEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginQueryIndexedEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndQueryIndexedEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndirectByteCountEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateCuModuleNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateCuFunctionNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyCuModuleNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyCuFunctionNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCuLaunchKernelNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageViewAddressNVX = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndirectCountAMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawIndexedIndirectCountAMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetShaderInfoAMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceExternalImageFormatPropertiesNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginConditionalRenderingEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndConditionalRenderingEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportWScalingNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkReleaseDisplayEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2EXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDisplayPowerControlEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkRegisterDeviceEventEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkRegisterDisplayEventEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSwapchainCounterEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRefreshCycleDurationGOOGLE = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPastPresentationTimingGOOGLE = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDiscardRectangleEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDiscardRectangleEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDiscardRectangleModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetHdrMetadataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetDebugUtilsObjectNameEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetDebugUtilsObjectTagEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueBeginDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueEndDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueInsertDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBeginDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdEndDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdInsertDebugUtilsLabelEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateDebugUtilsMessengerEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyDebugUtilsMessengerEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSubmitDebugUtilsMessageEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetSampleLocationsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceMultisamplePropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageDrmFormatModifierPropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateValidationCacheEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyValidationCacheEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkMergeValidationCachesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetValidationCacheDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindShadingRateImageNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportShadingRatePaletteNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoarseSampleOrderNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateAccelerationStructureNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyAccelerationStructureNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetAccelerationStructureMemoryRequirementsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindAccelerationStructureMemoryNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBuildAccelerationStructureNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyAccelerationStructureNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdTraceRaysNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateRayTracingPipelinesNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRayTracingShaderGroupHandlesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRayTracingShaderGroupHandlesNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetAccelerationStructureHandleNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCompileDeferredNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetMemoryHostPointerPropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteBufferMarkerAMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetCalibratedTimestampsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksIndirectNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksIndirectCountNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetExclusiveScissorEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetExclusiveScissorNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCheckpointNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetQueueCheckpointDataNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkInitializePerformanceApiINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkUninitializePerformanceApiINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPerformanceMarkerINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPerformanceStreamMarkerINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPerformanceOverrideINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAcquirePerformanceConfigurationINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkReleasePerformanceConfigurationINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkQueueSetPerformanceConfigurationINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPerformanceParameterINTEL = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetLocalDimmingAMD = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferDeviceAddressEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceToolPropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateHeadlessSurfaceEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLineStippleEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkResetQueryPoolEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCullModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetFrontFaceEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPrimitiveTopologyEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportWithCountEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetScissorWithCountEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindVertexBuffers2EXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthTestEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthWriteEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthCompareOpEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBoundsTestEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilTestEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetStencilOpEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkReleaseSwapchainImagesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetGeneratedCommandsMemoryRequirementsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdPreprocessGeneratedCommandsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdExecuteGeneratedCommandsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindPipelineShaderGroupNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateIndirectCommandsLayoutNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyIndirectCommandsLayoutNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkAcquireDrmDisplayEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDrmDisplayEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreatePrivateDataSlotEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyPrivateDataSlotEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetPrivateDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPrivateDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetLayoutSizeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetLayoutBindingOffsetEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindDescriptorBuffersEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDescriptorBufferOffsetsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindDescriptorBufferEmbeddedSamplersEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetBufferOpaqueCaptureDescriptorDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageOpaqueCaptureDescriptorDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageViewOpaqueCaptureDescriptorDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetSamplerOpaqueCaptureDescriptorDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetImageSubresourceLayout2EXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceFaultInfoEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetVertexInputEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSubpassShadingHUAWEI = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBindInvocationMaskHUAWEI = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetMemoryRemoteAddressNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPipelinePropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPatchControlPointsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRasterizerDiscardEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthBiasEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLogicOpEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPrimitiveRestartEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMultiEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMultiIndexedEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBuildMicromapsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBuildMicromapsEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyMicromapToMemoryEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyMemoryToMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWriteMicromapsPropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMicromapToMemoryEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMemoryToMicromapEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteMicromapsPropertiesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceMicromapCompatibilityEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetMicromapBuildSizesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawClusterHUAWEI = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawClusterIndirectHUAWEI = (void*) &TOS_nullFunc;
void* TOS_LINK_vkSetDeviceMemoryPriorityEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetLayoutHostMappingInfoVALVE = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDescriptorSetHostMappingVALVE = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMemoryIndirectNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMemoryToImageIndirectNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDecompressMemoryNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDecompressMemoryIndirectCountNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetTessellationDomainOriginEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthClampEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetPolygonModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRasterizationSamplesEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetSampleMaskEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetAlphaToCoverageEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetAlphaToOneEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLogicOpEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetColorBlendEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetColorBlendEquationEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetColorWriteMaskEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRasterizationStreamEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetConservativeRasterizationModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetExtraPrimitiveOverestimationSizeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthClipEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetSampleLocationsEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetColorBlendAdvancedEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetProvokingVertexModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLineRasterizationModeEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetLineStippleEnableEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetDepthClipNegativeOneToOneEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportWScalingEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetViewportSwizzleNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageToColorEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageToColorLocationNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageModulationModeNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageModulationTableEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageModulationTableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetShadingRateImageEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRepresentativeFragmentTestEnableNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetCoverageReductionModeNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetShaderModuleIdentifierEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetShaderModuleCreateInfoIdentifierEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetPhysicalDeviceOpticalFlowImageFormatsNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateOpticalFlowSessionNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyOpticalFlowSessionNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBindOpticalFlowSessionImageNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdOpticalFlowExecuteNV = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetFramebufferTilePropertiesQCOM = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDynamicRenderingTilePropertiesQCOM = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkDestroyAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBuildAccelerationStructuresKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdBuildAccelerationStructuresIndirectKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkBuildAccelerationStructuresKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyAccelerationStructureToMemoryKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCopyMemoryToAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkWriteAccelerationStructuresPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyAccelerationStructureToMemoryKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdCopyMemoryToAccelerationStructureKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetAccelerationStructureDeviceAddressKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetDeviceAccelerationStructureCompatibilityKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetAccelerationStructureBuildSizesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdTraceRaysKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCreateRayTracingPipelinesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdTraceRaysIndirectKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkGetRayTracingShaderGroupStackSizeKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdSetRayTracingPipelineStackSizeKHR = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksIndirectEXT = (void*) &TOS_nullFunc;
void* TOS_LINK_vkCmdDrawMeshTasksIndirectCountEXT = (void*) &TOS_nullFunc;
extern "C" {
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) { return ((VkResult (*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*)) TOS_LINK_vkCreateInstance)(pCreateInfo, pAllocator, pInstance); }
void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkInstance, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyInstance)(instance, pAllocator); }
VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) { return ((VkResult (*)(VkInstance, uint32_t*, VkPhysicalDevice*)) TOS_LINK_vkEnumeratePhysicalDevices)(instance, pPhysicalDeviceCount, pPhysicalDevices); }
void vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures*)) TOS_LINK_vkGetPhysicalDeviceFeatures)(physicalDevice, pFeatures); }
void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) { ((void (*)(VkPhysicalDevice, VkFormat, VkFormatProperties*)) TOS_LINK_vkGetPhysicalDeviceFormatProperties)(physicalDevice, format, pFormatProperties); }
VkResult vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties*)) TOS_LINK_vkGetPhysicalDeviceImageFormatProperties)(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties); }
void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties*)) TOS_LINK_vkGetPhysicalDeviceProperties)(physicalDevice, pProperties); }
void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) { ((void (*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*)) TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties); }
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*)) TOS_LINK_vkGetPhysicalDeviceMemoryProperties)(physicalDevice, pMemoryProperties); }
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) { return ((PFN_vkVoidFunction (*)(VkInstance, const char*)) TOS_LINK_vkGetInstanceProcAddr)(instance, pName); }
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char* pName) { return ((PFN_vkVoidFunction (*)(VkDevice, const char*)) TOS_LINK_vkGetDeviceProcAddr)(device, pName); }
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) { return ((VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*)) TOS_LINK_vkCreateDevice)(physicalDevice, pCreateInfo, pAllocator, pDevice); }
void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDevice)(device, pAllocator); }
VkResult vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) { return ((VkResult (*)(const char*, uint32_t*, VkExtensionProperties*)) TOS_LINK_vkEnumerateInstanceExtensionProperties)(pLayerName, pPropertyCount, pProperties); }
VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) { return ((VkResult (*)(VkPhysicalDevice, const char*, uint32_t*, VkExtensionProperties*)) TOS_LINK_vkEnumerateDeviceExtensionProperties)(physicalDevice, pLayerName, pPropertyCount, pProperties); }
VkResult vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) { return ((VkResult (*)(uint32_t*, VkLayerProperties*)) TOS_LINK_vkEnumerateInstanceLayerProperties)(pPropertyCount, pProperties); }
VkResult vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkLayerProperties*)) TOS_LINK_vkEnumerateDeviceLayerProperties)(physicalDevice, pPropertyCount, pProperties); }
void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) { ((void (*)(VkDevice, uint32_t, uint32_t, VkQueue*)) TOS_LINK_vkGetDeviceQueue)(device, queueFamilyIndex, queueIndex, pQueue); }
VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) { return ((VkResult (*)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence)) TOS_LINK_vkQueueSubmit)(queue, submitCount, pSubmits, fence); }
VkResult vkQueueWaitIdle(VkQueue queue) { return ((VkResult (*)(VkQueue)) TOS_LINK_vkQueueWaitIdle)(queue); }
VkResult vkDeviceWaitIdle(VkDevice device) { return ((VkResult (*)(VkDevice)) TOS_LINK_vkDeviceWaitIdle)(device); }
VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) { return ((VkResult (*)(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*)) TOS_LINK_vkAllocateMemory)(device, pAllocateInfo, pAllocator, pMemory); }
void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*)) TOS_LINK_vkFreeMemory)(device, memory, pAllocator); }
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) { return ((VkResult (*)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**)) TOS_LINK_vkMapMemory)(device, memory, offset, size, flags, ppData); }
void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) { ((void (*)(VkDevice, VkDeviceMemory)) TOS_LINK_vkUnmapMemory)(device, memory); }
VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) { return ((VkResult (*)(VkDevice, uint32_t, const VkMappedMemoryRange*)) TOS_LINK_vkFlushMappedMemoryRanges)(device, memoryRangeCount, pMemoryRanges); }
VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) { return ((VkResult (*)(VkDevice, uint32_t, const VkMappedMemoryRange*)) TOS_LINK_vkInvalidateMappedMemoryRanges)(device, memoryRangeCount, pMemoryRanges); }
void vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) { ((void (*)(VkDevice, VkDeviceMemory, VkDeviceSize*)) TOS_LINK_vkGetDeviceMemoryCommitment)(device, memory, pCommittedMemoryInBytes); }
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return ((VkResult (*)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize)) TOS_LINK_vkBindBufferMemory)(device, buffer, memory, memoryOffset); }
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return ((VkResult (*)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize)) TOS_LINK_vkBindImageMemory)(device, image, memory, memoryOffset); }
void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) { ((void (*)(VkDevice, VkBuffer, VkMemoryRequirements*)) TOS_LINK_vkGetBufferMemoryRequirements)(device, buffer, pMemoryRequirements); }
void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) { ((void (*)(VkDevice, VkImage, VkMemoryRequirements*)) TOS_LINK_vkGetImageMemoryRequirements)(device, image, pMemoryRequirements); }
void vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) { ((void (*)(VkDevice, VkImage, uint32_t*, VkSparseImageMemoryRequirements*)) TOS_LINK_vkGetImageSparseMemoryRequirements)(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements); }
void vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) { ((void (*)(VkPhysicalDevice, VkFormat, VkImageType, VkSampleCountFlagBits, VkImageUsageFlags, VkImageTiling, uint32_t*, VkSparseImageFormatProperties*)) TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties)(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties); }
VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) { return ((VkResult (*)(VkQueue, uint32_t, const VkBindSparseInfo*, VkFence)) TOS_LINK_vkQueueBindSparse)(queue, bindInfoCount, pBindInfo, fence); }
VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) { return ((VkResult (*)(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence*)) TOS_LINK_vkCreateFence)(device, pCreateInfo, pAllocator, pFence); }
void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkFence, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyFence)(device, fence, pAllocator); }
VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) { return ((VkResult (*)(VkDevice, uint32_t, const VkFence*)) TOS_LINK_vkResetFences)(device, fenceCount, pFences); }
VkResult vkGetFenceStatus(VkDevice device, VkFence fence) { return ((VkResult (*)(VkDevice, VkFence)) TOS_LINK_vkGetFenceStatus)(device, fence); }
VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) { return ((VkResult (*)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t)) TOS_LINK_vkWaitForFences)(device, fenceCount, pFences, waitAll, timeout); }
VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) { return ((VkResult (*)(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore*)) TOS_LINK_vkCreateSemaphore)(device, pCreateInfo, pAllocator, pSemaphore); }
void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkSemaphore, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySemaphore)(device, semaphore, pAllocator); }
VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) { return ((VkResult (*)(VkDevice, const VkEventCreateInfo*, const VkAllocationCallbacks*, VkEvent*)) TOS_LINK_vkCreateEvent)(device, pCreateInfo, pAllocator, pEvent); }
void vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkEvent, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyEvent)(device, event, pAllocator); }
VkResult vkGetEventStatus(VkDevice device, VkEvent event) { return ((VkResult (*)(VkDevice, VkEvent)) TOS_LINK_vkGetEventStatus)(device, event); }
VkResult vkSetEvent(VkDevice device, VkEvent event) { return ((VkResult (*)(VkDevice, VkEvent)) TOS_LINK_vkSetEvent)(device, event); }
VkResult vkResetEvent(VkDevice device, VkEvent event) { return ((VkResult (*)(VkDevice, VkEvent)) TOS_LINK_vkResetEvent)(device, event); }
VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) { return ((VkResult (*)(VkDevice, const VkQueryPoolCreateInfo*, const VkAllocationCallbacks*, VkQueryPool*)) TOS_LINK_vkCreateQueryPool)(device, pCreateInfo, pAllocator, pQueryPool); }
void vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkQueryPool, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyQueryPool)(device, queryPool, pAllocator); }
VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) { return ((VkResult (*)(VkDevice, VkQueryPool, uint32_t, uint32_t, size_t, void*, VkDeviceSize, VkQueryResultFlags)) TOS_LINK_vkGetQueryPoolResults)(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags); }
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) { return ((VkResult (*)(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*)) TOS_LINK_vkCreateBuffer)(device, pCreateInfo, pAllocator, pBuffer); }
void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkBuffer, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyBuffer)(device, buffer, pAllocator); }
VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) { return ((VkResult (*)(VkDevice, const VkBufferViewCreateInfo*, const VkAllocationCallbacks*, VkBufferView*)) TOS_LINK_vkCreateBufferView)(device, pCreateInfo, pAllocator, pView); }
void vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkBufferView, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyBufferView)(device, bufferView, pAllocator); }
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) { return ((VkResult (*)(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*)) TOS_LINK_vkCreateImage)(device, pCreateInfo, pAllocator, pImage); }
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkImage, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyImage)(device, image, pAllocator); }
void vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) { ((void (*)(VkDevice, VkImage, const VkImageSubresource*, VkSubresourceLayout*)) TOS_LINK_vkGetImageSubresourceLayout)(device, image, pSubresource, pLayout); }
VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) { return ((VkResult (*)(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*)) TOS_LINK_vkCreateImageView)(device, pCreateInfo, pAllocator, pView); }
void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkImageView, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyImageView)(device, imageView, pAllocator); }
VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) { return ((VkResult (*)(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*)) TOS_LINK_vkCreateShaderModule)(device, pCreateInfo, pAllocator, pShaderModule); }
void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkShaderModule, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyShaderModule)(device, shaderModule, pAllocator); }
VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) { return ((VkResult (*)(VkDevice, const VkPipelineCacheCreateInfo*, const VkAllocationCallbacks*, VkPipelineCache*)) TOS_LINK_vkCreatePipelineCache)(device, pCreateInfo, pAllocator, pPipelineCache); }
void vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkPipelineCache, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyPipelineCache)(device, pipelineCache, pAllocator); }
VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) { return ((VkResult (*)(VkDevice, VkPipelineCache, size_t*, void*)) TOS_LINK_vkGetPipelineCacheData)(device, pipelineCache, pDataSize, pData); }
VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) { return ((VkResult (*)(VkDevice, VkPipelineCache, uint32_t, const VkPipelineCache*)) TOS_LINK_vkMergePipelineCaches)(device, dstCache, srcCacheCount, pSrcCaches); }
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) { return ((VkResult (*)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*)) TOS_LINK_vkCreateGraphicsPipelines)(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); }
VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) { return ((VkResult (*)(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*)) TOS_LINK_vkCreateComputePipelines)(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); }
void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkPipeline, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyPipeline)(device, pipeline, pAllocator); }
VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) { return ((VkResult (*)(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*)) TOS_LINK_vkCreatePipelineLayout)(device, pCreateInfo, pAllocator, pPipelineLayout); }
void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyPipelineLayout)(device, pipelineLayout, pAllocator); }
VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) { return ((VkResult (*)(VkDevice, const VkSamplerCreateInfo*, const VkAllocationCallbacks*, VkSampler*)) TOS_LINK_vkCreateSampler)(device, pCreateInfo, pAllocator, pSampler); }
void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkSampler, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySampler)(device, sampler, pAllocator); }
VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) { return ((VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*)) TOS_LINK_vkCreateDescriptorSetLayout)(device, pCreateInfo, pAllocator, pSetLayout); }
void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDescriptorSetLayout)(device, descriptorSetLayout, pAllocator); }
VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) { return ((VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*)) TOS_LINK_vkCreateDescriptorPool)(device, pCreateInfo, pAllocator, pDescriptorPool); }
void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDescriptorPool)(device, descriptorPool, pAllocator); }
VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) { return ((VkResult (*)(VkDevice, VkDescriptorPool, VkDescriptorPoolResetFlags)) TOS_LINK_vkResetDescriptorPool)(device, descriptorPool, flags); }
VkResult vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) { return ((VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*)) TOS_LINK_vkAllocateDescriptorSets)(device, pAllocateInfo, pDescriptorSets); }
VkResult vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) { return ((VkResult (*)(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*)) TOS_LINK_vkFreeDescriptorSets)(device, descriptorPool, descriptorSetCount, pDescriptorSets); }
void vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) { ((void (*)(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*)) TOS_LINK_vkUpdateDescriptorSets)(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies); }
VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) { return ((VkResult (*)(VkDevice, const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer*)) TOS_LINK_vkCreateFramebuffer)(device, pCreateInfo, pAllocator, pFramebuffer); }
void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkFramebuffer, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyFramebuffer)(device, framebuffer, pAllocator); }
VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) { return ((VkResult (*)(VkDevice, const VkRenderPassCreateInfo*, const VkAllocationCallbacks*, VkRenderPass*)) TOS_LINK_vkCreateRenderPass)(device, pCreateInfo, pAllocator, pRenderPass); }
void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkRenderPass, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyRenderPass)(device, renderPass, pAllocator); }
void vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) { ((void (*)(VkDevice, VkRenderPass, VkExtent2D*)) TOS_LINK_vkGetRenderAreaGranularity)(device, renderPass, pGranularity); }
VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) { return ((VkResult (*)(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*)) TOS_LINK_vkCreateCommandPool)(device, pCreateInfo, pAllocator, pCommandPool); }
void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkCommandPool, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyCommandPool)(device, commandPool, pAllocator); }
VkResult vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) { return ((VkResult (*)(VkDevice, VkCommandPool, VkCommandPoolResetFlags)) TOS_LINK_vkResetCommandPool)(device, commandPool, flags); }
VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) { return ((VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*)) TOS_LINK_vkAllocateCommandBuffers)(device, pAllocateInfo, pCommandBuffers); }
void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) { ((void (*)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*)) TOS_LINK_vkFreeCommandBuffers)(device, commandPool, commandBufferCount, pCommandBuffers); }
VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) { return ((VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*)) TOS_LINK_vkBeginCommandBuffer)(commandBuffer, pBeginInfo); }
VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) { return ((VkResult (*)(VkCommandBuffer)) TOS_LINK_vkEndCommandBuffer)(commandBuffer); }
VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) { return ((VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags)) TOS_LINK_vkResetCommandBuffer)(commandBuffer, flags); }
void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline)) TOS_LINK_vkCmdBindPipeline)(commandBuffer, pipelineBindPoint, pipeline); }
void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*)) TOS_LINK_vkCmdSetViewport)(commandBuffer, firstViewport, viewportCount, pViewports); }
void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*)) TOS_LINK_vkCmdSetScissor)(commandBuffer, firstScissor, scissorCount, pScissors); }
void vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) { ((void (*)(VkCommandBuffer, float)) TOS_LINK_vkCmdSetLineWidth)(commandBuffer, lineWidth); }
void vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) { ((void (*)(VkCommandBuffer, float, float, float)) TOS_LINK_vkCmdSetDepthBias)(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor); }
void vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) { ((void (*)(VkCommandBuffer, float, float)) TOS_LINK_vkCmdSetDepthBounds)(commandBuffer, minDepthBounds, maxDepthBounds); }
void vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) { ((void (*)(VkCommandBuffer, VkStencilFaceFlags, uint32_t)) TOS_LINK_vkCmdSetStencilCompareMask)(commandBuffer, faceMask, compareMask); }
void vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) { ((void (*)(VkCommandBuffer, VkStencilFaceFlags, uint32_t)) TOS_LINK_vkCmdSetStencilWriteMask)(commandBuffer, faceMask, writeMask); }
void vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) { ((void (*)(VkCommandBuffer, VkStencilFaceFlags, uint32_t)) TOS_LINK_vkCmdSetStencilReference)(commandBuffer, faceMask, reference); }
void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*)) TOS_LINK_vkCmdBindDescriptorSets)(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets); }
void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType)) TOS_LINK_vkCmdBindIndexBuffer)(commandBuffer, buffer, offset, indexType); }
void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*)) TOS_LINK_vkCmdBindVertexBuffers)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets); }
void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDraw)(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance); }
void vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t)) TOS_LINK_vkCmdDrawIndexed)(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }
void vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndirect)(commandBuffer, buffer, offset, drawCount, stride); }
void vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndexedIndirect)(commandBuffer, buffer, offset, drawCount, stride); }
void vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDispatch)(commandBuffer, groupCountX, groupCountY, groupCountZ); }
void vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize)) TOS_LINK_vkCmdDispatchIndirect)(commandBuffer, buffer, offset); }
void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) { ((void (*)(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*)) TOS_LINK_vkCmdCopyBuffer)(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions); }
void vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageCopy*)) TOS_LINK_vkCmdCopyImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions); }
void vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageBlit*, VkFilter)) TOS_LINK_vkCmdBlitImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter); }
void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) { ((void (*)(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*)) TOS_LINK_vkCmdCopyBufferToImage)(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions); }
void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy*)) TOS_LINK_vkCmdCopyImageToBuffer)(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions); }
void vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*)) TOS_LINK_vkCmdUpdateBuffer)(commandBuffer, dstBuffer, dstOffset, dataSize, pData); }
void vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, uint32_t)) TOS_LINK_vkCmdFillBuffer)(commandBuffer, dstBuffer, dstOffset, size, data); }
void vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, const VkClearColorValue*, uint32_t, const VkImageSubresourceRange*)) TOS_LINK_vkCmdClearColorImage)(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges); }
void vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, const VkClearDepthStencilValue*, uint32_t, const VkImageSubresourceRange*)) TOS_LINK_vkCmdClearDepthStencilImage)(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges); }
void vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) { ((void (*)(VkCommandBuffer, uint32_t, const VkClearAttachment*, uint32_t, const VkClearRect*)) TOS_LINK_vkCmdClearAttachments)(commandBuffer, attachmentCount, pAttachments, rectCount, pRects); }
void vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) { ((void (*)(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageResolve*)) TOS_LINK_vkCmdResolveImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions); }
void vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) { ((void (*)(VkCommandBuffer, VkEvent, VkPipelineStageFlags)) TOS_LINK_vkCmdSetEvent)(commandBuffer, event, stageMask); }
void vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) { ((void (*)(VkCommandBuffer, VkEvent, VkPipelineStageFlags)) TOS_LINK_vkCmdResetEvent)(commandBuffer, event, stageMask); }
void vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) { ((void (*)(VkCommandBuffer, uint32_t, const VkEvent*, VkPipelineStageFlags, VkPipelineStageFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*)) TOS_LINK_vkCmdWaitEvents)(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers); }
void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) { ((void (*)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*)) TOS_LINK_vkCmdPipelineBarrier)(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers); }
void vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t, VkQueryControlFlags)) TOS_LINK_vkCmdBeginQuery)(commandBuffer, queryPool, query, flags); }
void vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t)) TOS_LINK_vkCmdEndQuery)(commandBuffer, queryPool, query); }
void vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t)) TOS_LINK_vkCmdResetQueryPool)(commandBuffer, queryPool, firstQuery, queryCount); }
void vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) { ((void (*)(VkCommandBuffer, VkPipelineStageFlagBits, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteTimestamp)(commandBuffer, pipelineStage, queryPool, query); }
void vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags)) TOS_LINK_vkCmdCopyQueryPoolResults)(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags); }
void vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) { ((void (*)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*)) TOS_LINK_vkCmdPushConstants)(commandBuffer, layout, stageFlags, offset, size, pValues); }
void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) { ((void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents)) TOS_LINK_vkCmdBeginRenderPass)(commandBuffer, pRenderPassBegin, contents); }
void vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) { ((void (*)(VkCommandBuffer, VkSubpassContents)) TOS_LINK_vkCmdNextSubpass)(commandBuffer, contents); }
void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdEndRenderPass)(commandBuffer); }
void vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) { ((void (*)(VkCommandBuffer, uint32_t, const VkCommandBuffer*)) TOS_LINK_vkCmdExecuteCommands)(commandBuffer, commandBufferCount, pCommandBuffers); }
VkResult vkEnumerateInstanceVersion(uint32_t* pApiVersion) { return ((VkResult (*)(uint32_t*)) TOS_LINK_vkEnumerateInstanceVersion)(pApiVersion); }
VkResult vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) { return ((VkResult (*)(VkDevice, uint32_t, const VkBindBufferMemoryInfo*)) TOS_LINK_vkBindBufferMemory2)(device, bindInfoCount, pBindInfos); }
VkResult vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) { return ((VkResult (*)(VkDevice, uint32_t, const VkBindImageMemoryInfo*)) TOS_LINK_vkBindImageMemory2)(device, bindInfoCount, pBindInfos); }
void vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) { ((void (*)(VkDevice, uint32_t, uint32_t, uint32_t, VkPeerMemoryFeatureFlags*)) TOS_LINK_vkGetDeviceGroupPeerMemoryFeatures)(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures); }
void vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetDeviceMask)(commandBuffer, deviceMask); }
void vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDispatchBase)(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ); }
VkResult vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) { return ((VkResult (*)(VkInstance, uint32_t*, VkPhysicalDeviceGroupProperties*)) TOS_LINK_vkEnumeratePhysicalDeviceGroups)(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties); }
void vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkImageMemoryRequirementsInfo2*, VkMemoryRequirements2*)) TOS_LINK_vkGetImageMemoryRequirements2)(device, pInfo, pMemoryRequirements); }
void vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkBufferMemoryRequirementsInfo2*, VkMemoryRequirements2*)) TOS_LINK_vkGetBufferMemoryRequirements2)(device, pInfo, pMemoryRequirements); }
void vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) { ((void (*)(VkDevice, const VkImageSparseMemoryRequirementsInfo2*, uint32_t*, VkSparseImageMemoryRequirements2*)) TOS_LINK_vkGetImageSparseMemoryRequirements2)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements); }
void vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2*)) TOS_LINK_vkGetPhysicalDeviceFeatures2)(physicalDevice, pFeatures); }
void vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*)) TOS_LINK_vkGetPhysicalDeviceProperties2)(physicalDevice, pProperties); }
void vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) { ((void (*)(VkPhysicalDevice, VkFormat, VkFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceFormatProperties2)(physicalDevice, format, pFormatProperties); }
VkResult vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2*, VkImageFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2)(physicalDevice, pImageFormatInfo, pImageFormatProperties); }
void vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) { ((void (*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties2*)) TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties); }
void vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2*)) TOS_LINK_vkGetPhysicalDeviceMemoryProperties2)(physicalDevice, pMemoryProperties); }
void vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2*, uint32_t*, VkSparseImageFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2)(physicalDevice, pFormatInfo, pPropertyCount, pProperties); }
void vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) { ((void (*)(VkDevice, VkCommandPool, VkCommandPoolTrimFlags)) TOS_LINK_vkTrimCommandPool)(device, commandPool, flags); }
void vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) { ((void (*)(VkDevice, const VkDeviceQueueInfo2*, VkQueue*)) TOS_LINK_vkGetDeviceQueue2)(device, pQueueInfo, pQueue); }
VkResult vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) { return ((VkResult (*)(VkDevice, const VkSamplerYcbcrConversionCreateInfo*, const VkAllocationCallbacks*, VkSamplerYcbcrConversion*)) TOS_LINK_vkCreateSamplerYcbcrConversion)(device, pCreateInfo, pAllocator, pYcbcrConversion); }
void vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkSamplerYcbcrConversion, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySamplerYcbcrConversion)(device, ycbcrConversion, pAllocator); }
VkResult vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) { return ((VkResult (*)(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*)) TOS_LINK_vkCreateDescriptorUpdateTemplate)(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate); }
void vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDescriptorUpdateTemplate)(device, descriptorUpdateTemplate, pAllocator); }
void vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) { ((void (*)(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void*)) TOS_LINK_vkUpdateDescriptorSetWithTemplate)(device, descriptorSet, descriptorUpdateTemplate, pData); }
void vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo*, VkExternalBufferProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalBufferProperties)(physicalDevice, pExternalBufferInfo, pExternalBufferProperties); }
void vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalFenceInfo*, VkExternalFenceProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalFenceProperties)(physicalDevice, pExternalFenceInfo, pExternalFenceProperties); }
void vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo*, VkExternalSemaphoreProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalSemaphoreProperties)(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties); }
void vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) { ((void (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, VkDescriptorSetLayoutSupport*)) TOS_LINK_vkGetDescriptorSetLayoutSupport)(device, pCreateInfo, pSupport); }
void vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndirectCount)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
void vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndexedIndirectCount)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
VkResult vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) { return ((VkResult (*)(VkDevice, const VkRenderPassCreateInfo2*, const VkAllocationCallbacks*, VkRenderPass*)) TOS_LINK_vkCreateRenderPass2)(device, pCreateInfo, pAllocator, pRenderPass); }
void vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) { ((void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*, const VkSubpassBeginInfo*)) TOS_LINK_vkCmdBeginRenderPass2)(commandBuffer, pRenderPassBegin, pSubpassBeginInfo); }
void vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) { ((void (*)(VkCommandBuffer, const VkSubpassBeginInfo*, const VkSubpassEndInfo*)) TOS_LINK_vkCmdNextSubpass2)(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo); }
void vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) { ((void (*)(VkCommandBuffer, const VkSubpassEndInfo*)) TOS_LINK_vkCmdEndRenderPass2)(commandBuffer, pSubpassEndInfo); }
void vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) { ((void (*)(VkDevice, VkQueryPool, uint32_t, uint32_t)) TOS_LINK_vkResetQueryPool)(device, queryPool, firstQuery, queryCount); }
VkResult vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) { return ((VkResult (*)(VkDevice, VkSemaphore, uint64_t*)) TOS_LINK_vkGetSemaphoreCounterValue)(device, semaphore, pValue); }
VkResult vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) { return ((VkResult (*)(VkDevice, const VkSemaphoreWaitInfo*, uint64_t)) TOS_LINK_vkWaitSemaphores)(device, pWaitInfo, timeout); }
VkResult vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) { return ((VkResult (*)(VkDevice, const VkSemaphoreSignalInfo*)) TOS_LINK_vkSignalSemaphore)(device, pSignalInfo); }
VkDeviceAddress vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) { return ((VkDeviceAddress (*)(VkDevice, const VkBufferDeviceAddressInfo*)) TOS_LINK_vkGetBufferDeviceAddress)(device, pInfo); }
VkResult vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkPhysicalDeviceToolProperties*)) TOS_LINK_vkGetPhysicalDeviceToolProperties)(physicalDevice, pToolCount, pToolProperties); }
VkResult vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) { return ((VkResult (*)(VkDevice, const VkPrivateDataSlotCreateInfo*, const VkAllocationCallbacks*, VkPrivateDataSlot*)) TOS_LINK_vkCreatePrivateDataSlot)(device, pCreateInfo, pAllocator, pPrivateDataSlot); }
void vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkPrivateDataSlot, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyPrivateDataSlot)(device, privateDataSlot, pAllocator); }
VkResult vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) { return ((VkResult (*)(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t)) TOS_LINK_vkSetPrivateData)(device, objectType, objectHandle, privateDataSlot, data); }
void vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) { ((void (*)(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t*)) TOS_LINK_vkGetPrivateData)(device, objectType, objectHandle, privateDataSlot, pData); }
void vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) { ((void (*)(VkCommandBuffer, VkEvent, const VkDependencyInfo*)) TOS_LINK_vkCmdSetEvent2)(commandBuffer, event, pDependencyInfo); }
void vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) { ((void (*)(VkCommandBuffer, VkEvent, VkPipelineStageFlags2)) TOS_LINK_vkCmdResetEvent2)(commandBuffer, event, stageMask); }
void vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) { ((void (*)(VkCommandBuffer, uint32_t, const VkEvent*, const VkDependencyInfo*)) TOS_LINK_vkCmdWaitEvents2)(commandBuffer, eventCount, pEvents, pDependencyInfos); }
void vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) { ((void (*)(VkCommandBuffer, const VkDependencyInfo*)) TOS_LINK_vkCmdPipelineBarrier2)(commandBuffer, pDependencyInfo); }
void vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) { ((void (*)(VkCommandBuffer, VkPipelineStageFlags2, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteTimestamp2)(commandBuffer, stage, queryPool, query); }
VkResult vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) { return ((VkResult (*)(VkQueue, uint32_t, const VkSubmitInfo2*, VkFence)) TOS_LINK_vkQueueSubmit2)(queue, submitCount, pSubmits, fence); }
void vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) { ((void (*)(VkCommandBuffer, const VkCopyBufferInfo2*)) TOS_LINK_vkCmdCopyBuffer2)(commandBuffer, pCopyBufferInfo); }
void vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) { ((void (*)(VkCommandBuffer, const VkCopyImageInfo2*)) TOS_LINK_vkCmdCopyImage2)(commandBuffer, pCopyImageInfo); }
void vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) { ((void (*)(VkCommandBuffer, const VkCopyBufferToImageInfo2*)) TOS_LINK_vkCmdCopyBufferToImage2)(commandBuffer, pCopyBufferToImageInfo); }
void vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) { ((void (*)(VkCommandBuffer, const VkCopyImageToBufferInfo2*)) TOS_LINK_vkCmdCopyImageToBuffer2)(commandBuffer, pCopyImageToBufferInfo); }
void vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) { ((void (*)(VkCommandBuffer, const VkBlitImageInfo2*)) TOS_LINK_vkCmdBlitImage2)(commandBuffer, pBlitImageInfo); }
void vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) { ((void (*)(VkCommandBuffer, const VkResolveImageInfo2*)) TOS_LINK_vkCmdResolveImage2)(commandBuffer, pResolveImageInfo); }
void vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) { ((void (*)(VkCommandBuffer, const VkRenderingInfo*)) TOS_LINK_vkCmdBeginRendering)(commandBuffer, pRenderingInfo); }
void vkCmdEndRendering(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdEndRendering)(commandBuffer); }
void vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) { ((void (*)(VkCommandBuffer, VkCullModeFlags)) TOS_LINK_vkCmdSetCullMode)(commandBuffer, cullMode); }
void vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) { ((void (*)(VkCommandBuffer, VkFrontFace)) TOS_LINK_vkCmdSetFrontFace)(commandBuffer, frontFace); }
void vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) { ((void (*)(VkCommandBuffer, VkPrimitiveTopology)) TOS_LINK_vkCmdSetPrimitiveTopology)(commandBuffer, primitiveTopology); }
void vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) { ((void (*)(VkCommandBuffer, uint32_t, const VkViewport*)) TOS_LINK_vkCmdSetViewportWithCount)(commandBuffer, viewportCount, pViewports); }
void vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) { ((void (*)(VkCommandBuffer, uint32_t, const VkRect2D*)) TOS_LINK_vkCmdSetScissorWithCount)(commandBuffer, scissorCount, pScissors); }
void vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*)) TOS_LINK_vkCmdBindVertexBuffers2)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides); }
void vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthTestEnable)(commandBuffer, depthTestEnable); }
void vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthWriteEnable)(commandBuffer, depthWriteEnable); }
void vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) { ((void (*)(VkCommandBuffer, VkCompareOp)) TOS_LINK_vkCmdSetDepthCompareOp)(commandBuffer, depthCompareOp); }
void vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthBoundsTestEnable)(commandBuffer, depthBoundsTestEnable); }
void vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetStencilTestEnable)(commandBuffer, stencilTestEnable); }
void vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) { ((void (*)(VkCommandBuffer, VkStencilFaceFlags, VkStencilOp, VkStencilOp, VkStencilOp, VkCompareOp)) TOS_LINK_vkCmdSetStencilOp)(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp); }
void vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetRasterizerDiscardEnable)(commandBuffer, rasterizerDiscardEnable); }
void vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthBiasEnable)(commandBuffer, depthBiasEnable); }
void vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetPrimitiveRestartEnable)(commandBuffer, primitiveRestartEnable); }
void vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceBufferMemoryRequirements*, VkMemoryRequirements2*)) TOS_LINK_vkGetDeviceBufferMemoryRequirements)(device, pInfo, pMemoryRequirements); }
void vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceImageMemoryRequirements*, VkMemoryRequirements2*)) TOS_LINK_vkGetDeviceImageMemoryRequirements)(device, pInfo, pMemoryRequirements); }
void vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceImageMemoryRequirements*, uint32_t*, VkSparseImageMemoryRequirements2*)) TOS_LINK_vkGetDeviceImageSparseMemoryRequirements)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements); }
void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySurfaceKHR)(instance, surface, pAllocator); }
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) { return ((VkResult (*)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32*)) TOS_LINK_vkGetPhysicalDeviceSurfaceSupportKHR)(physicalDevice, queueFamilyIndex, surface, pSupported); }
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) { return ((VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*)) TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(physicalDevice, surface, pSurfaceCapabilities); }
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) { return ((VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkSurfaceFormatKHR*)) TOS_LINK_vkGetPhysicalDeviceSurfaceFormatsKHR)(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats); }
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) { return ((VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkPresentModeKHR*)) TOS_LINK_vkGetPhysicalDeviceSurfacePresentModesKHR)(physicalDevice, surface, pPresentModeCount, pPresentModes); }
VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) { return ((VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*)) TOS_LINK_vkCreateSwapchainKHR)(device, pCreateInfo, pAllocator, pSwapchain); }
void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySwapchainKHR)(device, swapchain, pAllocator); }
VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*)) TOS_LINK_vkGetSwapchainImagesKHR)(device, swapchain, pSwapchainImageCount, pSwapchainImages); }
VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*)) TOS_LINK_vkAcquireNextImageKHR)(device, swapchain, timeout, semaphore, fence, pImageIndex); }
VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) { return ((VkResult (*)(VkQueue, const VkPresentInfoKHR*)) TOS_LINK_vkQueuePresentKHR)(queue, pPresentInfo); }
VkResult vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) { return ((VkResult (*)(VkDevice, VkDeviceGroupPresentCapabilitiesKHR*)) TOS_LINK_vkGetDeviceGroupPresentCapabilitiesKHR)(device, pDeviceGroupPresentCapabilities); }
VkResult vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) { return ((VkResult (*)(VkDevice, VkSurfaceKHR, VkDeviceGroupPresentModeFlagsKHR*)) TOS_LINK_vkGetDeviceGroupSurfacePresentModesKHR)(device, surface, pModes); }
VkResult vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) { return ((VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkRect2D*)) TOS_LINK_vkGetPhysicalDevicePresentRectanglesKHR)(physicalDevice, surface, pRectCount, pRects); }
VkResult vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) { return ((VkResult (*)(VkDevice, const VkAcquireNextImageInfoKHR*, uint32_t*)) TOS_LINK_vkAcquireNextImage2KHR)(device, pAcquireInfo, pImageIndex); }
VkResult vkGetPhysicalDeviceDisplayPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPropertiesKHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkDisplayPropertiesKHR*)) TOS_LINK_vkGetPhysicalDeviceDisplayPropertiesKHR)(physicalDevice, pPropertyCount, pProperties); }
VkResult vkGetPhysicalDeviceDisplayPlanePropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlanePropertiesKHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkDisplayPlanePropertiesKHR*)) TOS_LINK_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)(physicalDevice, pPropertyCount, pProperties); }
VkResult vkGetDisplayPlaneSupportedDisplaysKHR(VkPhysicalDevice physicalDevice, uint32_t planeIndex, uint32_t* pDisplayCount, VkDisplayKHR* pDisplays) { return ((VkResult (*)(VkPhysicalDevice, uint32_t, uint32_t*, VkDisplayKHR*)) TOS_LINK_vkGetDisplayPlaneSupportedDisplaysKHR)(physicalDevice, planeIndex, pDisplayCount, pDisplays); }
VkResult vkGetDisplayModePropertiesKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModePropertiesKHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, VkDisplayKHR, uint32_t*, VkDisplayModePropertiesKHR*)) TOS_LINK_vkGetDisplayModePropertiesKHR)(physicalDevice, display, pPropertyCount, pProperties); }
VkResult vkCreateDisplayModeKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, const VkDisplayModeCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDisplayModeKHR* pMode) { return ((VkResult (*)(VkPhysicalDevice, VkDisplayKHR, const VkDisplayModeCreateInfoKHR*, const VkAllocationCallbacks*, VkDisplayModeKHR*)) TOS_LINK_vkCreateDisplayModeKHR)(physicalDevice, display, pCreateInfo, pAllocator, pMode); }
VkResult vkGetDisplayPlaneCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode, uint32_t planeIndex, VkDisplayPlaneCapabilitiesKHR* pCapabilities) { return ((VkResult (*)(VkPhysicalDevice, VkDisplayModeKHR, uint32_t, VkDisplayPlaneCapabilitiesKHR*)) TOS_LINK_vkGetDisplayPlaneCapabilitiesKHR)(physicalDevice, mode, planeIndex, pCapabilities); }
VkResult vkCreateDisplayPlaneSurfaceKHR(VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) { return ((VkResult (*)(VkInstance, const VkDisplaySurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*)) TOS_LINK_vkCreateDisplayPlaneSurfaceKHR)(instance, pCreateInfo, pAllocator, pSurface); }
VkResult vkCreateSharedSwapchainsKHR(VkDevice device, uint32_t swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchains) { return ((VkResult (*)(VkDevice, uint32_t, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*)) TOS_LINK_vkCreateSharedSwapchainsKHR)(device, swapchainCount, pCreateInfos, pAllocator, pSwapchains); }
VkResult vkGetPhysicalDeviceVideoCapabilitiesKHR(VkPhysicalDevice physicalDevice, const VkVideoProfileInfoKHR* pVideoProfile, VkVideoCapabilitiesKHR* pCapabilities) { return ((VkResult (*)(VkPhysicalDevice, const VkVideoProfileInfoKHR*, VkVideoCapabilitiesKHR*)) TOS_LINK_vkGetPhysicalDeviceVideoCapabilitiesKHR)(physicalDevice, pVideoProfile, pCapabilities); }
VkResult vkGetPhysicalDeviceVideoFormatPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR* pVideoFormatInfo, uint32_t* pVideoFormatPropertyCount, VkVideoFormatPropertiesKHR* pVideoFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR*, uint32_t*, VkVideoFormatPropertiesKHR*)) TOS_LINK_vkGetPhysicalDeviceVideoFormatPropertiesKHR)(physicalDevice, pVideoFormatInfo, pVideoFormatPropertyCount, pVideoFormatProperties); }
VkResult vkCreateVideoSessionKHR(VkDevice device, const VkVideoSessionCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionKHR* pVideoSession) { return ((VkResult (*)(VkDevice, const VkVideoSessionCreateInfoKHR*, const VkAllocationCallbacks*, VkVideoSessionKHR*)) TOS_LINK_vkCreateVideoSessionKHR)(device, pCreateInfo, pAllocator, pVideoSession); }
void vkDestroyVideoSessionKHR(VkDevice device, VkVideoSessionKHR videoSession, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkVideoSessionKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyVideoSessionKHR)(device, videoSession, pAllocator); }
VkResult vkGetVideoSessionMemoryRequirementsKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t* pMemoryRequirementsCount, VkVideoSessionMemoryRequirementsKHR* pMemoryRequirements) { return ((VkResult (*)(VkDevice, VkVideoSessionKHR, uint32_t*, VkVideoSessionMemoryRequirementsKHR*)) TOS_LINK_vkGetVideoSessionMemoryRequirementsKHR)(device, videoSession, pMemoryRequirementsCount, pMemoryRequirements); }
VkResult vkBindVideoSessionMemoryKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t bindSessionMemoryInfoCount, const VkBindVideoSessionMemoryInfoKHR* pBindSessionMemoryInfos) { return ((VkResult (*)(VkDevice, VkVideoSessionKHR, uint32_t, const VkBindVideoSessionMemoryInfoKHR*)) TOS_LINK_vkBindVideoSessionMemoryKHR)(device, videoSession, bindSessionMemoryInfoCount, pBindSessionMemoryInfos); }
VkResult vkCreateVideoSessionParametersKHR(VkDevice device, const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionParametersKHR* pVideoSessionParameters) { return ((VkResult (*)(VkDevice, const VkVideoSessionParametersCreateInfoKHR*, const VkAllocationCallbacks*, VkVideoSessionParametersKHR*)) TOS_LINK_vkCreateVideoSessionParametersKHR)(device, pCreateInfo, pAllocator, pVideoSessionParameters); }
VkResult vkUpdateVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo) { return ((VkResult (*)(VkDevice, VkVideoSessionParametersKHR, const VkVideoSessionParametersUpdateInfoKHR*)) TOS_LINK_vkUpdateVideoSessionParametersKHR)(device, videoSessionParameters, pUpdateInfo); }
void vkDestroyVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkVideoSessionParametersKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyVideoSessionParametersKHR)(device, videoSessionParameters, pAllocator); }
void vkCmdBeginVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR* pBeginInfo) { ((void (*)(VkCommandBuffer, const VkVideoBeginCodingInfoKHR*)) TOS_LINK_vkCmdBeginVideoCodingKHR)(commandBuffer, pBeginInfo); }
void vkCmdEndVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR* pEndCodingInfo) { ((void (*)(VkCommandBuffer, const VkVideoEndCodingInfoKHR*)) TOS_LINK_vkCmdEndVideoCodingKHR)(commandBuffer, pEndCodingInfo); }
void vkCmdControlVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR* pCodingControlInfo) { ((void (*)(VkCommandBuffer, const VkVideoCodingControlInfoKHR*)) TOS_LINK_vkCmdControlVideoCodingKHR)(commandBuffer, pCodingControlInfo); }
void vkCmdDecodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR* pDecodeInfo) { ((void (*)(VkCommandBuffer, const VkVideoDecodeInfoKHR*)) TOS_LINK_vkCmdDecodeVideoKHR)(commandBuffer, pDecodeInfo); }
void vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) { ((void (*)(VkCommandBuffer, const VkRenderingInfo*)) TOS_LINK_vkCmdBeginRenderingKHR)(commandBuffer, pRenderingInfo); }
void vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdEndRenderingKHR)(commandBuffer); }
void vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2*)) TOS_LINK_vkGetPhysicalDeviceFeatures2KHR)(physicalDevice, pFeatures); }
void vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*)) TOS_LINK_vkGetPhysicalDeviceProperties2KHR)(physicalDevice, pProperties); }
void vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) { ((void (*)(VkPhysicalDevice, VkFormat, VkFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceFormatProperties2KHR)(physicalDevice, format, pFormatProperties); }
VkResult vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2*, VkImageFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2KHR)(physicalDevice, pImageFormatInfo, pImageFormatProperties); }
void vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) { ((void (*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties2*)) TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2KHR)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties); }
void vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) { ((void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2*)) TOS_LINK_vkGetPhysicalDeviceMemoryProperties2KHR)(physicalDevice, pMemoryProperties); }
void vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2*, uint32_t*, VkSparseImageFormatProperties2*)) TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)(physicalDevice, pFormatInfo, pPropertyCount, pProperties); }
void vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) { ((void (*)(VkDevice, uint32_t, uint32_t, uint32_t, VkPeerMemoryFeatureFlags*)) TOS_LINK_vkGetDeviceGroupPeerMemoryFeaturesKHR)(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures); }
void vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetDeviceMaskKHR)(commandBuffer, deviceMask); }
void vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDispatchBaseKHR)(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ); }
void vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) { ((void (*)(VkDevice, VkCommandPool, VkCommandPoolTrimFlags)) TOS_LINK_vkTrimCommandPoolKHR)(device, commandPool, flags); }
VkResult vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) { return ((VkResult (*)(VkInstance, uint32_t*, VkPhysicalDeviceGroupProperties*)) TOS_LINK_vkEnumeratePhysicalDeviceGroupsKHR)(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties); }
void vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo*, VkExternalBufferProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalBufferPropertiesKHR)(physicalDevice, pExternalBufferInfo, pExternalBufferProperties); }
VkResult vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) { return ((VkResult (*)(VkDevice, const VkMemoryGetFdInfoKHR*, int*)) TOS_LINK_vkGetMemoryFdKHR)(device, pGetFdInfo, pFd); }
VkResult vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) { return ((VkResult (*)(VkDevice, VkExternalMemoryHandleTypeFlagBits, int, VkMemoryFdPropertiesKHR*)) TOS_LINK_vkGetMemoryFdPropertiesKHR)(device, handleType, fd, pMemoryFdProperties); }
void vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo*, VkExternalSemaphoreProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties); }
VkResult vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) { return ((VkResult (*)(VkDevice, const VkImportSemaphoreFdInfoKHR*)) TOS_LINK_vkImportSemaphoreFdKHR)(device, pImportSemaphoreFdInfo); }
VkResult vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) { return ((VkResult (*)(VkDevice, const VkSemaphoreGetFdInfoKHR*, int*)) TOS_LINK_vkGetSemaphoreFdKHR)(device, pGetFdInfo, pFd); }
void vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkWriteDescriptorSet*)) TOS_LINK_vkCmdPushDescriptorSetKHR)(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites); }
void vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) { ((void (*)(VkCommandBuffer, VkDescriptorUpdateTemplate, VkPipelineLayout, uint32_t, const void*)) TOS_LINK_vkCmdPushDescriptorSetWithTemplateKHR)(commandBuffer, descriptorUpdateTemplate, layout, set, pData); }
VkResult vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) { return ((VkResult (*)(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*)) TOS_LINK_vkCreateDescriptorUpdateTemplateKHR)(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate); }
void vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDescriptorUpdateTemplateKHR)(device, descriptorUpdateTemplate, pAllocator); }
void vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) { ((void (*)(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void*)) TOS_LINK_vkUpdateDescriptorSetWithTemplateKHR)(device, descriptorSet, descriptorUpdateTemplate, pData); }
VkResult vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) { return ((VkResult (*)(VkDevice, const VkRenderPassCreateInfo2*, const VkAllocationCallbacks*, VkRenderPass*)) TOS_LINK_vkCreateRenderPass2KHR)(device, pCreateInfo, pAllocator, pRenderPass); }
void vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) { ((void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*, const VkSubpassBeginInfo*)) TOS_LINK_vkCmdBeginRenderPass2KHR)(commandBuffer, pRenderPassBegin, pSubpassBeginInfo); }
void vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) { ((void (*)(VkCommandBuffer, const VkSubpassBeginInfo*, const VkSubpassEndInfo*)) TOS_LINK_vkCmdNextSubpass2KHR)(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo); }
void vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) { ((void (*)(VkCommandBuffer, const VkSubpassEndInfo*)) TOS_LINK_vkCmdEndRenderPass2KHR)(commandBuffer, pSubpassEndInfo); }
VkResult vkGetSwapchainStatusKHR(VkDevice device, VkSwapchainKHR swapchain) { return ((VkResult (*)(VkDevice, VkSwapchainKHR)) TOS_LINK_vkGetSwapchainStatusKHR)(device, swapchain); }
void vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) { ((void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalFenceInfo*, VkExternalFenceProperties*)) TOS_LINK_vkGetPhysicalDeviceExternalFencePropertiesKHR)(physicalDevice, pExternalFenceInfo, pExternalFenceProperties); }
VkResult vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) { return ((VkResult (*)(VkDevice, const VkImportFenceFdInfoKHR*)) TOS_LINK_vkImportFenceFdKHR)(device, pImportFenceFdInfo); }
VkResult vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) { return ((VkResult (*)(VkDevice, const VkFenceGetFdInfoKHR*, int*)) TOS_LINK_vkGetFenceFdKHR)(device, pGetFdInfo, pFd); }
VkResult vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pCounterCount, VkPerformanceCounterKHR* pCounters, VkPerformanceCounterDescriptionKHR* pCounterDescriptions) { return ((VkResult (*)(VkPhysicalDevice, uint32_t, uint32_t*, VkPerformanceCounterKHR*, VkPerformanceCounterDescriptionKHR*)) TOS_LINK_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)(physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions); }
void vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, uint32_t* pNumPasses) { ((void (*)(VkPhysicalDevice, const VkQueryPoolPerformanceCreateInfoKHR*, uint32_t*)) TOS_LINK_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)(physicalDevice, pPerformanceQueryCreateInfo, pNumPasses); }
VkResult vkAcquireProfilingLockKHR(VkDevice device, const VkAcquireProfilingLockInfoKHR* pInfo) { return ((VkResult (*)(VkDevice, const VkAcquireProfilingLockInfoKHR*)) TOS_LINK_vkAcquireProfilingLockKHR)(device, pInfo); }
void vkReleaseProfilingLockKHR(VkDevice device) { ((void (*)(VkDevice)) TOS_LINK_vkReleaseProfilingLockKHR)(device); }
VkResult vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkSurfaceCapabilities2KHR* pSurfaceCapabilities) { return ((VkResult (*)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR*, VkSurfaceCapabilities2KHR*)) TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2KHR)(physicalDevice, pSurfaceInfo, pSurfaceCapabilities); }
VkResult vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats) { return ((VkResult (*)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR*, uint32_t*, VkSurfaceFormat2KHR*)) TOS_LINK_vkGetPhysicalDeviceSurfaceFormats2KHR)(physicalDevice, pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats); }
VkResult vkGetPhysicalDeviceDisplayProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayProperties2KHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkDisplayProperties2KHR*)) TOS_LINK_vkGetPhysicalDeviceDisplayProperties2KHR)(physicalDevice, pPropertyCount, pProperties); }
VkResult vkGetPhysicalDeviceDisplayPlaneProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlaneProperties2KHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkDisplayPlaneProperties2KHR*)) TOS_LINK_vkGetPhysicalDeviceDisplayPlaneProperties2KHR)(physicalDevice, pPropertyCount, pProperties); }
VkResult vkGetDisplayModeProperties2KHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModeProperties2KHR* pProperties) { return ((VkResult (*)(VkPhysicalDevice, VkDisplayKHR, uint32_t*, VkDisplayModeProperties2KHR*)) TOS_LINK_vkGetDisplayModeProperties2KHR)(physicalDevice, display, pPropertyCount, pProperties); }
VkResult vkGetDisplayPlaneCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkDisplayPlaneInfo2KHR* pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR* pCapabilities) { return ((VkResult (*)(VkPhysicalDevice, const VkDisplayPlaneInfo2KHR*, VkDisplayPlaneCapabilities2KHR*)) TOS_LINK_vkGetDisplayPlaneCapabilities2KHR)(physicalDevice, pDisplayPlaneInfo, pCapabilities); }
void vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkImageMemoryRequirementsInfo2*, VkMemoryRequirements2*)) TOS_LINK_vkGetImageMemoryRequirements2KHR)(device, pInfo, pMemoryRequirements); }
void vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkBufferMemoryRequirementsInfo2*, VkMemoryRequirements2*)) TOS_LINK_vkGetBufferMemoryRequirements2KHR)(device, pInfo, pMemoryRequirements); }
void vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) { ((void (*)(VkDevice, const VkImageSparseMemoryRequirementsInfo2*, uint32_t*, VkSparseImageMemoryRequirements2*)) TOS_LINK_vkGetImageSparseMemoryRequirements2KHR)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements); }
VkResult vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) { return ((VkResult (*)(VkDevice, const VkSamplerYcbcrConversionCreateInfo*, const VkAllocationCallbacks*, VkSamplerYcbcrConversion*)) TOS_LINK_vkCreateSamplerYcbcrConversionKHR)(device, pCreateInfo, pAllocator, pYcbcrConversion); }
void vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkSamplerYcbcrConversion, const VkAllocationCallbacks*)) TOS_LINK_vkDestroySamplerYcbcrConversionKHR)(device, ycbcrConversion, pAllocator); }
VkResult vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) { return ((VkResult (*)(VkDevice, uint32_t, const VkBindBufferMemoryInfo*)) TOS_LINK_vkBindBufferMemory2KHR)(device, bindInfoCount, pBindInfos); }
VkResult vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) { return ((VkResult (*)(VkDevice, uint32_t, const VkBindImageMemoryInfo*)) TOS_LINK_vkBindImageMemory2KHR)(device, bindInfoCount, pBindInfos); }
void vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) { ((void (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, VkDescriptorSetLayoutSupport*)) TOS_LINK_vkGetDescriptorSetLayoutSupportKHR)(device, pCreateInfo, pSupport); }
void vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndirectCountKHR)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
void vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndexedIndirectCountKHR)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
VkResult vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) { return ((VkResult (*)(VkDevice, VkSemaphore, uint64_t*)) TOS_LINK_vkGetSemaphoreCounterValueKHR)(device, semaphore, pValue); }
VkResult vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) { return ((VkResult (*)(VkDevice, const VkSemaphoreWaitInfo*, uint64_t)) TOS_LINK_vkWaitSemaphoresKHR)(device, pWaitInfo, timeout); }
VkResult vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) { return ((VkResult (*)(VkDevice, const VkSemaphoreSignalInfo*)) TOS_LINK_vkSignalSemaphoreKHR)(device, pSignalInfo); }
VkResult vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkPhysicalDeviceFragmentShadingRateKHR*)) TOS_LINK_vkGetPhysicalDeviceFragmentShadingRatesKHR)(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates); }
VkResult vkWaitForPresentKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, uint64_t, uint64_t)) TOS_LINK_vkWaitForPresentKHR)(device, swapchain, presentId, timeout); }
VkDeviceAddress vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) { return ((VkDeviceAddress (*)(VkDevice, const VkBufferDeviceAddressInfo*)) TOS_LINK_vkGetBufferDeviceAddressKHR)(device, pInfo); }
VkResult vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) { return ((VkResult (*)(VkDevice, const VkAllocationCallbacks*, VkDeferredOperationKHR*)) TOS_LINK_vkCreateDeferredOperationKHR)(device, pAllocator, pDeferredOperation); }
void vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkDeferredOperationKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDeferredOperationKHR)(device, operation, pAllocator); }
VkResult vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR)) TOS_LINK_vkGetDeferredOperationResultKHR)(device, operation); }
VkResult vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR)) TOS_LINK_vkDeferredOperationJoinKHR)(device, operation); }
VkResult vkGetPipelineExecutablePropertiesKHR(VkDevice device, const VkPipelineInfoKHR* pPipelineInfo, uint32_t* pExecutableCount, VkPipelineExecutablePropertiesKHR* pProperties) { return ((VkResult (*)(VkDevice, const VkPipelineInfoKHR*, uint32_t*, VkPipelineExecutablePropertiesKHR*)) TOS_LINK_vkGetPipelineExecutablePropertiesKHR)(device, pPipelineInfo, pExecutableCount, pProperties); }
VkResult vkGetPipelineExecutableStatisticsKHR(VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pStatisticCount, VkPipelineExecutableStatisticKHR* pStatistics) { return ((VkResult (*)(VkDevice, const VkPipelineExecutableInfoKHR*, uint32_t*, VkPipelineExecutableStatisticKHR*)) TOS_LINK_vkGetPipelineExecutableStatisticsKHR)(device, pExecutableInfo, pStatisticCount, pStatistics); }
VkResult vkGetPipelineExecutableInternalRepresentationsKHR(VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations) { return ((VkResult (*)(VkDevice, const VkPipelineExecutableInfoKHR*, uint32_t*, VkPipelineExecutableInternalRepresentationKHR*)) TOS_LINK_vkGetPipelineExecutableInternalRepresentationsKHR)(device, pExecutableInfo, pInternalRepresentationCount, pInternalRepresentations); }
void vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) { ((void (*)(VkCommandBuffer, VkEvent, const VkDependencyInfo*)) TOS_LINK_vkCmdSetEvent2KHR)(commandBuffer, event, pDependencyInfo); }
void vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) { ((void (*)(VkCommandBuffer, VkEvent, VkPipelineStageFlags2)) TOS_LINK_vkCmdResetEvent2KHR)(commandBuffer, event, stageMask); }
void vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) { ((void (*)(VkCommandBuffer, uint32_t, const VkEvent*, const VkDependencyInfo*)) TOS_LINK_vkCmdWaitEvents2KHR)(commandBuffer, eventCount, pEvents, pDependencyInfos); }
void vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) { ((void (*)(VkCommandBuffer, const VkDependencyInfo*)) TOS_LINK_vkCmdPipelineBarrier2KHR)(commandBuffer, pDependencyInfo); }
void vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) { ((void (*)(VkCommandBuffer, VkPipelineStageFlags2, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteTimestamp2KHR)(commandBuffer, stage, queryPool, query); }
VkResult vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) { return ((VkResult (*)(VkQueue, uint32_t, const VkSubmitInfo2*, VkFence)) TOS_LINK_vkQueueSubmit2KHR)(queue, submitCount, pSubmits, fence); }
void vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) { ((void (*)(VkCommandBuffer, VkPipelineStageFlags2, VkBuffer, VkDeviceSize, uint32_t)) TOS_LINK_vkCmdWriteBufferMarker2AMD)(commandBuffer, stage, dstBuffer, dstOffset, marker); }
void vkGetQueueCheckpointData2NV(VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData) { ((void (*)(VkQueue, uint32_t*, VkCheckpointData2NV*)) TOS_LINK_vkGetQueueCheckpointData2NV)(queue, pCheckpointDataCount, pCheckpointData); }
void vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) { ((void (*)(VkCommandBuffer, const VkCopyBufferInfo2*)) TOS_LINK_vkCmdCopyBuffer2KHR)(commandBuffer, pCopyBufferInfo); }
void vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) { ((void (*)(VkCommandBuffer, const VkCopyImageInfo2*)) TOS_LINK_vkCmdCopyImage2KHR)(commandBuffer, pCopyImageInfo); }
void vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) { ((void (*)(VkCommandBuffer, const VkCopyBufferToImageInfo2*)) TOS_LINK_vkCmdCopyBufferToImage2KHR)(commandBuffer, pCopyBufferToImageInfo); }
void vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) { ((void (*)(VkCommandBuffer, const VkCopyImageToBufferInfo2*)) TOS_LINK_vkCmdCopyImageToBuffer2KHR)(commandBuffer, pCopyImageToBufferInfo); }
void vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) { ((void (*)(VkCommandBuffer, const VkBlitImageInfo2*)) TOS_LINK_vkCmdBlitImage2KHR)(commandBuffer, pBlitImageInfo); }
void vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) { ((void (*)(VkCommandBuffer, const VkResolveImageInfo2*)) TOS_LINK_vkCmdResolveImage2KHR)(commandBuffer, pResolveImageInfo); }
void vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress) { ((void (*)(VkCommandBuffer, VkDeviceAddress)) TOS_LINK_vkCmdTraceRaysIndirect2KHR)(commandBuffer, indirectDeviceAddress); }
void vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceBufferMemoryRequirements*, VkMemoryRequirements2*)) TOS_LINK_vkGetDeviceBufferMemoryRequirementsKHR)(device, pInfo, pMemoryRequirements); }
void vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceImageMemoryRequirements*, VkMemoryRequirements2*)) TOS_LINK_vkGetDeviceImageMemoryRequirementsKHR)(device, pInfo, pMemoryRequirements); }
void vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) { ((void (*)(VkDevice, const VkDeviceImageMemoryRequirements*, uint32_t*, VkSparseImageMemoryRequirements2*)) TOS_LINK_vkGetDeviceImageSparseMemoryRequirementsKHR)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements); }
VkResult vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) { return ((VkResult (*)(VkInstance, const VkDebugReportCallbackCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugReportCallbackEXT*)) TOS_LINK_vkCreateDebugReportCallbackEXT)(instance, pCreateInfo, pAllocator, pCallback); }
void vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkInstance, VkDebugReportCallbackEXT, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDebugReportCallbackEXT)(instance, callback, pAllocator); }
void vkDebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage) { ((void (*)(VkInstance, VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char*, const char*)) TOS_LINK_vkDebugReportMessageEXT)(instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage); }
VkResult vkDebugMarkerSetObjectTagEXT(VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo) { return ((VkResult (*)(VkDevice, const VkDebugMarkerObjectTagInfoEXT*)) TOS_LINK_vkDebugMarkerSetObjectTagEXT)(device, pTagInfo); }
VkResult vkDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo) { return ((VkResult (*)(VkDevice, const VkDebugMarkerObjectNameInfoEXT*)) TOS_LINK_vkDebugMarkerSetObjectNameEXT)(device, pNameInfo); }
void vkCmdDebugMarkerBeginEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) { ((void (*)(VkCommandBuffer, const VkDebugMarkerMarkerInfoEXT*)) TOS_LINK_vkCmdDebugMarkerBeginEXT)(commandBuffer, pMarkerInfo); }
void vkCmdDebugMarkerEndEXT(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdDebugMarkerEndEXT)(commandBuffer); }
void vkCmdDebugMarkerInsertEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) { ((void (*)(VkCommandBuffer, const VkDebugMarkerMarkerInfoEXT*)) TOS_LINK_vkCmdDebugMarkerInsertEXT)(commandBuffer, pMarkerInfo); }
void vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*)) TOS_LINK_vkCmdBindTransformFeedbackBuffersEXT)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes); }
void vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*)) TOS_LINK_vkCmdBeginTransformFeedbackEXT)(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets); }
void vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*)) TOS_LINK_vkCmdEndTransformFeedbackEXT)(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets); }
void vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t, VkQueryControlFlags, uint32_t)) TOS_LINK_vkCmdBeginQueryIndexedEXT)(commandBuffer, queryPool, query, flags, index); }
void vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) { ((void (*)(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t)) TOS_LINK_vkCmdEndQueryIndexedEXT)(commandBuffer, queryPool, query, index); }
void vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndirectByteCountEXT)(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride); }
VkResult vkCreateCuModuleNVX(VkDevice device, const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuModuleNVX* pModule) { return ((VkResult (*)(VkDevice, const VkCuModuleCreateInfoNVX*, const VkAllocationCallbacks*, VkCuModuleNVX*)) TOS_LINK_vkCreateCuModuleNVX)(device, pCreateInfo, pAllocator, pModule); }
VkResult vkCreateCuFunctionNVX(VkDevice device, const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuFunctionNVX* pFunction) { return ((VkResult (*)(VkDevice, const VkCuFunctionCreateInfoNVX*, const VkAllocationCallbacks*, VkCuFunctionNVX*)) TOS_LINK_vkCreateCuFunctionNVX)(device, pCreateInfo, pAllocator, pFunction); }
void vkDestroyCuModuleNVX(VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkCuModuleNVX, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyCuModuleNVX)(device, module, pAllocator); }
void vkDestroyCuFunctionNVX(VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkCuFunctionNVX, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyCuFunctionNVX)(device, function, pAllocator); }
void vkCmdCuLaunchKernelNVX(VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo) { ((void (*)(VkCommandBuffer, const VkCuLaunchInfoNVX*)) TOS_LINK_vkCmdCuLaunchKernelNVX)(commandBuffer, pLaunchInfo); }
VkResult vkGetImageViewAddressNVX(VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties) { return ((VkResult (*)(VkDevice, VkImageView, VkImageViewAddressPropertiesNVX*)) TOS_LINK_vkGetImageViewAddressNVX)(device, imageView, pProperties); }
void vkCmdDrawIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndirectCountAMD)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
void vkCmdDrawIndexedIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawIndexedIndirectCountAMD)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
VkResult vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t* pInfoSize, void* pInfo) { return ((VkResult (*)(VkDevice, VkPipeline, VkShaderStageFlagBits, VkShaderInfoTypeAMD, size_t*, void*)) TOS_LINK_vkGetShaderInfoAMD)(device, pipeline, shaderStage, infoType, pInfoSize, pInfo); }
VkResult vkGetPhysicalDeviceExternalImageFormatPropertiesNV(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkExternalMemoryHandleTypeFlagsNV externalHandleType, VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkExternalMemoryHandleTypeFlagsNV, VkExternalImageFormatPropertiesNV*)) TOS_LINK_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)(physicalDevice, format, type, tiling, usage, flags, externalHandleType, pExternalImageFormatProperties); }
void vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) { ((void (*)(VkCommandBuffer, const VkConditionalRenderingBeginInfoEXT*)) TOS_LINK_vkCmdBeginConditionalRenderingEXT)(commandBuffer, pConditionalRenderingBegin); }
void vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdEndConditionalRenderingEXT)(commandBuffer); }
void vkCmdSetViewportWScalingNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV* pViewportWScalings) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkViewportWScalingNV*)) TOS_LINK_vkCmdSetViewportWScalingNV)(commandBuffer, firstViewport, viewportCount, pViewportWScalings); }
VkResult vkReleaseDisplayEXT(VkPhysicalDevice physicalDevice, VkDisplayKHR display) { return ((VkResult (*)(VkPhysicalDevice, VkDisplayKHR)) TOS_LINK_vkReleaseDisplayEXT)(physicalDevice, display); }
VkResult vkGetPhysicalDeviceSurfaceCapabilities2EXT(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilities2EXT* pSurfaceCapabilities) { return ((VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilities2EXT*)) TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2EXT)(physicalDevice, surface, pSurfaceCapabilities); }
VkResult vkDisplayPowerControlEXT(VkDevice device, VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo) { return ((VkResult (*)(VkDevice, VkDisplayKHR, const VkDisplayPowerInfoEXT*)) TOS_LINK_vkDisplayPowerControlEXT)(device, display, pDisplayPowerInfo); }
VkResult vkRegisterDeviceEventEXT(VkDevice device, const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) { return ((VkResult (*)(VkDevice, const VkDeviceEventInfoEXT*, const VkAllocationCallbacks*, VkFence*)) TOS_LINK_vkRegisterDeviceEventEXT)(device, pDeviceEventInfo, pAllocator, pFence); }
VkResult vkRegisterDisplayEventEXT(VkDevice device, VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) { return ((VkResult (*)(VkDevice, VkDisplayKHR, const VkDisplayEventInfoEXT*, const VkAllocationCallbacks*, VkFence*)) TOS_LINK_vkRegisterDisplayEventEXT)(device, display, pDisplayEventInfo, pAllocator, pFence); }
VkResult vkGetSwapchainCounterEXT(VkDevice device, VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, uint64_t* pCounterValue) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, VkSurfaceCounterFlagBitsEXT, uint64_t*)) TOS_LINK_vkGetSwapchainCounterEXT)(device, swapchain, counter, pCounterValue); }
VkResult vkGetRefreshCycleDurationGOOGLE(VkDevice device, VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, VkRefreshCycleDurationGOOGLE*)) TOS_LINK_vkGetRefreshCycleDurationGOOGLE)(device, swapchain, pDisplayTimingProperties); }
VkResult vkGetPastPresentationTimingGOOGLE(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pPresentationTimingCount, VkPastPresentationTimingGOOGLE* pPresentationTimings) { return ((VkResult (*)(VkDevice, VkSwapchainKHR, uint32_t*, VkPastPresentationTimingGOOGLE*)) TOS_LINK_vkGetPastPresentationTimingGOOGLE)(device, swapchain, pPresentationTimingCount, pPresentationTimings); }
void vkCmdSetDiscardRectangleEXT(VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D* pDiscardRectangles) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*)) TOS_LINK_vkCmdSetDiscardRectangleEXT)(commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles); }
void vkCmdSetDiscardRectangleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 discardRectangleEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDiscardRectangleEnableEXT)(commandBuffer, discardRectangleEnable); }
void vkCmdSetDiscardRectangleModeEXT(VkCommandBuffer commandBuffer, VkDiscardRectangleModeEXT discardRectangleMode) { ((void (*)(VkCommandBuffer, VkDiscardRectangleModeEXT)) TOS_LINK_vkCmdSetDiscardRectangleModeEXT)(commandBuffer, discardRectangleMode); }
void vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) { ((void (*)(VkDevice, uint32_t, const VkSwapchainKHR*, const VkHdrMetadataEXT*)) TOS_LINK_vkSetHdrMetadataEXT)(device, swapchainCount, pSwapchains, pMetadata); }
VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) { return ((VkResult (*)(VkDevice, const VkDebugUtilsObjectNameInfoEXT*)) TOS_LINK_vkSetDebugUtilsObjectNameEXT)(device, pNameInfo); }
VkResult vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) { return ((VkResult (*)(VkDevice, const VkDebugUtilsObjectTagInfoEXT*)) TOS_LINK_vkSetDebugUtilsObjectTagEXT)(device, pTagInfo); }
void vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) { ((void (*)(VkQueue, const VkDebugUtilsLabelEXT*)) TOS_LINK_vkQueueBeginDebugUtilsLabelEXT)(queue, pLabelInfo); }
void vkQueueEndDebugUtilsLabelEXT(VkQueue queue) { ((void (*)(VkQueue)) TOS_LINK_vkQueueEndDebugUtilsLabelEXT)(queue); }
void vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) { ((void (*)(VkQueue, const VkDebugUtilsLabelEXT*)) TOS_LINK_vkQueueInsertDebugUtilsLabelEXT)(queue, pLabelInfo); }
void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) { ((void (*)(VkCommandBuffer, const VkDebugUtilsLabelEXT*)) TOS_LINK_vkCmdBeginDebugUtilsLabelEXT)(commandBuffer, pLabelInfo); }
void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdEndDebugUtilsLabelEXT)(commandBuffer); }
void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) { ((void (*)(VkCommandBuffer, const VkDebugUtilsLabelEXT*)) TOS_LINK_vkCmdInsertDebugUtilsLabelEXT)(commandBuffer, pLabelInfo); }
VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) { return ((VkResult (*)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*)) TOS_LINK_vkCreateDebugUtilsMessengerEXT)(instance, pCreateInfo, pAllocator, pMessenger); }
void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyDebugUtilsMessengerEXT)(instance, messenger, pAllocator); }
void vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) { ((void (*)(VkInstance, VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*)) TOS_LINK_vkSubmitDebugUtilsMessageEXT)(instance, messageSeverity, messageTypes, pCallbackData); }
void vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) { ((void (*)(VkCommandBuffer, const VkSampleLocationsInfoEXT*)) TOS_LINK_vkCmdSetSampleLocationsEXT)(commandBuffer, pSampleLocationsInfo); }
void vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) { ((void (*)(VkPhysicalDevice, VkSampleCountFlagBits, VkMultisamplePropertiesEXT*)) TOS_LINK_vkGetPhysicalDeviceMultisamplePropertiesEXT)(physicalDevice, samples, pMultisampleProperties); }
VkResult vkGetImageDrmFormatModifierPropertiesEXT(VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties) { return ((VkResult (*)(VkDevice, VkImage, VkImageDrmFormatModifierPropertiesEXT*)) TOS_LINK_vkGetImageDrmFormatModifierPropertiesEXT)(device, image, pProperties); }
VkResult vkCreateValidationCacheEXT(VkDevice device, const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkValidationCacheEXT* pValidationCache) { return ((VkResult (*)(VkDevice, const VkValidationCacheCreateInfoEXT*, const VkAllocationCallbacks*, VkValidationCacheEXT*)) TOS_LINK_vkCreateValidationCacheEXT)(device, pCreateInfo, pAllocator, pValidationCache); }
void vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkValidationCacheEXT, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyValidationCacheEXT)(device, validationCache, pAllocator); }
VkResult vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT* pSrcCaches) { return ((VkResult (*)(VkDevice, VkValidationCacheEXT, uint32_t, const VkValidationCacheEXT*)) TOS_LINK_vkMergeValidationCachesEXT)(device, dstCache, srcCacheCount, pSrcCaches); }
VkResult vkGetValidationCacheDataEXT(VkDevice device, VkValidationCacheEXT validationCache, size_t* pDataSize, void* pData) { return ((VkResult (*)(VkDevice, VkValidationCacheEXT, size_t*, void*)) TOS_LINK_vkGetValidationCacheDataEXT)(device, validationCache, pDataSize, pData); }
void vkCmdBindShadingRateImageNV(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) { ((void (*)(VkCommandBuffer, VkImageView, VkImageLayout)) TOS_LINK_vkCmdBindShadingRateImageNV)(commandBuffer, imageView, imageLayout); }
void vkCmdSetViewportShadingRatePaletteNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV* pShadingRatePalettes) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkShadingRatePaletteNV*)) TOS_LINK_vkCmdSetViewportShadingRatePaletteNV)(commandBuffer, firstViewport, viewportCount, pShadingRatePalettes); }
void vkCmdSetCoarseSampleOrderNV(VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV* pCustomSampleOrders) { ((void (*)(VkCommandBuffer, VkCoarseSampleOrderTypeNV, uint32_t, const VkCoarseSampleOrderCustomNV*)) TOS_LINK_vkCmdSetCoarseSampleOrderNV)(commandBuffer, sampleOrderType, customSampleOrderCount, pCustomSampleOrders); }
VkResult vkCreateAccelerationStructureNV(VkDevice device, const VkAccelerationStructureCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureNV* pAccelerationStructure) { return ((VkResult (*)(VkDevice, const VkAccelerationStructureCreateInfoNV*, const VkAllocationCallbacks*, VkAccelerationStructureNV*)) TOS_LINK_vkCreateAccelerationStructureNV)(device, pCreateInfo, pAllocator, pAccelerationStructure); }
void vkDestroyAccelerationStructureNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkAccelerationStructureNV, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyAccelerationStructureNV)(device, accelerationStructure, pAllocator); }
void vkGetAccelerationStructureMemoryRequirementsNV(VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2KHR* pMemoryRequirements) { ((void (*)(VkDevice, const VkAccelerationStructureMemoryRequirementsInfoNV*, VkMemoryRequirements2KHR*)) TOS_LINK_vkGetAccelerationStructureMemoryRequirementsNV)(device, pInfo, pMemoryRequirements); }
VkResult vkBindAccelerationStructureMemoryNV(VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV* pBindInfos) { return ((VkResult (*)(VkDevice, uint32_t, const VkBindAccelerationStructureMemoryInfoNV*)) TOS_LINK_vkBindAccelerationStructureMemoryNV)(device, bindInfoCount, pBindInfos); }
void vkCmdBuildAccelerationStructureNV(VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV* pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset) { ((void (*)(VkCommandBuffer, const VkAccelerationStructureInfoNV*, VkBuffer, VkDeviceSize, VkBool32, VkAccelerationStructureNV, VkAccelerationStructureNV, VkBuffer, VkDeviceSize)) TOS_LINK_vkCmdBuildAccelerationStructureNV)(commandBuffer, pInfo, instanceData, instanceOffset, update, dst, src, scratch, scratchOffset); }
void vkCmdCopyAccelerationStructureNV(VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode) { ((void (*)(VkCommandBuffer, VkAccelerationStructureNV, VkAccelerationStructureNV, VkCopyAccelerationStructureModeKHR)) TOS_LINK_vkCmdCopyAccelerationStructureNV)(commandBuffer, dst, src, mode); }
void vkCmdTraceRaysNV(VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, VkDeviceSize, VkBuffer, VkDeviceSize, VkDeviceSize, VkBuffer, VkDeviceSize, VkDeviceSize, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdTraceRaysNV)(commandBuffer, raygenShaderBindingTableBuffer, raygenShaderBindingOffset, missShaderBindingTableBuffer, missShaderBindingOffset, missShaderBindingStride, hitShaderBindingTableBuffer, hitShaderBindingOffset, hitShaderBindingStride, callableShaderBindingTableBuffer, callableShaderBindingOffset, callableShaderBindingStride, width, height, depth); }
VkResult vkCreateRayTracingPipelinesNV(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) { return ((VkResult (*)(VkDevice, VkPipelineCache, uint32_t, const VkRayTracingPipelineCreateInfoNV*, const VkAllocationCallbacks*, VkPipeline*)) TOS_LINK_vkCreateRayTracingPipelinesNV)(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); }
VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) { return ((VkResult (*)(VkDevice, VkPipeline, uint32_t, uint32_t, size_t, void*)) TOS_LINK_vkGetRayTracingShaderGroupHandlesKHR)(device, pipeline, firstGroup, groupCount, dataSize, pData); }
VkResult vkGetRayTracingShaderGroupHandlesNV(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) { return ((VkResult (*)(VkDevice, VkPipeline, uint32_t, uint32_t, size_t, void*)) TOS_LINK_vkGetRayTracingShaderGroupHandlesNV)(device, pipeline, firstGroup, groupCount, dataSize, pData); }
VkResult vkGetAccelerationStructureHandleNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, size_t dataSize, void* pData) { return ((VkResult (*)(VkDevice, VkAccelerationStructureNV, size_t, void*)) TOS_LINK_vkGetAccelerationStructureHandleNV)(device, accelerationStructure, dataSize, pData); }
void vkCmdWriteAccelerationStructuresPropertiesNV(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) { ((void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureNV*, VkQueryType, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesNV)(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery); }
VkResult vkCompileDeferredNV(VkDevice device, VkPipeline pipeline, uint32_t shader) { return ((VkResult (*)(VkDevice, VkPipeline, uint32_t)) TOS_LINK_vkCompileDeferredNV)(device, pipeline, shader); }
VkResult vkGetMemoryHostPointerPropertiesEXT(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties) { return ((VkResult (*)(VkDevice, VkExternalMemoryHandleTypeFlagBits, const void*, VkMemoryHostPointerPropertiesEXT*)) TOS_LINK_vkGetMemoryHostPointerPropertiesEXT)(device, handleType, pHostPointer, pMemoryHostPointerProperties); }
void vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) { ((void (*)(VkCommandBuffer, VkPipelineStageFlagBits, VkBuffer, VkDeviceSize, uint32_t)) TOS_LINK_vkCmdWriteBufferMarkerAMD)(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker); }
VkResult vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainEXT* pTimeDomains) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkTimeDomainEXT*)) TOS_LINK_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)(physicalDevice, pTimeDomainCount, pTimeDomains); }
VkResult vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoEXT* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) { return ((VkResult (*)(VkDevice, uint32_t, const VkCalibratedTimestampInfoEXT*, uint64_t*, uint64_t*)) TOS_LINK_vkGetCalibratedTimestampsEXT)(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation); }
void vkCmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksNV)(commandBuffer, taskCount, firstTask); }
void vkCmdDrawMeshTasksIndirectNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksIndirectNV)(commandBuffer, buffer, offset, drawCount, stride); }
void vkCmdDrawMeshTasksIndirectCountNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksIndirectCountNV)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
void vkCmdSetExclusiveScissorEnableNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkBool32* pExclusiveScissorEnables) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBool32*)) TOS_LINK_vkCmdSetExclusiveScissorEnableNV)(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissorEnables); }
void vkCmdSetExclusiveScissorNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D* pExclusiveScissors) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*)) TOS_LINK_vkCmdSetExclusiveScissorNV)(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissors); }
void vkCmdSetCheckpointNV(VkCommandBuffer commandBuffer, const void* pCheckpointMarker) { ((void (*)(VkCommandBuffer, const void*)) TOS_LINK_vkCmdSetCheckpointNV)(commandBuffer, pCheckpointMarker); }
void vkGetQueueCheckpointDataNV(VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointDataNV* pCheckpointData) { ((void (*)(VkQueue, uint32_t*, VkCheckpointDataNV*)) TOS_LINK_vkGetQueueCheckpointDataNV)(queue, pCheckpointDataCount, pCheckpointData); }
VkResult vkInitializePerformanceApiINTEL(VkDevice device, const VkInitializePerformanceApiInfoINTEL* pInitializeInfo) { return ((VkResult (*)(VkDevice, const VkInitializePerformanceApiInfoINTEL*)) TOS_LINK_vkInitializePerformanceApiINTEL)(device, pInitializeInfo); }
void vkUninitializePerformanceApiINTEL(VkDevice device) { ((void (*)(VkDevice)) TOS_LINK_vkUninitializePerformanceApiINTEL)(device); }
VkResult vkCmdSetPerformanceMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL* pMarkerInfo) { return ((VkResult (*)(VkCommandBuffer, const VkPerformanceMarkerInfoINTEL*)) TOS_LINK_vkCmdSetPerformanceMarkerINTEL)(commandBuffer, pMarkerInfo); }
VkResult vkCmdSetPerformanceStreamMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo) { return ((VkResult (*)(VkCommandBuffer, const VkPerformanceStreamMarkerInfoINTEL*)) TOS_LINK_vkCmdSetPerformanceStreamMarkerINTEL)(commandBuffer, pMarkerInfo); }
VkResult vkCmdSetPerformanceOverrideINTEL(VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL* pOverrideInfo) { return ((VkResult (*)(VkCommandBuffer, const VkPerformanceOverrideInfoINTEL*)) TOS_LINK_vkCmdSetPerformanceOverrideINTEL)(commandBuffer, pOverrideInfo); }
VkResult vkAcquirePerformanceConfigurationINTEL(VkDevice device, const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, VkPerformanceConfigurationINTEL* pConfiguration) { return ((VkResult (*)(VkDevice, const VkPerformanceConfigurationAcquireInfoINTEL*, VkPerformanceConfigurationINTEL*)) TOS_LINK_vkAcquirePerformanceConfigurationINTEL)(device, pAcquireInfo, pConfiguration); }
VkResult vkReleasePerformanceConfigurationINTEL(VkDevice device, VkPerformanceConfigurationINTEL configuration) { return ((VkResult (*)(VkDevice, VkPerformanceConfigurationINTEL)) TOS_LINK_vkReleasePerformanceConfigurationINTEL)(device, configuration); }
VkResult vkQueueSetPerformanceConfigurationINTEL(VkQueue queue, VkPerformanceConfigurationINTEL configuration) { return ((VkResult (*)(VkQueue, VkPerformanceConfigurationINTEL)) TOS_LINK_vkQueueSetPerformanceConfigurationINTEL)(queue, configuration); }
VkResult vkGetPerformanceParameterINTEL(VkDevice device, VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL* pValue) { return ((VkResult (*)(VkDevice, VkPerformanceParameterTypeINTEL, VkPerformanceValueINTEL*)) TOS_LINK_vkGetPerformanceParameterINTEL)(device, parameter, pValue); }
void vkSetLocalDimmingAMD(VkDevice device, VkSwapchainKHR swapChain, VkBool32 localDimmingEnable) { ((void (*)(VkDevice, VkSwapchainKHR, VkBool32)) TOS_LINK_vkSetLocalDimmingAMD)(device, swapChain, localDimmingEnable); }
VkDeviceAddress vkGetBufferDeviceAddressEXT(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) { return ((VkDeviceAddress (*)(VkDevice, const VkBufferDeviceAddressInfo*)) TOS_LINK_vkGetBufferDeviceAddressEXT)(device, pInfo); }
VkResult vkGetPhysicalDeviceToolPropertiesEXT(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkPhysicalDeviceToolProperties*)) TOS_LINK_vkGetPhysicalDeviceToolPropertiesEXT)(physicalDevice, pToolCount, pToolProperties); }
VkResult vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkCooperativeMatrixPropertiesNV*)) TOS_LINK_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)(physicalDevice, pPropertyCount, pProperties); }
VkResult vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(VkPhysicalDevice physicalDevice, uint32_t* pCombinationCount, VkFramebufferMixedSamplesCombinationNV* pCombinations) { return ((VkResult (*)(VkPhysicalDevice, uint32_t*, VkFramebufferMixedSamplesCombinationNV*)) TOS_LINK_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)(physicalDevice, pCombinationCount, pCombinations); }
VkResult vkCreateHeadlessSurfaceEXT(VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) { return ((VkResult (*)(VkInstance, const VkHeadlessSurfaceCreateInfoEXT*, const VkAllocationCallbacks*, VkSurfaceKHR*)) TOS_LINK_vkCreateHeadlessSurfaceEXT)(instance, pCreateInfo, pAllocator, pSurface); }
void vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) { ((void (*)(VkCommandBuffer, uint32_t, uint16_t)) TOS_LINK_vkCmdSetLineStippleEXT)(commandBuffer, lineStippleFactor, lineStipplePattern); }
void vkResetQueryPoolEXT(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) { ((void (*)(VkDevice, VkQueryPool, uint32_t, uint32_t)) TOS_LINK_vkResetQueryPoolEXT)(device, queryPool, firstQuery, queryCount); }
void vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) { ((void (*)(VkCommandBuffer, VkCullModeFlags)) TOS_LINK_vkCmdSetCullModeEXT)(commandBuffer, cullMode); }
void vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) { ((void (*)(VkCommandBuffer, VkFrontFace)) TOS_LINK_vkCmdSetFrontFaceEXT)(commandBuffer, frontFace); }
void vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) { ((void (*)(VkCommandBuffer, VkPrimitiveTopology)) TOS_LINK_vkCmdSetPrimitiveTopologyEXT)(commandBuffer, primitiveTopology); }
void vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) { ((void (*)(VkCommandBuffer, uint32_t, const VkViewport*)) TOS_LINK_vkCmdSetViewportWithCountEXT)(commandBuffer, viewportCount, pViewports); }
void vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) { ((void (*)(VkCommandBuffer, uint32_t, const VkRect2D*)) TOS_LINK_vkCmdSetScissorWithCountEXT)(commandBuffer, scissorCount, pScissors); }
void vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*)) TOS_LINK_vkCmdBindVertexBuffers2EXT)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides); }
void vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthTestEnableEXT)(commandBuffer, depthTestEnable); }
void vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthWriteEnableEXT)(commandBuffer, depthWriteEnable); }
void vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) { ((void (*)(VkCommandBuffer, VkCompareOp)) TOS_LINK_vkCmdSetDepthCompareOpEXT)(commandBuffer, depthCompareOp); }
void vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthBoundsTestEnableEXT)(commandBuffer, depthBoundsTestEnable); }
void vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetStencilTestEnableEXT)(commandBuffer, stencilTestEnable); }
void vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) { ((void (*)(VkCommandBuffer, VkStencilFaceFlags, VkStencilOp, VkStencilOp, VkStencilOp, VkCompareOp)) TOS_LINK_vkCmdSetStencilOpEXT)(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp); }
VkResult vkReleaseSwapchainImagesEXT(VkDevice device, const VkReleaseSwapchainImagesInfoEXT* pReleaseInfo) { return ((VkResult (*)(VkDevice, const VkReleaseSwapchainImagesInfoEXT*)) TOS_LINK_vkReleaseSwapchainImagesEXT)(device, pReleaseInfo); }
void vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements) { ((void (*)(VkDevice, const VkGeneratedCommandsMemoryRequirementsInfoNV*, VkMemoryRequirements2*)) TOS_LINK_vkGetGeneratedCommandsMemoryRequirementsNV)(device, pInfo, pMemoryRequirements); }
void vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) { ((void (*)(VkCommandBuffer, const VkGeneratedCommandsInfoNV*)) TOS_LINK_vkCmdPreprocessGeneratedCommandsNV)(commandBuffer, pGeneratedCommandsInfo); }
void vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) { ((void (*)(VkCommandBuffer, VkBool32, const VkGeneratedCommandsInfoNV*)) TOS_LINK_vkCmdExecuteGeneratedCommandsNV)(commandBuffer, isPreprocessed, pGeneratedCommandsInfo); }
void vkCmdBindPipelineShaderGroupNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline, uint32_t)) TOS_LINK_vkCmdBindPipelineShaderGroupNV)(commandBuffer, pipelineBindPoint, pipeline, groupIndex); }
VkResult vkCreateIndirectCommandsLayoutNV(VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectCommandsLayoutNV* pIndirectCommandsLayout) { return ((VkResult (*)(VkDevice, const VkIndirectCommandsLayoutCreateInfoNV*, const VkAllocationCallbacks*, VkIndirectCommandsLayoutNV*)) TOS_LINK_vkCreateIndirectCommandsLayoutNV)(device, pCreateInfo, pAllocator, pIndirectCommandsLayout); }
void vkDestroyIndirectCommandsLayoutNV(VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkIndirectCommandsLayoutNV, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyIndirectCommandsLayoutNV)(device, indirectCommandsLayout, pAllocator); }
VkResult vkAcquireDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd, VkDisplayKHR display) { return ((VkResult (*)(VkPhysicalDevice, int32_t, VkDisplayKHR)) TOS_LINK_vkAcquireDrmDisplayEXT)(physicalDevice, drmFd, display); }
VkResult vkGetDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd, uint32_t connectorId, VkDisplayKHR* display) { return ((VkResult (*)(VkPhysicalDevice, int32_t, uint32_t, VkDisplayKHR*)) TOS_LINK_vkGetDrmDisplayEXT)(physicalDevice, drmFd, connectorId, display); }
VkResult vkCreatePrivateDataSlotEXT(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) { return ((VkResult (*)(VkDevice, const VkPrivateDataSlotCreateInfo*, const VkAllocationCallbacks*, VkPrivateDataSlot*)) TOS_LINK_vkCreatePrivateDataSlotEXT)(device, pCreateInfo, pAllocator, pPrivateDataSlot); }
void vkDestroyPrivateDataSlotEXT(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkPrivateDataSlot, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyPrivateDataSlotEXT)(device, privateDataSlot, pAllocator); }
VkResult vkSetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) { return ((VkResult (*)(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t)) TOS_LINK_vkSetPrivateDataEXT)(device, objectType, objectHandle, privateDataSlot, data); }
void vkGetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) { ((void (*)(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t*)) TOS_LINK_vkGetPrivateDataEXT)(device, objectType, objectHandle, privateDataSlot, pData); }
void vkGetDescriptorSetLayoutSizeEXT(VkDevice device, VkDescriptorSetLayout layout, VkDeviceSize* pLayoutSizeInBytes) { ((void (*)(VkDevice, VkDescriptorSetLayout, VkDeviceSize*)) TOS_LINK_vkGetDescriptorSetLayoutSizeEXT)(device, layout, pLayoutSizeInBytes); }
void vkGetDescriptorSetLayoutBindingOffsetEXT(VkDevice device, VkDescriptorSetLayout layout, uint32_t binding, VkDeviceSize* pOffset) { ((void (*)(VkDevice, VkDescriptorSetLayout, uint32_t, VkDeviceSize*)) TOS_LINK_vkGetDescriptorSetLayoutBindingOffsetEXT)(device, layout, binding, pOffset); }
void vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT* pDescriptorInfo, size_t dataSize, void* pDescriptor) { ((void (*)(VkDevice, const VkDescriptorGetInfoEXT*, size_t, void*)) TOS_LINK_vkGetDescriptorEXT)(device, pDescriptorInfo, dataSize, pDescriptor); }
void vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount, const VkDescriptorBufferBindingInfoEXT* pBindingInfos) { ((void (*)(VkCommandBuffer, uint32_t, const VkDescriptorBufferBindingInfoEXT*)) TOS_LINK_vkCmdBindDescriptorBuffersEXT)(commandBuffer, bufferCount, pBindingInfos); }
void vkCmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const uint32_t* pBufferIndices, const VkDeviceSize* pOffsets) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const uint32_t*, const VkDeviceSize*)) TOS_LINK_vkCmdSetDescriptorBufferOffsetsEXT)(commandBuffer, pipelineBindPoint, layout, firstSet, setCount, pBufferIndices, pOffsets); }
void vkCmdBindDescriptorBufferEmbeddedSamplersEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set) { ((void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t)) TOS_LINK_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)(commandBuffer, pipelineBindPoint, layout, set); }
VkResult vkGetBufferOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkBufferCaptureDescriptorDataInfoEXT* pInfo, void* pData) { return ((VkResult (*)(VkDevice, const VkBufferCaptureDescriptorDataInfoEXT*, void*)) TOS_LINK_vkGetBufferOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData); }
VkResult vkGetImageOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageCaptureDescriptorDataInfoEXT* pInfo, void* pData) { return ((VkResult (*)(VkDevice, const VkImageCaptureDescriptorDataInfoEXT*, void*)) TOS_LINK_vkGetImageOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData); }
VkResult vkGetImageViewOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageViewCaptureDescriptorDataInfoEXT* pInfo, void* pData) { return ((VkResult (*)(VkDevice, const VkImageViewCaptureDescriptorDataInfoEXT*, void*)) TOS_LINK_vkGetImageViewOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData); }
VkResult vkGetSamplerOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkSamplerCaptureDescriptorDataInfoEXT* pInfo, void* pData) { return ((VkResult (*)(VkDevice, const VkSamplerCaptureDescriptorDataInfoEXT*, void*)) TOS_LINK_vkGetSamplerOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData); }
VkResult vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkAccelerationStructureCaptureDescriptorDataInfoEXT* pInfo, void* pData) { return ((VkResult (*)(VkDevice, const VkAccelerationStructureCaptureDescriptorDataInfoEXT*, void*)) TOS_LINK_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData); }
void vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2EXT* pSubresource, VkSubresourceLayout2EXT* pLayout) { ((void (*)(VkDevice, VkImage, const VkImageSubresource2EXT*, VkSubresourceLayout2EXT*)) TOS_LINK_vkGetImageSubresourceLayout2EXT)(device, image, pSubresource, pLayout); }
VkResult vkGetDeviceFaultInfoEXT(VkDevice device, VkDeviceFaultCountsEXT* pFaultCounts, VkDeviceFaultInfoEXT* pFaultInfo) { return ((VkResult (*)(VkDevice, VkDeviceFaultCountsEXT*, VkDeviceFaultInfoEXT*)) TOS_LINK_vkGetDeviceFaultInfoEXT)(device, pFaultCounts, pFaultInfo); }
void vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) { ((void (*)(VkCommandBuffer, uint32_t, const VkVertexInputBindingDescription2EXT*, uint32_t, const VkVertexInputAttributeDescription2EXT*)) TOS_LINK_vkCmdSetVertexInputEXT)(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions); }
VkResult vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI(VkDevice device, VkRenderPass renderpass, VkExtent2D* pMaxWorkgroupSize) { return ((VkResult (*)(VkDevice, VkRenderPass, VkExtent2D*)) TOS_LINK_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI)(device, renderpass, pMaxWorkgroupSize); }
void vkCmdSubpassShadingHUAWEI(VkCommandBuffer commandBuffer) { ((void (*)(VkCommandBuffer)) TOS_LINK_vkCmdSubpassShadingHUAWEI)(commandBuffer); }
void vkCmdBindInvocationMaskHUAWEI(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) { ((void (*)(VkCommandBuffer, VkImageView, VkImageLayout)) TOS_LINK_vkCmdBindInvocationMaskHUAWEI)(commandBuffer, imageView, imageLayout); }
VkResult vkGetMemoryRemoteAddressNV(VkDevice device, const VkMemoryGetRemoteAddressInfoNV* pMemoryGetRemoteAddressInfo, VkRemoteAddressNV* pAddress) { return ((VkResult (*)(VkDevice, const VkMemoryGetRemoteAddressInfoNV*, VkRemoteAddressNV*)) TOS_LINK_vkGetMemoryRemoteAddressNV)(device, pMemoryGetRemoteAddressInfo, pAddress); }
VkResult vkGetPipelinePropertiesEXT(VkDevice device, const VkPipelineInfoEXT* pPipelineInfo, VkBaseOutStructure* pPipelineProperties) { return ((VkResult (*)(VkDevice, const VkPipelineInfoEXT*, VkBaseOutStructure*)) TOS_LINK_vkGetPipelinePropertiesEXT)(device, pPipelineInfo, pPipelineProperties); }
void vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetPatchControlPointsEXT)(commandBuffer, patchControlPoints); }
void vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetRasterizerDiscardEnableEXT)(commandBuffer, rasterizerDiscardEnable); }
void vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthBiasEnableEXT)(commandBuffer, depthBiasEnable); }
void vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) { ((void (*)(VkCommandBuffer, VkLogicOp)) TOS_LINK_vkCmdSetLogicOpEXT)(commandBuffer, logicOp); }
void vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetPrimitiveRestartEnableEXT)(commandBuffer, primitiveRestartEnable); }
void vkCmdDrawMultiEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride) { ((void (*)(VkCommandBuffer, uint32_t, const VkMultiDrawInfoEXT*, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMultiEXT)(commandBuffer, drawCount, pVertexInfo, instanceCount, firstInstance, stride); }
void vkCmdDrawMultiIndexedEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t* pVertexOffset) { ((void (*)(VkCommandBuffer, uint32_t, const VkMultiDrawIndexedInfoEXT*, uint32_t, uint32_t, uint32_t, const int32_t*)) TOS_LINK_vkCmdDrawMultiIndexedEXT)(commandBuffer, drawCount, pIndexInfo, instanceCount, firstInstance, stride, pVertexOffset); }
VkResult vkCreateMicromapEXT(VkDevice device, const VkMicromapCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkMicromapEXT* pMicromap) { return ((VkResult (*)(VkDevice, const VkMicromapCreateInfoEXT*, const VkAllocationCallbacks*, VkMicromapEXT*)) TOS_LINK_vkCreateMicromapEXT)(device, pCreateInfo, pAllocator, pMicromap); }
void vkDestroyMicromapEXT(VkDevice device, VkMicromapEXT micromap, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkMicromapEXT, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyMicromapEXT)(device, micromap, pAllocator); }
void vkCmdBuildMicromapsEXT(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkMicromapBuildInfoEXT* pInfos) { ((void (*)(VkCommandBuffer, uint32_t, const VkMicromapBuildInfoEXT*)) TOS_LINK_vkCmdBuildMicromapsEXT)(commandBuffer, infoCount, pInfos); }
VkResult vkBuildMicromapsEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkMicromapBuildInfoEXT* pInfos) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, uint32_t, const VkMicromapBuildInfoEXT*)) TOS_LINK_vkBuildMicromapsEXT)(device, deferredOperation, infoCount, pInfos); }
VkResult vkCopyMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapInfoEXT* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyMicromapInfoEXT*)) TOS_LINK_vkCopyMicromapEXT)(device, deferredOperation, pInfo); }
VkResult vkCopyMicromapToMemoryEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapToMemoryInfoEXT* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyMicromapToMemoryInfoEXT*)) TOS_LINK_vkCopyMicromapToMemoryEXT)(device, deferredOperation, pInfo); }
VkResult vkCopyMemoryToMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToMicromapInfoEXT* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyMemoryToMicromapInfoEXT*)) TOS_LINK_vkCopyMemoryToMicromapEXT)(device, deferredOperation, pInfo); }
VkResult vkWriteMicromapsPropertiesEXT(VkDevice device, uint32_t micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) { return ((VkResult (*)(VkDevice, uint32_t, const VkMicromapEXT*, VkQueryType, size_t, void*, size_t)) TOS_LINK_vkWriteMicromapsPropertiesEXT)(device, micromapCount, pMicromaps, queryType, dataSize, pData, stride); }
void vkCmdCopyMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyMicromapInfoEXT*)) TOS_LINK_vkCmdCopyMicromapEXT)(commandBuffer, pInfo); }
void vkCmdCopyMicromapToMemoryEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapToMemoryInfoEXT* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyMicromapToMemoryInfoEXT*)) TOS_LINK_vkCmdCopyMicromapToMemoryEXT)(commandBuffer, pInfo); }
void vkCmdCopyMemoryToMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMemoryToMicromapInfoEXT* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyMemoryToMicromapInfoEXT*)) TOS_LINK_vkCmdCopyMemoryToMicromapEXT)(commandBuffer, pInfo); }
void vkCmdWriteMicromapsPropertiesEXT(VkCommandBuffer commandBuffer, uint32_t micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) { ((void (*)(VkCommandBuffer, uint32_t, const VkMicromapEXT*, VkQueryType, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteMicromapsPropertiesEXT)(commandBuffer, micromapCount, pMicromaps, queryType, queryPool, firstQuery); }
void vkGetDeviceMicromapCompatibilityEXT(VkDevice device, const VkMicromapVersionInfoEXT* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) { ((void (*)(VkDevice, const VkMicromapVersionInfoEXT*, VkAccelerationStructureCompatibilityKHR*)) TOS_LINK_vkGetDeviceMicromapCompatibilityEXT)(device, pVersionInfo, pCompatibility); }
void vkGetMicromapBuildSizesEXT(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkMicromapBuildInfoEXT* pBuildInfo, VkMicromapBuildSizesInfoEXT* pSizeInfo) { ((void (*)(VkDevice, VkAccelerationStructureBuildTypeKHR, const VkMicromapBuildInfoEXT*, VkMicromapBuildSizesInfoEXT*)) TOS_LINK_vkGetMicromapBuildSizesEXT)(device, buildType, pBuildInfo, pSizeInfo); }
void vkCmdDrawClusterHUAWEI(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawClusterHUAWEI)(commandBuffer, groupCountX, groupCountY, groupCountZ); }
void vkCmdDrawClusterIndirectHUAWEI(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize)) TOS_LINK_vkCmdDrawClusterIndirectHUAWEI)(commandBuffer, buffer, offset); }
void vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority) { ((void (*)(VkDevice, VkDeviceMemory, float)) TOS_LINK_vkSetDeviceMemoryPriorityEXT)(device, memory, priority); }
void vkGetDescriptorSetLayoutHostMappingInfoVALVE(VkDevice device, const VkDescriptorSetBindingReferenceVALVE* pBindingReference, VkDescriptorSetLayoutHostMappingInfoVALVE* pHostMapping) { ((void (*)(VkDevice, const VkDescriptorSetBindingReferenceVALVE*, VkDescriptorSetLayoutHostMappingInfoVALVE*)) TOS_LINK_vkGetDescriptorSetLayoutHostMappingInfoVALVE)(device, pBindingReference, pHostMapping); }
void vkGetDescriptorSetHostMappingVALVE(VkDevice device, VkDescriptorSet descriptorSet, void** ppData) { ((void (*)(VkDevice, VkDescriptorSet, void**)) TOS_LINK_vkGetDescriptorSetHostMappingVALVE)(device, descriptorSet, ppData); }
void vkCmdCopyMemoryIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkDeviceAddress, uint32_t, uint32_t)) TOS_LINK_vkCmdCopyMemoryIndirectNV)(commandBuffer, copyBufferAddress, copyCount, stride); }
void vkCmdCopyMemoryToImageIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride, VkImage dstImage, VkImageLayout dstImageLayout, const VkImageSubresourceLayers* pImageSubresources) { ((void (*)(VkCommandBuffer, VkDeviceAddress, uint32_t, uint32_t, VkImage, VkImageLayout, const VkImageSubresourceLayers*)) TOS_LINK_vkCmdCopyMemoryToImageIndirectNV)(commandBuffer, copyBufferAddress, copyCount, stride, dstImage, dstImageLayout, pImageSubresources); }
void vkCmdDecompressMemoryNV(VkCommandBuffer commandBuffer, uint32_t decompressRegionCount, const VkDecompressMemoryRegionNV* pDecompressMemoryRegions) { ((void (*)(VkCommandBuffer, uint32_t, const VkDecompressMemoryRegionNV*)) TOS_LINK_vkCmdDecompressMemoryNV)(commandBuffer, decompressRegionCount, pDecompressMemoryRegions); }
void vkCmdDecompressMemoryIndirectCountNV(VkCommandBuffer commandBuffer, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t stride) { ((void (*)(VkCommandBuffer, VkDeviceAddress, VkDeviceAddress, uint32_t)) TOS_LINK_vkCmdDecompressMemoryIndirectCountNV)(commandBuffer, indirectCommandsAddress, indirectCommandsCountAddress, stride); }
void vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) { ((void (*)(VkCommandBuffer, VkTessellationDomainOrigin)) TOS_LINK_vkCmdSetTessellationDomainOriginEXT)(commandBuffer, domainOrigin); }
void vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthClampEnableEXT)(commandBuffer, depthClampEnable); }
void vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) { ((void (*)(VkCommandBuffer, VkPolygonMode)) TOS_LINK_vkCmdSetPolygonModeEXT)(commandBuffer, polygonMode); }
void vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) { ((void (*)(VkCommandBuffer, VkSampleCountFlagBits)) TOS_LINK_vkCmdSetRasterizationSamplesEXT)(commandBuffer, rasterizationSamples); }
void vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) { ((void (*)(VkCommandBuffer, VkSampleCountFlagBits, const VkSampleMask*)) TOS_LINK_vkCmdSetSampleMaskEXT)(commandBuffer, samples, pSampleMask); }
void vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetAlphaToCoverageEnableEXT)(commandBuffer, alphaToCoverageEnable); }
void vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetAlphaToOneEnableEXT)(commandBuffer, alphaToOneEnable); }
void vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetLogicOpEnableEXT)(commandBuffer, logicOpEnable); }
void vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBool32*)) TOS_LINK_vkCmdSetColorBlendEnableEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables); }
void vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkColorBlendEquationEXT*)) TOS_LINK_vkCmdSetColorBlendEquationEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations); }
void vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkColorComponentFlags*)) TOS_LINK_vkCmdSetColorWriteMaskEXT)(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks); }
void vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetRasterizationStreamEXT)(commandBuffer, rasterizationStream); }
void vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) { ((void (*)(VkCommandBuffer, VkConservativeRasterizationModeEXT)) TOS_LINK_vkCmdSetConservativeRasterizationModeEXT)(commandBuffer, conservativeRasterizationMode); }
void vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) { ((void (*)(VkCommandBuffer, float)) TOS_LINK_vkCmdSetExtraPrimitiveOverestimationSizeEXT)(commandBuffer, extraPrimitiveOverestimationSize); }
void vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthClipEnableEXT)(commandBuffer, depthClipEnable); }
void vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetSampleLocationsEnableEXT)(commandBuffer, sampleLocationsEnable); }
void vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkColorBlendAdvancedEXT*)) TOS_LINK_vkCmdSetColorBlendAdvancedEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced); }
void vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) { ((void (*)(VkCommandBuffer, VkProvokingVertexModeEXT)) TOS_LINK_vkCmdSetProvokingVertexModeEXT)(commandBuffer, provokingVertexMode); }
void vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) { ((void (*)(VkCommandBuffer, VkLineRasterizationModeEXT)) TOS_LINK_vkCmdSetLineRasterizationModeEXT)(commandBuffer, lineRasterizationMode); }
void vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetLineStippleEnableEXT)(commandBuffer, stippledLineEnable); }
void vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetDepthClipNegativeOneToOneEXT)(commandBuffer, negativeOneToOne); }
void vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetViewportWScalingEnableNV)(commandBuffer, viewportWScalingEnable); }
void vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkViewportSwizzleNV*)) TOS_LINK_vkCmdSetViewportSwizzleNV)(commandBuffer, firstViewport, viewportCount, pViewportSwizzles); }
void vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetCoverageToColorEnableNV)(commandBuffer, coverageToColorEnable); }
void vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetCoverageToColorLocationNV)(commandBuffer, coverageToColorLocation); }
void vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) { ((void (*)(VkCommandBuffer, VkCoverageModulationModeNV)) TOS_LINK_vkCmdSetCoverageModulationModeNV)(commandBuffer, coverageModulationMode); }
void vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetCoverageModulationTableEnableNV)(commandBuffer, coverageModulationTableEnable); }
void vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) { ((void (*)(VkCommandBuffer, uint32_t, const float*)) TOS_LINK_vkCmdSetCoverageModulationTableNV)(commandBuffer, coverageModulationTableCount, pCoverageModulationTable); }
void vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetShadingRateImageEnableNV)(commandBuffer, shadingRateImageEnable); }
void vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) { ((void (*)(VkCommandBuffer, VkBool32)) TOS_LINK_vkCmdSetRepresentativeFragmentTestEnableNV)(commandBuffer, representativeFragmentTestEnable); }
void vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) { ((void (*)(VkCommandBuffer, VkCoverageReductionModeNV)) TOS_LINK_vkCmdSetCoverageReductionModeNV)(commandBuffer, coverageReductionMode); }
void vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier) { ((void (*)(VkDevice, VkShaderModule, VkShaderModuleIdentifierEXT*)) TOS_LINK_vkGetShaderModuleIdentifierEXT)(device, shaderModule, pIdentifier); }
void vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier) { ((void (*)(VkDevice, const VkShaderModuleCreateInfo*, VkShaderModuleIdentifierEXT*)) TOS_LINK_vkGetShaderModuleCreateInfoIdentifierEXT)(device, pCreateInfo, pIdentifier); }
VkResult vkGetPhysicalDeviceOpticalFlowImageFormatsNV(VkPhysicalDevice physicalDevice, const VkOpticalFlowImageFormatInfoNV* pOpticalFlowImageFormatInfo, uint32_t* pFormatCount, VkOpticalFlowImageFormatPropertiesNV* pImageFormatProperties) { return ((VkResult (*)(VkPhysicalDevice, const VkOpticalFlowImageFormatInfoNV*, uint32_t*, VkOpticalFlowImageFormatPropertiesNV*)) TOS_LINK_vkGetPhysicalDeviceOpticalFlowImageFormatsNV)(physicalDevice, pOpticalFlowImageFormatInfo, pFormatCount, pImageFormatProperties); }
VkResult vkCreateOpticalFlowSessionNV(VkDevice device, const VkOpticalFlowSessionCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkOpticalFlowSessionNV* pSession) { return ((VkResult (*)(VkDevice, const VkOpticalFlowSessionCreateInfoNV*, const VkAllocationCallbacks*, VkOpticalFlowSessionNV*)) TOS_LINK_vkCreateOpticalFlowSessionNV)(device, pCreateInfo, pAllocator, pSession); }
void vkDestroyOpticalFlowSessionNV(VkDevice device, VkOpticalFlowSessionNV session, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkOpticalFlowSessionNV, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyOpticalFlowSessionNV)(device, session, pAllocator); }
VkResult vkBindOpticalFlowSessionImageNV(VkDevice device, VkOpticalFlowSessionNV session, VkOpticalFlowSessionBindingPointNV bindingPoint, VkImageView view, VkImageLayout layout) { return ((VkResult (*)(VkDevice, VkOpticalFlowSessionNV, VkOpticalFlowSessionBindingPointNV, VkImageView, VkImageLayout)) TOS_LINK_vkBindOpticalFlowSessionImageNV)(device, session, bindingPoint, view, layout); }
void vkCmdOpticalFlowExecuteNV(VkCommandBuffer commandBuffer, VkOpticalFlowSessionNV session, const VkOpticalFlowExecuteInfoNV* pExecuteInfo) { ((void (*)(VkCommandBuffer, VkOpticalFlowSessionNV, const VkOpticalFlowExecuteInfoNV*)) TOS_LINK_vkCmdOpticalFlowExecuteNV)(commandBuffer, session, pExecuteInfo); }
VkResult vkGetFramebufferTilePropertiesQCOM(VkDevice device, VkFramebuffer framebuffer, uint32_t* pPropertiesCount, VkTilePropertiesQCOM* pProperties) { return ((VkResult (*)(VkDevice, VkFramebuffer, uint32_t*, VkTilePropertiesQCOM*)) TOS_LINK_vkGetFramebufferTilePropertiesQCOM)(device, framebuffer, pPropertiesCount, pProperties); }
VkResult vkGetDynamicRenderingTilePropertiesQCOM(VkDevice device, const VkRenderingInfo* pRenderingInfo, VkTilePropertiesQCOM* pProperties) { return ((VkResult (*)(VkDevice, const VkRenderingInfo*, VkTilePropertiesQCOM*)) TOS_LINK_vkGetDynamicRenderingTilePropertiesQCOM)(device, pRenderingInfo, pProperties); }
VkResult vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) { return ((VkResult (*)(VkDevice, const VkAccelerationStructureCreateInfoKHR*, const VkAllocationCallbacks*, VkAccelerationStructureKHR*)) TOS_LINK_vkCreateAccelerationStructureKHR)(device, pCreateInfo, pAllocator, pAccelerationStructure); }
void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) { ((void (*)(VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*)) TOS_LINK_vkDestroyAccelerationStructureKHR)(device, accelerationStructure, pAllocator); }
void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) { ((void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR* const*)) TOS_LINK_vkCmdBuildAccelerationStructuresKHR)(commandBuffer, infoCount, pInfos, ppBuildRangeInfos); }
void vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t* const* ppMaxPrimitiveCounts) { ((void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkDeviceAddress*, const uint32_t*, const uint32_t* const*)) TOS_LINK_vkCmdBuildAccelerationStructuresIndirectKHR)(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts); }
VkResult vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR* const*)) TOS_LINK_vkBuildAccelerationStructuresKHR)(device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos); }
VkResult vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyAccelerationStructureInfoKHR*)) TOS_LINK_vkCopyAccelerationStructureKHR)(device, deferredOperation, pInfo); }
VkResult vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyAccelerationStructureToMemoryInfoKHR*)) TOS_LINK_vkCopyAccelerationStructureToMemoryKHR)(device, deferredOperation, pInfo); }
VkResult vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, const VkCopyMemoryToAccelerationStructureInfoKHR*)) TOS_LINK_vkCopyMemoryToAccelerationStructureKHR)(device, deferredOperation, pInfo); }
VkResult vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) { return ((VkResult (*)(VkDevice, uint32_t, const VkAccelerationStructureKHR*, VkQueryType, size_t, void*, size_t)) TOS_LINK_vkWriteAccelerationStructuresPropertiesKHR)(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride); }
void vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyAccelerationStructureInfoKHR*)) TOS_LINK_vkCmdCopyAccelerationStructureKHR)(commandBuffer, pInfo); }
void vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR*)) TOS_LINK_vkCmdCopyAccelerationStructureToMemoryKHR)(commandBuffer, pInfo); }
void vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) { ((void (*)(VkCommandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR*)) TOS_LINK_vkCmdCopyMemoryToAccelerationStructureKHR)(commandBuffer, pInfo); }
VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) { return ((VkDeviceAddress (*)(VkDevice, const VkAccelerationStructureDeviceAddressInfoKHR*)) TOS_LINK_vkGetAccelerationStructureDeviceAddressKHR)(device, pInfo); }
void vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) { ((void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureKHR*, VkQueryType, VkQueryPool, uint32_t)) TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesKHR)(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery); }
void vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) { ((void (*)(VkDevice, const VkAccelerationStructureVersionInfoKHR*, VkAccelerationStructureCompatibilityKHR*)) TOS_LINK_vkGetDeviceAccelerationStructureCompatibilityKHR)(device, pVersionInfo, pCompatibility); }
void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) { ((void (*)(VkDevice, VkAccelerationStructureBuildTypeKHR, const VkAccelerationStructureBuildGeometryInfoKHR*, const uint32_t*, VkAccelerationStructureBuildSizesInfoKHR*)) TOS_LINK_vkGetAccelerationStructureBuildSizesKHR)(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo); }
void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) { ((void (*)(VkCommandBuffer, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdTraceRaysKHR)(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth); }
VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) { return ((VkResult (*)(VkDevice, VkDeferredOperationKHR, VkPipelineCache, uint32_t, const VkRayTracingPipelineCreateInfoKHR*, const VkAllocationCallbacks*, VkPipeline*)) TOS_LINK_vkCreateRayTracingPipelinesKHR)(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); }
VkResult vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) { return ((VkResult (*)(VkDevice, VkPipeline, uint32_t, uint32_t, size_t, void*)) TOS_LINK_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)(device, pipeline, firstGroup, groupCount, dataSize, pData); }
void vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) { ((void (*)(VkCommandBuffer, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, VkDeviceAddress)) TOS_LINK_vkCmdTraceRaysIndirectKHR)(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress); }
VkDeviceSize vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) { return ((VkDeviceSize (*)(VkDevice, VkPipeline, uint32_t, VkShaderGroupShaderKHR)) TOS_LINK_vkGetRayTracingShaderGroupStackSizeKHR)(device, pipeline, group, groupShader); }
void vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) { ((void (*)(VkCommandBuffer, uint32_t)) TOS_LINK_vkCmdSetRayTracingPipelineStackSizeKHR)(commandBuffer, pipelineStackSize); }
void vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { ((void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksEXT)(commandBuffer, groupCountX, groupCountY, groupCountZ); }
void vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksIndirectEXT)(commandBuffer, buffer, offset, drawCount, stride); }
void vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { ((void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t)) TOS_LINK_vkCmdDrawMeshTasksIndirectCountEXT)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); }
}

#include <TOSLib.hpp>

void LoadSymbolsVulkan(TOS::DynamicLibrary &library)
{
	void *sym;
	sym = (void*) library.takeSymbol("vkCreateInstance");
	TOS_LINK_vkCreateInstance = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyInstance");
	TOS_LINK_vkDestroyInstance = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumeratePhysicalDevices");
	TOS_LINK_vkEnumeratePhysicalDevices = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFeatures");
	TOS_LINK_vkGetPhysicalDeviceFeatures = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFormatProperties");
	TOS_LINK_vkGetPhysicalDeviceFormatProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceImageFormatProperties");
	TOS_LINK_vkGetPhysicalDeviceImageFormatProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceProperties");
	TOS_LINK_vkGetPhysicalDeviceProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceQueueFamilyProperties");
	TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceMemoryProperties");
	TOS_LINK_vkGetPhysicalDeviceMemoryProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetInstanceProcAddr");
	TOS_LINK_vkGetInstanceProcAddr = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceProcAddr");
	TOS_LINK_vkGetDeviceProcAddr = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDevice");
	TOS_LINK_vkCreateDevice = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDevice");
	TOS_LINK_vkDestroyDevice = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumerateInstanceExtensionProperties");
	TOS_LINK_vkEnumerateInstanceExtensionProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumerateDeviceExtensionProperties");
	TOS_LINK_vkEnumerateDeviceExtensionProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumerateInstanceLayerProperties");
	TOS_LINK_vkEnumerateInstanceLayerProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumerateDeviceLayerProperties");
	TOS_LINK_vkEnumerateDeviceLayerProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceQueue");
	TOS_LINK_vkGetDeviceQueue = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueSubmit");
	TOS_LINK_vkQueueSubmit = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueWaitIdle");
	TOS_LINK_vkQueueWaitIdle = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDeviceWaitIdle");
	TOS_LINK_vkDeviceWaitIdle = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAllocateMemory");
	TOS_LINK_vkAllocateMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkFreeMemory");
	TOS_LINK_vkFreeMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkMapMemory");
	TOS_LINK_vkMapMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUnmapMemory");
	TOS_LINK_vkUnmapMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkFlushMappedMemoryRanges");
	TOS_LINK_vkFlushMappedMemoryRanges = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkInvalidateMappedMemoryRanges");
	TOS_LINK_vkInvalidateMappedMemoryRanges = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceMemoryCommitment");
	TOS_LINK_vkGetDeviceMemoryCommitment = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindBufferMemory");
	TOS_LINK_vkBindBufferMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindImageMemory");
	TOS_LINK_vkBindImageMemory = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferMemoryRequirements");
	TOS_LINK_vkGetBufferMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageMemoryRequirements");
	TOS_LINK_vkGetImageMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageSparseMemoryRequirements");
	TOS_LINK_vkGetImageSparseMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSparseImageFormatProperties");
	TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueBindSparse");
	TOS_LINK_vkQueueBindSparse = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateFence");
	TOS_LINK_vkCreateFence = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyFence");
	TOS_LINK_vkDestroyFence = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetFences");
	TOS_LINK_vkResetFences = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetFenceStatus");
	TOS_LINK_vkGetFenceStatus = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWaitForFences");
	TOS_LINK_vkWaitForFences = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSemaphore");
	TOS_LINK_vkCreateSemaphore = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySemaphore");
	TOS_LINK_vkDestroySemaphore = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateEvent");
	TOS_LINK_vkCreateEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyEvent");
	TOS_LINK_vkDestroyEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetEventStatus");
	TOS_LINK_vkGetEventStatus = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetEvent");
	TOS_LINK_vkSetEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetEvent");
	TOS_LINK_vkResetEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateQueryPool");
	TOS_LINK_vkCreateQueryPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyQueryPool");
	TOS_LINK_vkDestroyQueryPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetQueryPoolResults");
	TOS_LINK_vkGetQueryPoolResults = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateBuffer");
	TOS_LINK_vkCreateBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyBuffer");
	TOS_LINK_vkDestroyBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateBufferView");
	TOS_LINK_vkCreateBufferView = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyBufferView");
	TOS_LINK_vkDestroyBufferView = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateImage");
	TOS_LINK_vkCreateImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyImage");
	TOS_LINK_vkDestroyImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageSubresourceLayout");
	TOS_LINK_vkGetImageSubresourceLayout = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateImageView");
	TOS_LINK_vkCreateImageView = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyImageView");
	TOS_LINK_vkDestroyImageView = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateShaderModule");
	TOS_LINK_vkCreateShaderModule = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyShaderModule");
	TOS_LINK_vkDestroyShaderModule = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreatePipelineCache");
	TOS_LINK_vkCreatePipelineCache = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyPipelineCache");
	TOS_LINK_vkDestroyPipelineCache = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPipelineCacheData");
	TOS_LINK_vkGetPipelineCacheData = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkMergePipelineCaches");
	TOS_LINK_vkMergePipelineCaches = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateGraphicsPipelines");
	TOS_LINK_vkCreateGraphicsPipelines = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateComputePipelines");
	TOS_LINK_vkCreateComputePipelines = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyPipeline");
	TOS_LINK_vkDestroyPipeline = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreatePipelineLayout");
	TOS_LINK_vkCreatePipelineLayout = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyPipelineLayout");
	TOS_LINK_vkDestroyPipelineLayout = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSampler");
	TOS_LINK_vkCreateSampler = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySampler");
	TOS_LINK_vkDestroySampler = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDescriptorSetLayout");
	TOS_LINK_vkCreateDescriptorSetLayout = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDescriptorSetLayout");
	TOS_LINK_vkDestroyDescriptorSetLayout = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDescriptorPool");
	TOS_LINK_vkCreateDescriptorPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDescriptorPool");
	TOS_LINK_vkDestroyDescriptorPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetDescriptorPool");
	TOS_LINK_vkResetDescriptorPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAllocateDescriptorSets");
	TOS_LINK_vkAllocateDescriptorSets = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkFreeDescriptorSets");
	TOS_LINK_vkFreeDescriptorSets = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUpdateDescriptorSets");
	TOS_LINK_vkUpdateDescriptorSets = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateFramebuffer");
	TOS_LINK_vkCreateFramebuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyFramebuffer");
	TOS_LINK_vkDestroyFramebuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateRenderPass");
	TOS_LINK_vkCreateRenderPass = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyRenderPass");
	TOS_LINK_vkDestroyRenderPass = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRenderAreaGranularity");
	TOS_LINK_vkGetRenderAreaGranularity = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateCommandPool");
	TOS_LINK_vkCreateCommandPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyCommandPool");
	TOS_LINK_vkDestroyCommandPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetCommandPool");
	TOS_LINK_vkResetCommandPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAllocateCommandBuffers");
	TOS_LINK_vkAllocateCommandBuffers = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkFreeCommandBuffers");
	TOS_LINK_vkFreeCommandBuffers = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBeginCommandBuffer");
	TOS_LINK_vkBeginCommandBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEndCommandBuffer");
	TOS_LINK_vkEndCommandBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetCommandBuffer");
	TOS_LINK_vkResetCommandBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindPipeline");
	TOS_LINK_vkCmdBindPipeline = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewport");
	TOS_LINK_vkCmdSetViewport = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetScissor");
	TOS_LINK_vkCmdSetScissor = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLineWidth");
	TOS_LINK_vkCmdSetLineWidth = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBias");
	TOS_LINK_vkCmdSetDepthBias = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBounds");
	TOS_LINK_vkCmdSetDepthBounds = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilCompareMask");
	TOS_LINK_vkCmdSetStencilCompareMask = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilWriteMask");
	TOS_LINK_vkCmdSetStencilWriteMask = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilReference");
	TOS_LINK_vkCmdSetStencilReference = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindDescriptorSets");
	TOS_LINK_vkCmdBindDescriptorSets = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindIndexBuffer");
	TOS_LINK_vkCmdBindIndexBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindVertexBuffers");
	TOS_LINK_vkCmdBindVertexBuffers = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDraw");
	TOS_LINK_vkCmdDraw = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndexed");
	TOS_LINK_vkCmdDrawIndexed = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndirect");
	TOS_LINK_vkCmdDrawIndirect = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndexedIndirect");
	TOS_LINK_vkCmdDrawIndexedIndirect = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDispatch");
	TOS_LINK_vkCmdDispatch = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDispatchIndirect");
	TOS_LINK_vkCmdDispatchIndirect = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBuffer");
	TOS_LINK_vkCmdCopyBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImage");
	TOS_LINK_vkCmdCopyImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBlitImage");
	TOS_LINK_vkCmdBlitImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBufferToImage");
	TOS_LINK_vkCmdCopyBufferToImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImageToBuffer");
	TOS_LINK_vkCmdCopyImageToBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdUpdateBuffer");
	TOS_LINK_vkCmdUpdateBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdFillBuffer");
	TOS_LINK_vkCmdFillBuffer = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdClearColorImage");
	TOS_LINK_vkCmdClearColorImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdClearDepthStencilImage");
	TOS_LINK_vkCmdClearDepthStencilImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdClearAttachments");
	TOS_LINK_vkCmdClearAttachments = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResolveImage");
	TOS_LINK_vkCmdResolveImage = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetEvent");
	TOS_LINK_vkCmdSetEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResetEvent");
	TOS_LINK_vkCmdResetEvent = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWaitEvents");
	TOS_LINK_vkCmdWaitEvents = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPipelineBarrier");
	TOS_LINK_vkCmdPipelineBarrier = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginQuery");
	TOS_LINK_vkCmdBeginQuery = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndQuery");
	TOS_LINK_vkCmdEndQuery = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResetQueryPool");
	TOS_LINK_vkCmdResetQueryPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteTimestamp");
	TOS_LINK_vkCmdWriteTimestamp = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyQueryPoolResults");
	TOS_LINK_vkCmdCopyQueryPoolResults = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPushConstants");
	TOS_LINK_vkCmdPushConstants = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginRenderPass");
	TOS_LINK_vkCmdBeginRenderPass = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdNextSubpass");
	TOS_LINK_vkCmdNextSubpass = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndRenderPass");
	TOS_LINK_vkCmdEndRenderPass = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdExecuteCommands");
	TOS_LINK_vkCmdExecuteCommands = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumerateInstanceVersion");
	TOS_LINK_vkEnumerateInstanceVersion = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindBufferMemory2");
	TOS_LINK_vkBindBufferMemory2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindImageMemory2");
	TOS_LINK_vkBindImageMemory2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceGroupPeerMemoryFeatures");
	TOS_LINK_vkGetDeviceGroupPeerMemoryFeatures = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDeviceMask");
	TOS_LINK_vkCmdSetDeviceMask = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDispatchBase");
	TOS_LINK_vkCmdDispatchBase = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumeratePhysicalDeviceGroups");
	TOS_LINK_vkEnumeratePhysicalDeviceGroups = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageMemoryRequirements2");
	TOS_LINK_vkGetImageMemoryRequirements2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferMemoryRequirements2");
	TOS_LINK_vkGetBufferMemoryRequirements2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageSparseMemoryRequirements2");
	TOS_LINK_vkGetImageSparseMemoryRequirements2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFeatures2");
	TOS_LINK_vkGetPhysicalDeviceFeatures2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceProperties2");
	TOS_LINK_vkGetPhysicalDeviceProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFormatProperties2");
	TOS_LINK_vkGetPhysicalDeviceFormatProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceImageFormatProperties2");
	TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceQueueFamilyProperties2");
	TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceMemoryProperties2");
	TOS_LINK_vkGetPhysicalDeviceMemoryProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSparseImageFormatProperties2");
	TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkTrimCommandPool");
	TOS_LINK_vkTrimCommandPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceQueue2");
	TOS_LINK_vkGetDeviceQueue2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSamplerYcbcrConversion");
	TOS_LINK_vkCreateSamplerYcbcrConversion = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySamplerYcbcrConversion");
	TOS_LINK_vkDestroySamplerYcbcrConversion = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDescriptorUpdateTemplate");
	TOS_LINK_vkCreateDescriptorUpdateTemplate = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDescriptorUpdateTemplate");
	TOS_LINK_vkDestroyDescriptorUpdateTemplate = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUpdateDescriptorSetWithTemplate");
	TOS_LINK_vkUpdateDescriptorSetWithTemplate = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalBufferProperties");
	TOS_LINK_vkGetPhysicalDeviceExternalBufferProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalFenceProperties");
	TOS_LINK_vkGetPhysicalDeviceExternalFenceProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalSemaphoreProperties");
	TOS_LINK_vkGetPhysicalDeviceExternalSemaphoreProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetLayoutSupport");
	TOS_LINK_vkGetDescriptorSetLayoutSupport = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndirectCount");
	TOS_LINK_vkCmdDrawIndirectCount = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndexedIndirectCount");
	TOS_LINK_vkCmdDrawIndexedIndirectCount = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateRenderPass2");
	TOS_LINK_vkCreateRenderPass2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginRenderPass2");
	TOS_LINK_vkCmdBeginRenderPass2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdNextSubpass2");
	TOS_LINK_vkCmdNextSubpass2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndRenderPass2");
	TOS_LINK_vkCmdEndRenderPass2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetQueryPool");
	TOS_LINK_vkResetQueryPool = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSemaphoreCounterValue");
	TOS_LINK_vkGetSemaphoreCounterValue = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWaitSemaphores");
	TOS_LINK_vkWaitSemaphores = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSignalSemaphore");
	TOS_LINK_vkSignalSemaphore = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferDeviceAddress");
	TOS_LINK_vkGetBufferDeviceAddress = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceToolProperties");
	TOS_LINK_vkGetPhysicalDeviceToolProperties = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreatePrivateDataSlot");
	TOS_LINK_vkCreatePrivateDataSlot = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyPrivateDataSlot");
	TOS_LINK_vkDestroyPrivateDataSlot = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetPrivateData");
	TOS_LINK_vkSetPrivateData = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPrivateData");
	TOS_LINK_vkGetPrivateData = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetEvent2");
	TOS_LINK_vkCmdSetEvent2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResetEvent2");
	TOS_LINK_vkCmdResetEvent2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWaitEvents2");
	TOS_LINK_vkCmdWaitEvents2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPipelineBarrier2");
	TOS_LINK_vkCmdPipelineBarrier2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteTimestamp2");
	TOS_LINK_vkCmdWriteTimestamp2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueSubmit2");
	TOS_LINK_vkQueueSubmit2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBuffer2");
	TOS_LINK_vkCmdCopyBuffer2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImage2");
	TOS_LINK_vkCmdCopyImage2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBufferToImage2");
	TOS_LINK_vkCmdCopyBufferToImage2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImageToBuffer2");
	TOS_LINK_vkCmdCopyImageToBuffer2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBlitImage2");
	TOS_LINK_vkCmdBlitImage2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResolveImage2");
	TOS_LINK_vkCmdResolveImage2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginRendering");
	TOS_LINK_vkCmdBeginRendering = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndRendering");
	TOS_LINK_vkCmdEndRendering = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCullMode");
	TOS_LINK_vkCmdSetCullMode = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetFrontFace");
	TOS_LINK_vkCmdSetFrontFace = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPrimitiveTopology");
	TOS_LINK_vkCmdSetPrimitiveTopology = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportWithCount");
	TOS_LINK_vkCmdSetViewportWithCount = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetScissorWithCount");
	TOS_LINK_vkCmdSetScissorWithCount = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindVertexBuffers2");
	TOS_LINK_vkCmdBindVertexBuffers2 = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthTestEnable");
	TOS_LINK_vkCmdSetDepthTestEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthWriteEnable");
	TOS_LINK_vkCmdSetDepthWriteEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthCompareOp");
	TOS_LINK_vkCmdSetDepthCompareOp = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBoundsTestEnable");
	TOS_LINK_vkCmdSetDepthBoundsTestEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilTestEnable");
	TOS_LINK_vkCmdSetStencilTestEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilOp");
	TOS_LINK_vkCmdSetStencilOp = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRasterizerDiscardEnable");
	TOS_LINK_vkCmdSetRasterizerDiscardEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBiasEnable");
	TOS_LINK_vkCmdSetDepthBiasEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPrimitiveRestartEnable");
	TOS_LINK_vkCmdSetPrimitiveRestartEnable = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceBufferMemoryRequirements");
	TOS_LINK_vkGetDeviceBufferMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceImageMemoryRequirements");
	TOS_LINK_vkGetDeviceImageMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceImageSparseMemoryRequirements");
	TOS_LINK_vkGetDeviceImageSparseMemoryRequirements = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySurfaceKHR");
	TOS_LINK_vkDestroySurfaceKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceSupportKHR");
	TOS_LINK_vkGetPhysicalDeviceSurfaceSupportKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceFormatsKHR");
	TOS_LINK_vkGetPhysicalDeviceSurfaceFormatsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfacePresentModesKHR");
	TOS_LINK_vkGetPhysicalDeviceSurfacePresentModesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSwapchainKHR");
	TOS_LINK_vkCreateSwapchainKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySwapchainKHR");
	TOS_LINK_vkDestroySwapchainKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSwapchainImagesKHR");
	TOS_LINK_vkGetSwapchainImagesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAcquireNextImageKHR");
	TOS_LINK_vkAcquireNextImageKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueuePresentKHR");
	TOS_LINK_vkQueuePresentKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceGroupPresentCapabilitiesKHR");
	TOS_LINK_vkGetDeviceGroupPresentCapabilitiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceGroupSurfacePresentModesKHR");
	TOS_LINK_vkGetDeviceGroupSurfacePresentModesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDevicePresentRectanglesKHR");
	TOS_LINK_vkGetPhysicalDevicePresentRectanglesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAcquireNextImage2KHR");
	TOS_LINK_vkAcquireNextImage2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceDisplayPropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceDisplayPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceDisplayPlanePropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDisplayPlaneSupportedDisplaysKHR");
	TOS_LINK_vkGetDisplayPlaneSupportedDisplaysKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDisplayModePropertiesKHR");
	TOS_LINK_vkGetDisplayModePropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDisplayModeKHR");
	TOS_LINK_vkCreateDisplayModeKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDisplayPlaneCapabilitiesKHR");
	TOS_LINK_vkGetDisplayPlaneCapabilitiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDisplayPlaneSurfaceKHR");
	TOS_LINK_vkCreateDisplayPlaneSurfaceKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSharedSwapchainsKHR");
	TOS_LINK_vkCreateSharedSwapchainsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceVideoCapabilitiesKHR");
	TOS_LINK_vkGetPhysicalDeviceVideoCapabilitiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceVideoFormatPropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceVideoFormatPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateVideoSessionKHR");
	TOS_LINK_vkCreateVideoSessionKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyVideoSessionKHR");
	TOS_LINK_vkDestroyVideoSessionKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetVideoSessionMemoryRequirementsKHR");
	TOS_LINK_vkGetVideoSessionMemoryRequirementsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindVideoSessionMemoryKHR");
	TOS_LINK_vkBindVideoSessionMemoryKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateVideoSessionParametersKHR");
	TOS_LINK_vkCreateVideoSessionParametersKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUpdateVideoSessionParametersKHR");
	TOS_LINK_vkUpdateVideoSessionParametersKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyVideoSessionParametersKHR");
	TOS_LINK_vkDestroyVideoSessionParametersKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginVideoCodingKHR");
	TOS_LINK_vkCmdBeginVideoCodingKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndVideoCodingKHR");
	TOS_LINK_vkCmdEndVideoCodingKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdControlVideoCodingKHR");
	TOS_LINK_vkCmdControlVideoCodingKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDecodeVideoKHR");
	TOS_LINK_vkCmdDecodeVideoKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginRenderingKHR");
	TOS_LINK_vkCmdBeginRenderingKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndRenderingKHR");
	TOS_LINK_vkCmdEndRenderingKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFeatures2KHR");
	TOS_LINK_vkGetPhysicalDeviceFeatures2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFormatProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceFormatProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceImageFormatProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceImageFormatProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceQueueFamilyProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceQueueFamilyProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceMemoryProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceMemoryProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceSparseImageFormatProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceGroupPeerMemoryFeaturesKHR");
	TOS_LINK_vkGetDeviceGroupPeerMemoryFeaturesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDeviceMaskKHR");
	TOS_LINK_vkCmdSetDeviceMaskKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDispatchBaseKHR");
	TOS_LINK_vkCmdDispatchBaseKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkTrimCommandPoolKHR");
	TOS_LINK_vkTrimCommandPoolKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumeratePhysicalDeviceGroupsKHR");
	TOS_LINK_vkEnumeratePhysicalDeviceGroupsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalBufferPropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceExternalBufferPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetMemoryFdKHR");
	TOS_LINK_vkGetMemoryFdKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetMemoryFdPropertiesKHR");
	TOS_LINK_vkGetMemoryFdPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkImportSemaphoreFdKHR");
	TOS_LINK_vkImportSemaphoreFdKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSemaphoreFdKHR");
	TOS_LINK_vkGetSemaphoreFdKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPushDescriptorSetKHR");
	TOS_LINK_vkCmdPushDescriptorSetKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPushDescriptorSetWithTemplateKHR");
	TOS_LINK_vkCmdPushDescriptorSetWithTemplateKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDescriptorUpdateTemplateKHR");
	TOS_LINK_vkCreateDescriptorUpdateTemplateKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDescriptorUpdateTemplateKHR");
	TOS_LINK_vkDestroyDescriptorUpdateTemplateKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUpdateDescriptorSetWithTemplateKHR");
	TOS_LINK_vkUpdateDescriptorSetWithTemplateKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateRenderPass2KHR");
	TOS_LINK_vkCreateRenderPass2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginRenderPass2KHR");
	TOS_LINK_vkCmdBeginRenderPass2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdNextSubpass2KHR");
	TOS_LINK_vkCmdNextSubpass2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndRenderPass2KHR");
	TOS_LINK_vkCmdEndRenderPass2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSwapchainStatusKHR");
	TOS_LINK_vkGetSwapchainStatusKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalFencePropertiesKHR");
	TOS_LINK_vkGetPhysicalDeviceExternalFencePropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkImportFenceFdKHR");
	TOS_LINK_vkImportFenceFdKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetFenceFdKHR");
	TOS_LINK_vkGetFenceFdKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
	TOS_LINK_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
	TOS_LINK_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAcquireProfilingLockKHR");
	TOS_LINK_vkAcquireProfilingLockKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkReleaseProfilingLockKHR");
	TOS_LINK_vkReleaseProfilingLockKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceCapabilities2KHR");
	TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceFormats2KHR");
	TOS_LINK_vkGetPhysicalDeviceSurfaceFormats2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceDisplayProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceDisplayProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceDisplayPlaneProperties2KHR");
	TOS_LINK_vkGetPhysicalDeviceDisplayPlaneProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDisplayModeProperties2KHR");
	TOS_LINK_vkGetDisplayModeProperties2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDisplayPlaneCapabilities2KHR");
	TOS_LINK_vkGetDisplayPlaneCapabilities2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageMemoryRequirements2KHR");
	TOS_LINK_vkGetImageMemoryRequirements2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferMemoryRequirements2KHR");
	TOS_LINK_vkGetBufferMemoryRequirements2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageSparseMemoryRequirements2KHR");
	TOS_LINK_vkGetImageSparseMemoryRequirements2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateSamplerYcbcrConversionKHR");
	TOS_LINK_vkCreateSamplerYcbcrConversionKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroySamplerYcbcrConversionKHR");
	TOS_LINK_vkDestroySamplerYcbcrConversionKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindBufferMemory2KHR");
	TOS_LINK_vkBindBufferMemory2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindImageMemory2KHR");
	TOS_LINK_vkBindImageMemory2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetLayoutSupportKHR");
	TOS_LINK_vkGetDescriptorSetLayoutSupportKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndirectCountKHR");
	TOS_LINK_vkCmdDrawIndirectCountKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndexedIndirectCountKHR");
	TOS_LINK_vkCmdDrawIndexedIndirectCountKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSemaphoreCounterValueKHR");
	TOS_LINK_vkGetSemaphoreCounterValueKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWaitSemaphoresKHR");
	TOS_LINK_vkWaitSemaphoresKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSignalSemaphoreKHR");
	TOS_LINK_vkSignalSemaphoreKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceFragmentShadingRatesKHR");
	TOS_LINK_vkGetPhysicalDeviceFragmentShadingRatesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWaitForPresentKHR");
	TOS_LINK_vkWaitForPresentKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferDeviceAddressKHR");
	TOS_LINK_vkGetBufferDeviceAddressKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDeferredOperationKHR");
	TOS_LINK_vkCreateDeferredOperationKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDeferredOperationKHR");
	TOS_LINK_vkDestroyDeferredOperationKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeferredOperationResultKHR");
	TOS_LINK_vkGetDeferredOperationResultKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDeferredOperationJoinKHR");
	TOS_LINK_vkDeferredOperationJoinKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPipelineExecutablePropertiesKHR");
	TOS_LINK_vkGetPipelineExecutablePropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPipelineExecutableStatisticsKHR");
	TOS_LINK_vkGetPipelineExecutableStatisticsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPipelineExecutableInternalRepresentationsKHR");
	TOS_LINK_vkGetPipelineExecutableInternalRepresentationsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetEvent2KHR");
	TOS_LINK_vkCmdSetEvent2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResetEvent2KHR");
	TOS_LINK_vkCmdResetEvent2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWaitEvents2KHR");
	TOS_LINK_vkCmdWaitEvents2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPipelineBarrier2KHR");
	TOS_LINK_vkCmdPipelineBarrier2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteTimestamp2KHR");
	TOS_LINK_vkCmdWriteTimestamp2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueSubmit2KHR");
	TOS_LINK_vkQueueSubmit2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteBufferMarker2AMD");
	TOS_LINK_vkCmdWriteBufferMarker2AMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetQueueCheckpointData2NV");
	TOS_LINK_vkGetQueueCheckpointData2NV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBuffer2KHR");
	TOS_LINK_vkCmdCopyBuffer2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImage2KHR");
	TOS_LINK_vkCmdCopyImage2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyBufferToImage2KHR");
	TOS_LINK_vkCmdCopyBufferToImage2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyImageToBuffer2KHR");
	TOS_LINK_vkCmdCopyImageToBuffer2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBlitImage2KHR");
	TOS_LINK_vkCmdBlitImage2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdResolveImage2KHR");
	TOS_LINK_vkCmdResolveImage2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdTraceRaysIndirect2KHR");
	TOS_LINK_vkCmdTraceRaysIndirect2KHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceBufferMemoryRequirementsKHR");
	TOS_LINK_vkGetDeviceBufferMemoryRequirementsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceImageMemoryRequirementsKHR");
	TOS_LINK_vkGetDeviceImageMemoryRequirementsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceImageSparseMemoryRequirementsKHR");
	TOS_LINK_vkGetDeviceImageSparseMemoryRequirementsKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDebugReportCallbackEXT");
	TOS_LINK_vkCreateDebugReportCallbackEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDebugReportCallbackEXT");
	TOS_LINK_vkDestroyDebugReportCallbackEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDebugReportMessageEXT");
	TOS_LINK_vkDebugReportMessageEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDebugMarkerSetObjectTagEXT");
	TOS_LINK_vkDebugMarkerSetObjectTagEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDebugMarkerSetObjectNameEXT");
	TOS_LINK_vkDebugMarkerSetObjectNameEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDebugMarkerBeginEXT");
	TOS_LINK_vkCmdDebugMarkerBeginEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDebugMarkerEndEXT");
	TOS_LINK_vkCmdDebugMarkerEndEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDebugMarkerInsertEXT");
	TOS_LINK_vkCmdDebugMarkerInsertEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindTransformFeedbackBuffersEXT");
	TOS_LINK_vkCmdBindTransformFeedbackBuffersEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginTransformFeedbackEXT");
	TOS_LINK_vkCmdBeginTransformFeedbackEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndTransformFeedbackEXT");
	TOS_LINK_vkCmdEndTransformFeedbackEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginQueryIndexedEXT");
	TOS_LINK_vkCmdBeginQueryIndexedEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndQueryIndexedEXT");
	TOS_LINK_vkCmdEndQueryIndexedEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndirectByteCountEXT");
	TOS_LINK_vkCmdDrawIndirectByteCountEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateCuModuleNVX");
	TOS_LINK_vkCreateCuModuleNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateCuFunctionNVX");
	TOS_LINK_vkCreateCuFunctionNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyCuModuleNVX");
	TOS_LINK_vkDestroyCuModuleNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyCuFunctionNVX");
	TOS_LINK_vkDestroyCuFunctionNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCuLaunchKernelNVX");
	TOS_LINK_vkCmdCuLaunchKernelNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageViewAddressNVX");
	TOS_LINK_vkGetImageViewAddressNVX = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndirectCountAMD");
	TOS_LINK_vkCmdDrawIndirectCountAMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawIndexedIndirectCountAMD");
	TOS_LINK_vkCmdDrawIndexedIndirectCountAMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetShaderInfoAMD");
	TOS_LINK_vkGetShaderInfoAMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
	TOS_LINK_vkGetPhysicalDeviceExternalImageFormatPropertiesNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginConditionalRenderingEXT");
	TOS_LINK_vkCmdBeginConditionalRenderingEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndConditionalRenderingEXT");
	TOS_LINK_vkCmdEndConditionalRenderingEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportWScalingNV");
	TOS_LINK_vkCmdSetViewportWScalingNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkReleaseDisplayEXT");
	TOS_LINK_vkReleaseDisplayEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSurfaceCapabilities2EXT");
	TOS_LINK_vkGetPhysicalDeviceSurfaceCapabilities2EXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDisplayPowerControlEXT");
	TOS_LINK_vkDisplayPowerControlEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkRegisterDeviceEventEXT");
	TOS_LINK_vkRegisterDeviceEventEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkRegisterDisplayEventEXT");
	TOS_LINK_vkRegisterDisplayEventEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSwapchainCounterEXT");
	TOS_LINK_vkGetSwapchainCounterEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRefreshCycleDurationGOOGLE");
	TOS_LINK_vkGetRefreshCycleDurationGOOGLE = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPastPresentationTimingGOOGLE");
	TOS_LINK_vkGetPastPresentationTimingGOOGLE = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDiscardRectangleEXT");
	TOS_LINK_vkCmdSetDiscardRectangleEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDiscardRectangleEnableEXT");
	TOS_LINK_vkCmdSetDiscardRectangleEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDiscardRectangleModeEXT");
	TOS_LINK_vkCmdSetDiscardRectangleModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetHdrMetadataEXT");
	TOS_LINK_vkSetHdrMetadataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetDebugUtilsObjectNameEXT");
	TOS_LINK_vkSetDebugUtilsObjectNameEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetDebugUtilsObjectTagEXT");
	TOS_LINK_vkSetDebugUtilsObjectTagEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueBeginDebugUtilsLabelEXT");
	TOS_LINK_vkQueueBeginDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueEndDebugUtilsLabelEXT");
	TOS_LINK_vkQueueEndDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueInsertDebugUtilsLabelEXT");
	TOS_LINK_vkQueueInsertDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBeginDebugUtilsLabelEXT");
	TOS_LINK_vkCmdBeginDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdEndDebugUtilsLabelEXT");
	TOS_LINK_vkCmdEndDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdInsertDebugUtilsLabelEXT");
	TOS_LINK_vkCmdInsertDebugUtilsLabelEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateDebugUtilsMessengerEXT");
	TOS_LINK_vkCreateDebugUtilsMessengerEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyDebugUtilsMessengerEXT");
	TOS_LINK_vkDestroyDebugUtilsMessengerEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSubmitDebugUtilsMessageEXT");
	TOS_LINK_vkSubmitDebugUtilsMessageEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetSampleLocationsEXT");
	TOS_LINK_vkCmdSetSampleLocationsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceMultisamplePropertiesEXT");
	TOS_LINK_vkGetPhysicalDeviceMultisamplePropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageDrmFormatModifierPropertiesEXT");
	TOS_LINK_vkGetImageDrmFormatModifierPropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateValidationCacheEXT");
	TOS_LINK_vkCreateValidationCacheEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyValidationCacheEXT");
	TOS_LINK_vkDestroyValidationCacheEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkMergeValidationCachesEXT");
	TOS_LINK_vkMergeValidationCachesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetValidationCacheDataEXT");
	TOS_LINK_vkGetValidationCacheDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindShadingRateImageNV");
	TOS_LINK_vkCmdBindShadingRateImageNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportShadingRatePaletteNV");
	TOS_LINK_vkCmdSetViewportShadingRatePaletteNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoarseSampleOrderNV");
	TOS_LINK_vkCmdSetCoarseSampleOrderNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateAccelerationStructureNV");
	TOS_LINK_vkCreateAccelerationStructureNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyAccelerationStructureNV");
	TOS_LINK_vkDestroyAccelerationStructureNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetAccelerationStructureMemoryRequirementsNV");
	TOS_LINK_vkGetAccelerationStructureMemoryRequirementsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindAccelerationStructureMemoryNV");
	TOS_LINK_vkBindAccelerationStructureMemoryNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBuildAccelerationStructureNV");
	TOS_LINK_vkCmdBuildAccelerationStructureNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyAccelerationStructureNV");
	TOS_LINK_vkCmdCopyAccelerationStructureNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdTraceRaysNV");
	TOS_LINK_vkCmdTraceRaysNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateRayTracingPipelinesNV");
	TOS_LINK_vkCreateRayTracingPipelinesNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRayTracingShaderGroupHandlesKHR");
	TOS_LINK_vkGetRayTracingShaderGroupHandlesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRayTracingShaderGroupHandlesNV");
	TOS_LINK_vkGetRayTracingShaderGroupHandlesNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetAccelerationStructureHandleNV");
	TOS_LINK_vkGetAccelerationStructureHandleNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteAccelerationStructuresPropertiesNV");
	TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCompileDeferredNV");
	TOS_LINK_vkCompileDeferredNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetMemoryHostPointerPropertiesEXT");
	TOS_LINK_vkGetMemoryHostPointerPropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteBufferMarkerAMD");
	TOS_LINK_vkCmdWriteBufferMarkerAMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
	TOS_LINK_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetCalibratedTimestampsEXT");
	TOS_LINK_vkGetCalibratedTimestampsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksNV");
	TOS_LINK_vkCmdDrawMeshTasksNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksIndirectNV");
	TOS_LINK_vkCmdDrawMeshTasksIndirectNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksIndirectCountNV");
	TOS_LINK_vkCmdDrawMeshTasksIndirectCountNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetExclusiveScissorEnableNV");
	TOS_LINK_vkCmdSetExclusiveScissorEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetExclusiveScissorNV");
	TOS_LINK_vkCmdSetExclusiveScissorNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCheckpointNV");
	TOS_LINK_vkCmdSetCheckpointNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetQueueCheckpointDataNV");
	TOS_LINK_vkGetQueueCheckpointDataNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkInitializePerformanceApiINTEL");
	TOS_LINK_vkInitializePerformanceApiINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkUninitializePerformanceApiINTEL");
	TOS_LINK_vkUninitializePerformanceApiINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPerformanceMarkerINTEL");
	TOS_LINK_vkCmdSetPerformanceMarkerINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPerformanceStreamMarkerINTEL");
	TOS_LINK_vkCmdSetPerformanceStreamMarkerINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPerformanceOverrideINTEL");
	TOS_LINK_vkCmdSetPerformanceOverrideINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAcquirePerformanceConfigurationINTEL");
	TOS_LINK_vkAcquirePerformanceConfigurationINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkReleasePerformanceConfigurationINTEL");
	TOS_LINK_vkReleasePerformanceConfigurationINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkQueueSetPerformanceConfigurationINTEL");
	TOS_LINK_vkQueueSetPerformanceConfigurationINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPerformanceParameterINTEL");
	TOS_LINK_vkGetPerformanceParameterINTEL = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetLocalDimmingAMD");
	TOS_LINK_vkSetLocalDimmingAMD = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferDeviceAddressEXT");
	TOS_LINK_vkGetBufferDeviceAddressEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceToolPropertiesEXT");
	TOS_LINK_vkGetPhysicalDeviceToolPropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
	TOS_LINK_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
	TOS_LINK_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateHeadlessSurfaceEXT");
	TOS_LINK_vkCreateHeadlessSurfaceEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLineStippleEXT");
	TOS_LINK_vkCmdSetLineStippleEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkResetQueryPoolEXT");
	TOS_LINK_vkResetQueryPoolEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCullModeEXT");
	TOS_LINK_vkCmdSetCullModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetFrontFaceEXT");
	TOS_LINK_vkCmdSetFrontFaceEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPrimitiveTopologyEXT");
	TOS_LINK_vkCmdSetPrimitiveTopologyEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportWithCountEXT");
	TOS_LINK_vkCmdSetViewportWithCountEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetScissorWithCountEXT");
	TOS_LINK_vkCmdSetScissorWithCountEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindVertexBuffers2EXT");
	TOS_LINK_vkCmdBindVertexBuffers2EXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthTestEnableEXT");
	TOS_LINK_vkCmdSetDepthTestEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthWriteEnableEXT");
	TOS_LINK_vkCmdSetDepthWriteEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthCompareOpEXT");
	TOS_LINK_vkCmdSetDepthCompareOpEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBoundsTestEnableEXT");
	TOS_LINK_vkCmdSetDepthBoundsTestEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilTestEnableEXT");
	TOS_LINK_vkCmdSetStencilTestEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetStencilOpEXT");
	TOS_LINK_vkCmdSetStencilOpEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkReleaseSwapchainImagesEXT");
	TOS_LINK_vkReleaseSwapchainImagesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetGeneratedCommandsMemoryRequirementsNV");
	TOS_LINK_vkGetGeneratedCommandsMemoryRequirementsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdPreprocessGeneratedCommandsNV");
	TOS_LINK_vkCmdPreprocessGeneratedCommandsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdExecuteGeneratedCommandsNV");
	TOS_LINK_vkCmdExecuteGeneratedCommandsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindPipelineShaderGroupNV");
	TOS_LINK_vkCmdBindPipelineShaderGroupNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateIndirectCommandsLayoutNV");
	TOS_LINK_vkCreateIndirectCommandsLayoutNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyIndirectCommandsLayoutNV");
	TOS_LINK_vkDestroyIndirectCommandsLayoutNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkAcquireDrmDisplayEXT");
	TOS_LINK_vkAcquireDrmDisplayEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDrmDisplayEXT");
	TOS_LINK_vkGetDrmDisplayEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreatePrivateDataSlotEXT");
	TOS_LINK_vkCreatePrivateDataSlotEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyPrivateDataSlotEXT");
	TOS_LINK_vkDestroyPrivateDataSlotEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetPrivateDataEXT");
	TOS_LINK_vkSetPrivateDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPrivateDataEXT");
	TOS_LINK_vkGetPrivateDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetLayoutSizeEXT");
	TOS_LINK_vkGetDescriptorSetLayoutSizeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetLayoutBindingOffsetEXT");
	TOS_LINK_vkGetDescriptorSetLayoutBindingOffsetEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorEXT");
	TOS_LINK_vkGetDescriptorEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindDescriptorBuffersEXT");
	TOS_LINK_vkCmdBindDescriptorBuffersEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDescriptorBufferOffsetsEXT");
	TOS_LINK_vkCmdSetDescriptorBufferOffsetsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindDescriptorBufferEmbeddedSamplersEXT");
	TOS_LINK_vkCmdBindDescriptorBufferEmbeddedSamplersEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetBufferOpaqueCaptureDescriptorDataEXT");
	TOS_LINK_vkGetBufferOpaqueCaptureDescriptorDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageOpaqueCaptureDescriptorDataEXT");
	TOS_LINK_vkGetImageOpaqueCaptureDescriptorDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageViewOpaqueCaptureDescriptorDataEXT");
	TOS_LINK_vkGetImageViewOpaqueCaptureDescriptorDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetSamplerOpaqueCaptureDescriptorDataEXT");
	TOS_LINK_vkGetSamplerOpaqueCaptureDescriptorDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT");
	TOS_LINK_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetImageSubresourceLayout2EXT");
	TOS_LINK_vkGetImageSubresourceLayout2EXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceFaultInfoEXT");
	TOS_LINK_vkGetDeviceFaultInfoEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetVertexInputEXT");
	TOS_LINK_vkCmdSetVertexInputEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI");
	TOS_LINK_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSubpassShadingHUAWEI");
	TOS_LINK_vkCmdSubpassShadingHUAWEI = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBindInvocationMaskHUAWEI");
	TOS_LINK_vkCmdBindInvocationMaskHUAWEI = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetMemoryRemoteAddressNV");
	TOS_LINK_vkGetMemoryRemoteAddressNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPipelinePropertiesEXT");
	TOS_LINK_vkGetPipelinePropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPatchControlPointsEXT");
	TOS_LINK_vkCmdSetPatchControlPointsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRasterizerDiscardEnableEXT");
	TOS_LINK_vkCmdSetRasterizerDiscardEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthBiasEnableEXT");
	TOS_LINK_vkCmdSetDepthBiasEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLogicOpEXT");
	TOS_LINK_vkCmdSetLogicOpEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPrimitiveRestartEnableEXT");
	TOS_LINK_vkCmdSetPrimitiveRestartEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMultiEXT");
	TOS_LINK_vkCmdDrawMultiEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMultiIndexedEXT");
	TOS_LINK_vkCmdDrawMultiIndexedEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateMicromapEXT");
	TOS_LINK_vkCreateMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyMicromapEXT");
	TOS_LINK_vkDestroyMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBuildMicromapsEXT");
	TOS_LINK_vkCmdBuildMicromapsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBuildMicromapsEXT");
	TOS_LINK_vkBuildMicromapsEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyMicromapEXT");
	TOS_LINK_vkCopyMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyMicromapToMemoryEXT");
	TOS_LINK_vkCopyMicromapToMemoryEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyMemoryToMicromapEXT");
	TOS_LINK_vkCopyMemoryToMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWriteMicromapsPropertiesEXT");
	TOS_LINK_vkWriteMicromapsPropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMicromapEXT");
	TOS_LINK_vkCmdCopyMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMicromapToMemoryEXT");
	TOS_LINK_vkCmdCopyMicromapToMemoryEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMemoryToMicromapEXT");
	TOS_LINK_vkCmdCopyMemoryToMicromapEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteMicromapsPropertiesEXT");
	TOS_LINK_vkCmdWriteMicromapsPropertiesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceMicromapCompatibilityEXT");
	TOS_LINK_vkGetDeviceMicromapCompatibilityEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetMicromapBuildSizesEXT");
	TOS_LINK_vkGetMicromapBuildSizesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawClusterHUAWEI");
	TOS_LINK_vkCmdDrawClusterHUAWEI = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawClusterIndirectHUAWEI");
	TOS_LINK_vkCmdDrawClusterIndirectHUAWEI = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkSetDeviceMemoryPriorityEXT");
	TOS_LINK_vkSetDeviceMemoryPriorityEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetLayoutHostMappingInfoVALVE");
	TOS_LINK_vkGetDescriptorSetLayoutHostMappingInfoVALVE = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDescriptorSetHostMappingVALVE");
	TOS_LINK_vkGetDescriptorSetHostMappingVALVE = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMemoryIndirectNV");
	TOS_LINK_vkCmdCopyMemoryIndirectNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMemoryToImageIndirectNV");
	TOS_LINK_vkCmdCopyMemoryToImageIndirectNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDecompressMemoryNV");
	TOS_LINK_vkCmdDecompressMemoryNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDecompressMemoryIndirectCountNV");
	TOS_LINK_vkCmdDecompressMemoryIndirectCountNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetTessellationDomainOriginEXT");
	TOS_LINK_vkCmdSetTessellationDomainOriginEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthClampEnableEXT");
	TOS_LINK_vkCmdSetDepthClampEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetPolygonModeEXT");
	TOS_LINK_vkCmdSetPolygonModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRasterizationSamplesEXT");
	TOS_LINK_vkCmdSetRasterizationSamplesEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetSampleMaskEXT");
	TOS_LINK_vkCmdSetSampleMaskEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetAlphaToCoverageEnableEXT");
	TOS_LINK_vkCmdSetAlphaToCoverageEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetAlphaToOneEnableEXT");
	TOS_LINK_vkCmdSetAlphaToOneEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLogicOpEnableEXT");
	TOS_LINK_vkCmdSetLogicOpEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetColorBlendEnableEXT");
	TOS_LINK_vkCmdSetColorBlendEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetColorBlendEquationEXT");
	TOS_LINK_vkCmdSetColorBlendEquationEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetColorWriteMaskEXT");
	TOS_LINK_vkCmdSetColorWriteMaskEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRasterizationStreamEXT");
	TOS_LINK_vkCmdSetRasterizationStreamEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetConservativeRasterizationModeEXT");
	TOS_LINK_vkCmdSetConservativeRasterizationModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetExtraPrimitiveOverestimationSizeEXT");
	TOS_LINK_vkCmdSetExtraPrimitiveOverestimationSizeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthClipEnableEXT");
	TOS_LINK_vkCmdSetDepthClipEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetSampleLocationsEnableEXT");
	TOS_LINK_vkCmdSetSampleLocationsEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetColorBlendAdvancedEXT");
	TOS_LINK_vkCmdSetColorBlendAdvancedEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetProvokingVertexModeEXT");
	TOS_LINK_vkCmdSetProvokingVertexModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLineRasterizationModeEXT");
	TOS_LINK_vkCmdSetLineRasterizationModeEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetLineStippleEnableEXT");
	TOS_LINK_vkCmdSetLineStippleEnableEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetDepthClipNegativeOneToOneEXT");
	TOS_LINK_vkCmdSetDepthClipNegativeOneToOneEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportWScalingEnableNV");
	TOS_LINK_vkCmdSetViewportWScalingEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetViewportSwizzleNV");
	TOS_LINK_vkCmdSetViewportSwizzleNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageToColorEnableNV");
	TOS_LINK_vkCmdSetCoverageToColorEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageToColorLocationNV");
	TOS_LINK_vkCmdSetCoverageToColorLocationNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageModulationModeNV");
	TOS_LINK_vkCmdSetCoverageModulationModeNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageModulationTableEnableNV");
	TOS_LINK_vkCmdSetCoverageModulationTableEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageModulationTableNV");
	TOS_LINK_vkCmdSetCoverageModulationTableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetShadingRateImageEnableNV");
	TOS_LINK_vkCmdSetShadingRateImageEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRepresentativeFragmentTestEnableNV");
	TOS_LINK_vkCmdSetRepresentativeFragmentTestEnableNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetCoverageReductionModeNV");
	TOS_LINK_vkCmdSetCoverageReductionModeNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetShaderModuleIdentifierEXT");
	TOS_LINK_vkGetShaderModuleIdentifierEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetShaderModuleCreateInfoIdentifierEXT");
	TOS_LINK_vkGetShaderModuleCreateInfoIdentifierEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetPhysicalDeviceOpticalFlowImageFormatsNV");
	TOS_LINK_vkGetPhysicalDeviceOpticalFlowImageFormatsNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateOpticalFlowSessionNV");
	TOS_LINK_vkCreateOpticalFlowSessionNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyOpticalFlowSessionNV");
	TOS_LINK_vkDestroyOpticalFlowSessionNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBindOpticalFlowSessionImageNV");
	TOS_LINK_vkBindOpticalFlowSessionImageNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdOpticalFlowExecuteNV");
	TOS_LINK_vkCmdOpticalFlowExecuteNV = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetFramebufferTilePropertiesQCOM");
	TOS_LINK_vkGetFramebufferTilePropertiesQCOM = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDynamicRenderingTilePropertiesQCOM");
	TOS_LINK_vkGetDynamicRenderingTilePropertiesQCOM = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateAccelerationStructureKHR");
	TOS_LINK_vkCreateAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkDestroyAccelerationStructureKHR");
	TOS_LINK_vkDestroyAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBuildAccelerationStructuresKHR");
	TOS_LINK_vkCmdBuildAccelerationStructuresKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdBuildAccelerationStructuresIndirectKHR");
	TOS_LINK_vkCmdBuildAccelerationStructuresIndirectKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkBuildAccelerationStructuresKHR");
	TOS_LINK_vkBuildAccelerationStructuresKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyAccelerationStructureKHR");
	TOS_LINK_vkCopyAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyAccelerationStructureToMemoryKHR");
	TOS_LINK_vkCopyAccelerationStructureToMemoryKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCopyMemoryToAccelerationStructureKHR");
	TOS_LINK_vkCopyMemoryToAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkWriteAccelerationStructuresPropertiesKHR");
	TOS_LINK_vkWriteAccelerationStructuresPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyAccelerationStructureKHR");
	TOS_LINK_vkCmdCopyAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyAccelerationStructureToMemoryKHR");
	TOS_LINK_vkCmdCopyAccelerationStructureToMemoryKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdCopyMemoryToAccelerationStructureKHR");
	TOS_LINK_vkCmdCopyMemoryToAccelerationStructureKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetAccelerationStructureDeviceAddressKHR");
	TOS_LINK_vkGetAccelerationStructureDeviceAddressKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdWriteAccelerationStructuresPropertiesKHR");
	TOS_LINK_vkCmdWriteAccelerationStructuresPropertiesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetDeviceAccelerationStructureCompatibilityKHR");
	TOS_LINK_vkGetDeviceAccelerationStructureCompatibilityKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetAccelerationStructureBuildSizesKHR");
	TOS_LINK_vkGetAccelerationStructureBuildSizesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdTraceRaysKHR");
	TOS_LINK_vkCmdTraceRaysKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCreateRayTracingPipelinesKHR");
	TOS_LINK_vkCreateRayTracingPipelinesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
	TOS_LINK_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdTraceRaysIndirectKHR");
	TOS_LINK_vkCmdTraceRaysIndirectKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkGetRayTracingShaderGroupStackSizeKHR");
	TOS_LINK_vkGetRayTracingShaderGroupStackSizeKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdSetRayTracingPipelineStackSizeKHR");
	TOS_LINK_vkCmdSetRayTracingPipelineStackSizeKHR = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksEXT");
	TOS_LINK_vkCmdDrawMeshTasksEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksIndirectEXT");
	TOS_LINK_vkCmdDrawMeshTasksIndirectEXT = sym ? sym : (void*) &TOS_nullFunc;
	sym = (void*) library.takeSymbol("vkCmdDrawMeshTasksIndirectCountEXT");
	TOS_LINK_vkCmdDrawMeshTasksIndirectCountEXT = sym ? sym : (void*) &TOS_nullFunc;
}
