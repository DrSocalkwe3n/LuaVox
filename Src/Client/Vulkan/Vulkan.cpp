#include <boost/asio/io_context.hpp>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include "Vulkan.hpp"
#include "Client/ServerSession.hpp"
#include "Common/Async.hpp"
#include "Common/Net.hpp"
#include "assets.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#ifdef HAS_IMGUI
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#endif
#include <freetype/ftglyph.h>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <png++/png.hpp>
#include "VulkanRenderSession.hpp"
#include <Server/GameServer.hpp>

extern void LoadSymbolsVulkan(TOS::DynamicLibrary &library);

namespace LV::Client::VK {

struct ServerObj {
	Server::GameServer GS;
	Net::SocketServer LS;

	ServerObj(asio::io_context &ioc)
		: GS(ioc, ""), LS(ioc, [&](tcp::socket sock) -> coro<> { co_await GS.pushSocketConnect(std::move(sock)); }, 7890)
	{
	}
};

ByteBuffer loadPNG(std::ifstream &&read, int &width, int &height, bool &hasAlpha, bool flipOver)
{
	png::image<png::rgba_pixel> img(read);
	width = img.get_width();
	height = img.get_height();
	hasAlpha = true;
	ByteBuffer buff(4*width*height);
	for(int i = 0; i<height; i++)
		std::copy(
				((const char*) &img.get_pixbuf().operator [](i)[0]),
				((const char*) &img.get_pixbuf().operator [](i)[0])+4*width,
				(buff.data())+4*width*(flipOver ? height-i-1 : i));
	return buff;
}

ByteBuffer loadPNG(std::istream &&read, int &width, int &height, bool &hasAlpha, bool flipOver)
{
	png::image<png::rgba_pixel> img(read);
	width = img.get_width();
	height = img.get_height();
	hasAlpha = true;
	ByteBuffer buff(4*width*height);
	for(int i = 0; i<height; i++)
		std::copy(
				((const char*) &img.get_pixbuf().operator [](i)[0]),
				((const char*) &img.get_pixbuf().operator [](i)[0])+4*width,
				(buff.data())+4*width*(flipOver ? height-i-1 : i));
	return buff;
}

Vulkan::Vulkan(asio::io_context &ioc)
	: AsyncObject(ioc), GuardLock(ioc.get_executor())
{
	Screen.Width = 1920/2;
	Screen.Height = 1080/2;
	getSettingsNext() = getBestSettings();
	reInit();

	Game.ImGuiInterfaces.push_back(&Vulkan::gui_MainMenu);

	Game.MainThread = std::thread([&]() {
		auto useLock = Game.UseLock.lock();
		try {
			run();
		} catch(const std::exception &exc) {
			LOG.error() << "Vulkan::run: " << exc.what();
		}

		try { Game.RSession = nullptr; } catch(const std::exception &exc) {
			LOG.error() << "Game.RSession = nullptr: " << exc.what();
		}

		try { Game.Session = nullptr; } catch(const std::exception &exc) {
			LOG.error() << "Game.Session = nullptr: " << exc.what();
		}

		try { Game.Server = nullptr; } catch(const std::exception &exc) {
			LOG.error() << "Game.Server = nullptr: " << exc.what();
		}

		GuardLock.reset();
	});
}

Vulkan::~Vulkan()
{
	Game.UseLock.wait_no_use();
	Game.MainThread.join();

	for(std::shared_ptr<IVulkanDependent> dependent : ROS_Dependents)
		dependent->free(this);
	ROS_Dependents.clear();

	deInitVulkan();

	if(Graphics.Window)
	{
		glfwDestroyWindow(Graphics.Window);
		Graphics.Window = nullptr;
	}
}

void Vulkan::run()
{
	NeedShutdown = false;
	Graphics.ThisThread = std::this_thread::get_id();

	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
	VkSemaphore SemaphoreImageAcquired, SemaphoreDrawComplete;

	assert(!vkCreateSemaphore(Graphics.Device, &semaphoreCreateInfo, NULL, &SemaphoreImageAcquired));
	assert(!vkCreateSemaphore(Graphics.Device, &semaphoreCreateInfo, NULL, &SemaphoreDrawComplete));


	double prevTime = glfwGetTime();
	while(!NeedShutdown)
	{
		float dTime = glfwGetTime()-prevTime;
		prevTime = glfwGetTime();

		Screen.State = DrawState::Begin;
		{
			std::lock_guard lock(Screen.BeforeDrawMtx);
			while(!Screen.BeforeDraw.empty())
			{
				Screen.BeforeDraw.front()(this);
				Screen.BeforeDraw.pop();
			}
		}

		if(!NeedShutdown && glfwWindowShouldClose(Graphics.Window)) {
			NeedShutdown = true;

			try {
				if(Game.Session)
					Game.Session->shutdown(EnumDisconnect::ByInterface);
			} catch(const std::exception &exc) {
				LOG.error() << "Game.Session->shutdown: " << exc.what();
			}

			try {
				if(Game.Server) 
					Game.Server->GS.shutdown("Завершение работы из-за остановки клиента");
			} catch(const std::exception &exc) {
				LOG.error() << "Game.Server->GS.shutdown: " << exc.what();
			}
		}

		if(Game.Session) {
			ServerSession &sobj = *Game.Session;

			// Спрятать или показать курсор
			{
				int mode = glfwGetInputMode(Graphics.Window, GLFW_CURSOR);
				if(mode == GLFW_CURSOR_HIDDEN && sobj.CursorMode != ISurfaceEventListener::EnumCursorMoveMode::MoveAndHidden)
					glfwSetInputMode(Graphics.Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				else if(mode == GLFW_CURSOR_NORMAL && sobj.CursorMode != ISurfaceEventListener::EnumCursorMoveMode::Default) {
					glfwSetInputMode(Graphics.Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
					glfwSetCursorPos(Graphics.Window, Screen.Width/2., Screen.Height/2.);
				}
			}


		}

		// if(CallBeforeDraw)
		// 	CallBeforeDraw(this);

		if(Game.RSession) {
			Game.RSession->beforeDraw();
		}

		glfwPollEvents();

		VkResult err;
		err = vkAcquireNextImageKHR(Graphics.Device, Graphics.Swapchain, UINT64_MAX, SemaphoreImageAcquired, (VkFence) 0, &Graphics.DrawBufferCurrent);

		if (err == VK_ERROR_OUT_OF_DATE_KHR)
		{
			freeSwapchains();
			buildSwapchains();
			continue;
		} else if (err == VK_SUBOPTIMAL_KHR)
		{
			LOGGER.debug() << "VK_SUBOPTIMAL_KHR Pre";
		} else
			assert(!err);


		Screen.State = DrawState::Drawing;
		//Готовим инструкции рисовки
		{
			const VkCommandBufferBeginInfo cmd_buf_info =
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr
			};

			assert(!vkBeginCommandBuffer(Graphics.CommandBufferRender, &cmd_buf_info));
		}

		{
			VkImageMemoryBarrier image_memory_barrier =
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image, // Graphics.InlineTexture.Image,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};

			vkCmdPipelineBarrier(Graphics.CommandBufferRender, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
		}

		{
			const VkClearValue clear_values[2] =
			{
				[0] = { .color = { .float32 = { 0.1f, 0.1f, 0.1f, 1.f }}},
				[1] = { .depthStencil = { 1, 0 } },
			};
		
			const VkRenderPassBeginInfo rp_begin =
			{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = Graphics.RenderPass,
				.framebuffer = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].FrameBuffer, //Graphics.InlineTexture.Frame,
				.renderArea = VkRect2D {
					.offset = {0, 0},
					.extent = Screen.FrameExtent
				},
				.clearValueCount = 2,
				.pClearValues = clear_values
			};
			
			vkCmdBeginRenderPass(Graphics.CommandBufferRender, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
		}


		{
			VkViewport viewport = { 0.f, 0.f, float(Screen.Width), float(Screen.Height), 0.f, 1.f };
			vkCmdSetViewport(Graphics.CommandBufferRender, 0, 1, &viewport);

			VkRect2D scissor = { { int32_t(0), int32_t(0) }, { Screen.Width, Screen.Height } };
			vkCmdSetScissor(Graphics.CommandBufferRender, 0, 1, &scissor);
		}

		GlobalTime gTime = glfwGetTime();

		if(Game.RSession) {
			auto &robj = *Game.RSession;
			// Рендер мира

			robj.drawWorld(gTime, dTime, Graphics.CommandBufferRender);

            uint16_t minSize = std::min(Screen.Width, Screen.Height);
            glm::ivec2 interfaceSize = {int(Screen.Width*720/minSize), int(Screen.Height*720/minSize)};
		}

		// vkCmdEndRenderPass(Graphics.CommandBufferRender);

		// {
		// 	VkImageMemoryBarrier src_barrier =
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		// 		.pNext = nullptr,
		// 		.srcAccessMask = 0,
		// 		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		// 		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		// 		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.image = Graphics.InlineTexture.Image,
		// 		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		// 	};

		// 	VkImageMemoryBarrier dst_barrier =
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		// 		.pNext = nullptr,
		// 		.srcAccessMask = 0,
		// 		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		// 		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		// 		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.image = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image,
		// 		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		// 	};

		// 	vkCmdPipelineBarrier(
		// 		Graphics.CommandBufferRender,
		// 		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		// 		VK_PIPELINE_STAGE_TRANSFER_BIT,
		// 		0,
		// 		0, nullptr,
		// 		0, nullptr,
		// 		1, &src_barrier
		// 	);

		// 	vkCmdPipelineBarrier(
		// 		Graphics.CommandBufferRender,
		// 		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		// 		VK_PIPELINE_STAGE_TRANSFER_BIT,
		// 		0,
		// 		0, nullptr,
		// 		0, nullptr,
		// 		1, &dst_barrier
		// 	);

		// 	VkImageCopy copy_region =
		// 	{
		// 		.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		// 		.srcOffset = {0, 0, 0},
		// 		.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		// 		.dstOffset = {0, 0, 0},
		// 		.extent = {Screen.FrameExtent.width, Screen.FrameExtent.height, 1}
		// 	};

		// 	vkCmdCopyImage(
		// 		Graphics.CommandBufferRender,
		// 		Graphics.InlineTexture.Image,
		// 		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		// 		Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image,
		// 		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		// 		1, &copy_region
		// 	);

		// 	VkImageMemoryBarrier post_copy_barrier =
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		// 		.pNext = nullptr,
		// 		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		// 		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		// 		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		// 		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.image = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image,
		// 		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		// 	};

		// 	vkCmdPipelineBarrier(
		// 		Graphics.CommandBufferRender,
		// 		VK_PIPELINE_STAGE_TRANSFER_BIT,
		// 		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		// 		0,
		// 		0, nullptr,
		// 		0, nullptr,
		// 		1, &post_copy_barrier
		// 	);
		// }
		
		// {
		// 	VkImageMemoryBarrier prePresentBarrier =
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		// 		.pNext = nullptr,
		// 		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		// 		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		// 		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		// 		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.image = Graphics.InlineTexture.Image,
		// 		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		// 	};

		// 	vkCmdPipelineBarrier(Graphics.CommandBufferRender, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		// 			0, 0, nullptr, 0, nullptr, 1, &prePresentBarrier);
		// }

		// {
		// 	VkImageMemoryBarrier image_memory_barrier = 
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		// 		.pNext = nullptr,
		// 		.srcAccessMask = 0,
		// 		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		// 		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		// 		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		// 		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		// 		.image = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image,
		// 		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		// 	};

		// 	vkCmdPipelineBarrier(Graphics.CommandBufferRender, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		// 			0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
		// }

		// {
		// 	const VkClearValue clear_values[2] =
		// 	{
		// 		[0] = { .color = { .float32 = { 0.1f, 0.1f, 0.1f, 1.0f }}},
		// 		[1] = { .depthStencil = { 1, 0 } },
		// 	};
		
		// 	const VkRenderPassBeginInfo rp_begin =
		// 	{
		// 		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		// 		.pNext = nullptr,
		// 		.renderPass = Graphics.RenderPass,
		// 		.framebuffer = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].FrameBuffer,
		// 		.renderArea = VkRect2D {
		// 			.offset = {0, 0},
		// 			.extent = Screen.FrameExtent
		// 		},
		// 		.clearValueCount = 2,
		// 		.pClearValues = clear_values
		// 	};
			
		// 	vkCmdBeginRenderPass(Graphics.CommandBufferRender, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
		// }



		#ifdef HAS_IMGUI
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize({(float) Screen.Width, (float) Screen.Height});

		assert(Game.ImGuiInterfaces.size());
		(this->*Game.ImGuiInterfaces.back())();

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Graphics.CommandBufferRender);
		#endif

		vkCmdEndRenderPass(Graphics.CommandBufferRender);

		{
			VkImageMemoryBarrier prePresentBarrier =
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = Graphics.DrawBuffers[Graphics.DrawBufferCurrent].Image,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};

			vkCmdPipelineBarrier(Graphics.CommandBufferRender, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					0, 0, nullptr, 0, nullptr, 1, &prePresentBarrier);
		}

		assert(!vkEndCommandBuffer(Graphics.CommandBufferRender));

		{
			VkFence nullFence = VK_NULL_HANDLE;
			VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			VkSubmitInfo submit_info =
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &SemaphoreImageAcquired,
				.pWaitDstStageMask = &pipe_stage_flags,
				.commandBufferCount = 1,
				.pCommandBuffers = &Graphics.CommandBufferRender,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &SemaphoreDrawComplete
			};

			//Рисуем, когда получим картинку
			assert(!vkQueueSubmit(Graphics.DeviceQueueGraphic, 1, &submit_info, nullFence));
		}

		{
			VkPresentInfoKHR present =
			{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = NULL,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &SemaphoreDrawComplete,
				.swapchainCount = 1,
				.pSwapchains = &Graphics.Swapchain,
				.pImageIndices = &Graphics.DrawBufferCurrent
			};

			// Завершаем картинку
			err = vkQueuePresentKHR(Graphics.DeviceQueueGraphic, &present);
			if (err == VK_ERROR_OUT_OF_DATE_KHR)
			{
				freeSwapchains();
				buildSwapchains();
			} else if (err == VK_SUBOPTIMAL_KHR)
				LOGGER.debug() << "VK_SUBOPTIMAL_KHR Post";
			else
				assert(!err);
		}

		if(Game.Session) {
			Game.Session->atFreeDrawTime(gTime, dTime);
		}

		assert(!vkQueueWaitIdle(Graphics.DeviceQueueGraphic));

		vkDeviceWaitIdle(Graphics.Device);
		Screen.State = DrawState::End;
	}

	vkDestroySemaphore(Graphics.Device, SemaphoreImageAcquired, nullptr);
	vkDestroySemaphore(Graphics.Device, SemaphoreDrawComplete, nullptr);
}

void Vulkan::glfwCallbackError(int error, const char *description)
{
	Logger("Vulkan").error() << "glfwError " << error << ": " << description;
}

uint32_t Vulkan::memoryTypeFromProperties(uint32_t bitsOfAcceptableTypes, VkFlags bitsOfAccessMask)
{
	// Просматриваем допустимые типы памяти bitsOfAcceptableTypes
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++, bitsOfAcceptableTypes >>= 1)
		if ((bitsOfAcceptableTypes & 1) == 1)
			// Сверяем с маской необходимого доступа к памяти
			if ((Graphics.DeviceMemoryProperties.memoryTypes[i].propertyFlags & bitsOfAccessMask) == bitsOfAccessMask)
				// Нашли
				return i;


	MAKE_ERROR("Требуемый тип памяти отсутствует, bitsOfAcceptableTypes: "
			<< bitsOfAcceptableTypes << ", bitsOfAccessMask: "
			<< uint32_t(bitsOfAccessMask));
}

void Vulkan::freeSwapchains()
{
	//vkDeviceWaitIdle(Screen.Device);

	if(Graphics.Instance && Graphics.Device)
	{
		std::vector<VkImageView> oldViews;
		std::vector<VkFramebuffer> oldFrames;
		oldViews.push_back(Graphics.DepthView);

		if(Graphics.InlineTexture.View)
		{
			oldViews.push_back(Graphics.InlineTexture.View);
			Graphics.InlineTexture.View = nullptr;
		}

		if(Graphics.InlineTexture.Frame)
		{
			oldFrames.push_back(Graphics.InlineTexture.Frame);
			Graphics.InlineTexture.Frame = nullptr;
		}

		for (uint32_t i = 0; i < Graphics.DrawBuffers.size(); i++)
		{
			auto &obj = Graphics.DrawBuffers[i];

			if(obj.FrameBuffer)
				oldFrames.push_back(obj.FrameBuffer);

			if(obj.View)
				oldViews.push_back(obj.View);

			if(obj.Cmd)
				vkFreeCommandBuffers(Graphics.Device, Graphics.Pool, 1, &obj.Cmd);


			// Изображения предоставлены SwapChain'ом, удаляются автоматически
			/*if(obj.Image)
			{
				vkDestroyImage(Graphics.Device, obj.Image, nullptr);
				obj.Image = nullptr;
			}*/
		}


		beforeDraw([oldViews = std::move(oldViews), oldFrames = std::move(oldFrames),
			depthImage = Graphics.DepthImage, depthMemory = Graphics.DepthMemory, inlineImage = Graphics.InlineTexture.Image, inlineMemory = Graphics.InlineTexture.Memory](Vulkan *instance)
		{
			for(auto &iter : oldFrames)
				vkDestroyFramebuffer(instance->Graphics.Device, iter, nullptr);

			for(auto &iter : oldViews)
				vkDestroyImageView(instance->Graphics.Device, iter, nullptr);

			vkDestroyImage(instance->Graphics.Device, depthImage, nullptr);
			vkFreeMemory(instance->Graphics.Device, depthMemory, nullptr);

			vkDestroyImage(instance->Graphics.Device, inlineImage, nullptr);
			vkFreeMemory(instance->Graphics.Device, inlineMemory, nullptr);
		});

		Graphics.InlineTexture.Image = nullptr;
		Graphics.InlineTexture.Memory = nullptr;
	}

	Graphics.DepthView = nullptr;
	Graphics.DepthImage = nullptr;
	Graphics.DepthMemory = nullptr;
	Graphics.DrawBuffers.clear();
}


static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

void Vulkan::buildSwapchains()
{
	assert(Graphics.PhysicalDevice);
	assert(Graphics.Surface);
	assert(Graphics.Window);

	// Определение нового размера буфера
	std::stringstream report;
	report << "Пересоздание цепочки вывода, текущий размер окна: " << Screen.Width << " x " << Screen.Height << '\n';

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	assert(!vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Graphics.PhysicalDevice, Graphics.Surface, &surfaceCapabilities));

	uint32_t count = -1;
	VkExtent2D swapchainExtent;
	if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
		// Если размер поверхности не определен, размер устанавливается
		// равным размеру запрашиваемых изображений, который должен
		// соответствовать минимальным и максимальным значениям.

		report << "Оконная подсистема не сообщила о размере буфера (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF)\n";
		int width, height;
		glfwGetFramebufferSize(Graphics.Window, &width, &height);
		report << "Размер определён под запросу glfw: " << width << " x " << height << '\n';
		report << "Приводим к ограничениям предоставленным оконной подсистемой:\n\tmin("
				<< surfaceCapabilities.minImageExtent.width << " x "
				<< surfaceCapabilities.minImageExtent.height
				<< ") / max(" << surfaceCapabilities.maxImageExtent.width
				<< " x " << surfaceCapabilities.maxImageExtent.height
				<< ")\n";

		swapchainExtent.width = std::clamp((uint32_t) width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		swapchainExtent.height = std::clamp((uint32_t) height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		report << "Утверждённый размер экранного буфера: "
				<< swapchainExtent.width << " x " << swapchainExtent.height << '\n';
	} else {
		swapchainExtent = surfaceCapabilities.currentExtent;
		report << "Размер буффера, предоставленный оконной подсистемой: "
				<< swapchainExtent.width << " x "
				<< swapchainExtent.height << '\n';
	}

	Screen.FrameExtent = swapchainExtent;

	// Определение количества сменных буферов
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t desiredNumOfSwapchainImages = surfaceCapabilities.minImageCount;
	if (surfaceCapabilities.maxImageCount > 0 && desiredNumOfSwapchainImages > surfaceCapabilities.maxImageCount)
		desiredNumOfSwapchainImages = surfaceCapabilities.maxImageCount;

	report << "Количество изображений в цепочке смены кадров: min(" << surfaceCapabilities.minImageCount
			<< ") / max(" << surfaceCapabilities.maxImageCount
			<< "); Запрошено оконной подсистемой: "
			<< surfaceCapabilities.minImageCount << "; утверждено: "
			<< desiredNumOfSwapchainImages << '\n';

	VkSurfaceTransformFlagsKHR preTransform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		report << "Используемая трансформация: VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR\n";
	} else {
		preTransform = surfaceCapabilities.currentTransform;
		report << "Используемая трансформация: Не VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR\n";
	}

	// Пересоздание Swapchain
	VkSwapchainKHR oldSwapchain = Graphics.Swapchain;
	const VkSwapchainCreateInfoKHR swapchainInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = Graphics.Surface,
		.minImageCount = desiredNumOfSwapchainImages,
		.imageFormat = Graphics.SurfaceFormat,
		.imageColorSpace = Graphics.SurfaceColorSpace,
		.imageExtent = swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = VkSurfaceTransformFlagBitsKHR(preTransform),
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = swapchainPresentMode,
		.clipped = true,
		.oldSwapchain = oldSwapchain
	};

	assert(!vkCreateSwapchainKHR(Graphics.Device, &swapchainInfo, nullptr, &Graphics.Swapchain));

	if (oldSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(Graphics.Device, oldSwapchain, nullptr);

	// Получение сменных буферов
	assert(!vkGetSwapchainImagesKHR(Graphics.Device, Graphics.Swapchain, &count, nullptr));
	std::vector<VkImage> swapchainImages(count);
	assert(!vkGetSwapchainImagesKHR(Graphics.Device, Graphics.Swapchain, &count, swapchainImages.data()));

	Graphics.DrawBuffers.resize(count);
	Graphics.DrawBufferCount = count;
	report << "Получено сменых изображений цепочки: " << count << '\n';

	// Создание отображений для полученых изображений буферов
	for(uint32_t iter = 0; iter < count; iter++)
	{
		VkImageViewCreateInfo color_attachment_view =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = swapchainImages[iter],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = Graphics.SurfaceFormat,
			.components = VkComponentMapping
			{
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			},

			.subresourceRange = VkImageSubresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		Graphics.DrawBuffers[iter].Image = swapchainImages[iter];
		assert(!vkCreateImageView(Graphics.Device, &color_attachment_view, nullptr, &Graphics.DrawBuffers[iter].View));
	}

	// Текущий рабочий буфер обнуляется
	Graphics.DrawBufferCurrent = 0;

	// Пересоздание буфера глубины
	const VkImageCreateInfo depthImage =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = Graphics.DepthFormat,
		.extent = { swapchainExtent.width, swapchainExtent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	};

	assert(!vkCreateImage(Graphics.Device, &depthImage, nullptr, &Graphics.DepthImage));

	// Самостоятельно выделяем память под буфер
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(Graphics.Device, Graphics.DepthImage, &memReqs);

	VkMemoryAllocateInfo memAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = memoryTypeFromProperties(memReqs.memoryTypeBits, 0)
	};

	assert(!vkAllocateMemory(Graphics.Device, &memAlloc, nullptr, &Graphics.DepthMemory));
	assert(!vkBindImageMemory(Graphics.Device, Graphics.DepthImage, Graphics.DepthMemory, 0));

	// Синхронизация формата изображения
	VkImageMemoryBarrier infoImageMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = Graphics.DepthImage,
		.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
	};

	vkCmdPipelineBarrier(Graphics.CommandBufferData, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);
	//setImageLayout(Depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_NONE);

	// Создаём отображение для буфера глубины
	VkImageViewCreateInfo view =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = Graphics.DepthImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = Graphics.DepthFormat,
		.components = { VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange = VkImageSubresourceRange
		{
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	assert(!vkCreateImageView(Graphics.Device, &view, nullptr, &Graphics.DepthView));

	if(!Graphics.InlineTexture.Image) {
		VkImageCreateInfo imageCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = Graphics.SurfaceFormat,
			.extent = {swapchainExtent.width, swapchainExtent.height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage =
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_TRANSFER_DST_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT
				| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		
		assert(!vkCreateImage(Graphics.Device, &imageCreateInfo , nullptr , &Graphics.InlineTexture.Image));
	}

	if(!Graphics.InlineTexture.Memory) {
		VkMemoryRequirements memoryReqs;
		const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

		vkGetImageMemoryRequirements(Graphics.Device, Graphics.InlineTexture.Image, &memoryReqs);

		VkMemoryAllocateInfo memoryAlloc =
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = memoryReqs.size,
			.memoryTypeIndex = memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		};

		assert(!vkAllocateMemory(Graphics.Device, &memoryAlloc, nullptr, &Graphics.InlineTexture.Memory));
		assert(!vkBindImageMemory(Graphics.Device, Graphics.InlineTexture.Image, Graphics.InlineTexture.Memory, 0));

		VkImageMemoryBarrier prePresentBarrier =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = Graphics.InlineTexture.Image,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		vkCmdPipelineBarrier(Graphics.CommandBufferData, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0, 0, nullptr, 0, nullptr, 1, &prePresentBarrier);
		
	}

	if(!Graphics.InlineTexture.View) {
		view.image = Graphics.InlineTexture.Image;
		view.format = VK_FORMAT_B8G8R8A8_UNORM;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.components = { 
			VK_COMPONENT_SWIZZLE_IDENTITY
		},
		assert(!vkCreateImageView(Graphics.Device, &view, nullptr, &Graphics.InlineTexture.View));
	}

	if(!Graphics.InlineTexture.Frame) {
		VkImageView attachments[2];
		attachments[0] = Graphics.InlineTexture.View;
		attachments[1] = Graphics.DepthView;
		const VkFramebufferCreateInfo infoFb =
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = Graphics.RenderPass,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.width = swapchainExtent.width,
			.height = swapchainExtent.height,
			.layers = 1
		};
		
		assert(!vkCreateFramebuffer(Graphics.Device, &infoFb, nullptr, &Graphics.InlineTexture.Frame));
	}

	// Создаём экранные буферы глубины, связанные с одним кадровым буфером глубины
	VkImageView attachments[2];
	attachments[1] = Graphics.DepthView;

	const VkFramebufferCreateInfo infoFb =
	{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = Graphics.RenderPass,
		.attachmentCount = 2,
		.pAttachments = attachments,
		.width = swapchainExtent.width,
		.height = swapchainExtent.height,
		.layers = 1
	};

	for (uint32_t iter = 0; iter < Graphics.DrawBufferCount; iter++)
	{
		attachments[0] = Graphics.DrawBuffers[iter].View;
		assert(!vkCreateFramebuffer(Graphics.Device, &infoFb, nullptr, &Graphics.DrawBuffers[iter].FrameBuffer));
	}

	// Передача информации о количестве сменных буферов в ImGui
	#ifdef HAS_IMGUI
	if(ImGui::GetCurrentContext())
		ImGui_ImplVulkan_SetMinImageCount(Graphics.DrawBufferCount);
	#endif

	Graphics.SwapchainChangeReport = report.str();
	flushCommandBufferData();
}

void Vulkan::glfwCallbackOnResize(GLFWwindow *window, int width, int height)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);

	if(handler)
	{
		handler->Screen.Width = width;
		handler->Screen.Height = height;
		handler->freeSwapchains();
		handler->buildSwapchains();

		if(handler->Game.Session)
			handler->Game.Session->onResize(width, height);
	}
}

void Vulkan::glfwCallbackOnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);

	if(handler->Game.Session)
		handler->Game.Session->onCursorBtn((ISurfaceEventListener::EnumCursorBtn) button, action);
}

void Vulkan::glfwCallbackOnCursorPos(GLFWwindow* window, double xpos, double ypos)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);

	if(handler->Game.Session) {
		ServerSession &sobj = *handler->Game.Session;
		if(sobj.CursorMode == ISurfaceEventListener::EnumCursorMoveMode::Default) {
			sobj.onCursorPosChange((int32_t) xpos, (int32_t) ypos);
		} else {
			glfwSetCursorPos(handler->Graphics.Window, handler->Screen.Width/2., handler->Screen.Height/2.);
			sobj.onCursorMove(xpos-handler->Screen.Width/2., handler->Screen.Height/2.-ypos);
		}
	}
}

void Vulkan::glfwCallbackOnScale(GLFWwindow* window, float xscale, float yscale)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);

}

void Vulkan::glfwCallbackOnKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);
	if(handler->Game.Session)
		handler->Game.Session->onKeyboardBtn(key, action);
}

void Vulkan::glfwCallbackOnFocus(GLFWwindow* window, int focused)
{
	Vulkan *handler = (Vulkan*) glfwGetWindowUserPointer(window);
	if(handler->Game.Session)
		handler->Game.Session->onChangeFocusState(focused);
}

void Vulkan::checkLibrary()
{
	if(LibraryVulkan)
		return;

	std::stringstream error;

	try {
		// Подгружаем библиотеку
#ifdef _WIN32
		LibraryVulkan.emplace("vulkan-1");
#else
		LibraryVulkan.emplace("libvulkan.so.1");
#endif

		// Подгружаем символы
		try { LoadSymbolsVulkan(*LibraryVulkan); }
		catch(const std::exception &exc) {
			error << "Ошибка загрузки символов библиотеки Vulkan:\n" << exc.what() << '\n';
			goto onError;
		}

		// Инициализация GLFW
		if(!glfwInit())
		{
			const char *error_msg;
			int err_code = glfwGetError(&error_msg);
			error << "Не удалось инициализировать GLFW (glfwInit). Код: " << err_code << ". Ошибка: " << error_msg << '\n';
			goto onError;
		}

		// Дополнительный тест GLFW на присутствие
		// минимального функционала Vulkan ICD
		if (!glfwVulkanSupported())
		{
			const char *error_msg;
			int err_code = glfwGetError(&error_msg);
			error << "GLFW сообщает об отсутствии минимально необходимого функционала Vulkan ICD (glfwVulkanSupported), код: " << err_code << ", ошибка: " << error_msg << '\n';
			// Просто уведомляем, не создаём ошибку
		}

		// Загрузим доступные слои
		{
			uint32_t count = -1;

			assert(!vkEnumerateInstanceLayerProperties(&count, nullptr));
			assert(count != -1);

			if(count)
			{
				std::vector<VkLayerProperties> layerProperties(count);
				assert(!vkEnumerateInstanceLayerProperties(&count, layerProperties.data()));

				Graphics.InstanceLayers.resize(count);

				auto *ptrFrom = layerProperties.data();
				auto *ptrTo = Graphics.InstanceLayers.data();
				for(uint32_t iter = 0; iter < count; iter++, ptrFrom++, ptrTo++)
				{
					ptrTo->LayerName = ptrFrom->layerName;
					ptrTo->SpecVersion = ptrFrom->specVersion;
					ptrTo->ImplementationVersion = ptrFrom->implementationVersion;
					ptrTo->Description = ptrFrom->description;
				}
			} else
				Graphics.InstanceLayers.clear();
		}

		// Загрузим доступные расширения
		{
			uint32_t count = -1;

			assert(!vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
			assert(count != -1);

			if(count)
			{
				std::vector<VkExtensionProperties> extensionProperties(count);
				assert(!vkEnumerateInstanceExtensionProperties(nullptr, &count, extensionProperties.data()));

				Graphics.InstanceExtensions.resize(count);

				auto *ptrFrom = extensionProperties.data();
				auto *ptrTo = Graphics.InstanceExtensions.data();
				for(uint32_t iter = 0; iter < count; iter++, ptrFrom++, ptrTo++)
				{
					ptrTo->ExtensionName = ptrFrom->extensionName;
					ptrTo->SpecVersion = ptrFrom->specVersion;
				}
			} else
				Graphics.InstanceExtensions.clear();
		}

		// Загрузим расширения, необходимые GLFW
		{
			uint32_t count = -1;
			const char **extensionsRequired = glfwGetRequiredInstanceExtensions(&count);

			if(!extensionsRequired)
			{
				error << "Функция glfwGetRequiredInstanceExtensions не предоставила необходимые расширения\n";
				goto onError;
			}

			assert(count != -1);

			if(count)
			{
				Graphics.GLFWExtensions.resize(count);
				auto *ptrFrom = extensionsRequired;
				auto *ptrTo = Graphics.GLFWExtensions.data();
				for(uint32_t iter = 0; iter < count; iter++, ptrFrom++, ptrTo++)
					*ptrTo = *ptrFrom;
			} else
				Graphics.GLFWExtensions.clear();
		}

		// Создадим instance для запроса информации о доступных
		// устройствах и их характеристиках
		vkInstance localInstance(this);
		std::vector<VkPhysicalDevice> devices;
		{
			uint32_t count = -1;
			VkResult res;

			assert(!vkEnumeratePhysicalDevices(localInstance.getInstance(), &count, nullptr));

			if(!count)
			{
				error << "vkEnumeratePhysicalDevices сообщил об отсутствии подходящего устройства\n";
				goto onError;
			}

			devices.resize(count);
			assert(!vkEnumeratePhysicalDevices(localInstance.getInstance(), &count, devices.data()));
			Graphics.Devices.resize(count);

			// Перебор устройств
			for(size_t iter = 0; iter < count; iter++)
			{
				// Информация об устройстве
				VkPhysicalDeviceProperties deviceProperties;
				auto &device = Graphics.Devices[iter];
				vkGetPhysicalDeviceProperties(devices[iter], &deviceProperties);
				vkGetPhysicalDeviceFeatures(devices[iter], &device.DeviceFeatures);

				// Копируем характеристики устройства
				device.ApiVersion = deviceProperties.apiVersion;
				device.DriverVersion = deviceProperties.driverVersion;
				device.VendorID = deviceProperties.vendorID;
				device.DeviceID = deviceProperties.deviceID;
				device.DeviceType = deviceProperties.deviceType;
				device.DeviceName = deviceProperties.deviceName;
				device.Limits = deviceProperties.limits;
				device.SparseProperties = deviceProperties.sparseProperties;


				{	// Проверяем расширения
					uint32_t count = -1;
					res = vkEnumerateDeviceExtensionProperties(devices[iter], nullptr, &count, nullptr);
					if(res || count == -1)
					{
						error << "Не удалось запросить расширения устройства:\n" << device.getDeviceName() << '\n';
						goto next1;
					} else if(!count)
						goto next1;

					std::vector<VkExtensionProperties> extensions(count);
					vkEnumerateDeviceExtensionProperties(devices[iter], nullptr, &count, extensions.data());
					device.Extensions.resize(count);
					for(size_t extension = 0; extension < count; extension++)
					{
						device.Extensions[extension].ExtensionName = extensions[extension].extensionName;
						device.Extensions[extension].SpecVersion = extensions[extension].specVersion;
					}
				}

				next1:
				{ 	// Проверяем очереди
					uint32_t count = -1;
					vkGetPhysicalDeviceQueueFamilyProperties(devices[iter], &count, nullptr);
					if(count == -1)
					{
						error << "Не удалось запросить очереди устройства:\n" << device.getDeviceName() << '\n';
						goto next2;
					} else if(!count)
						goto next2;

					std::vector<VkQueueFamilyProperties> deviceQueueProperties(count);
					vkGetPhysicalDeviceQueueFamilyProperties(devices[iter], &count, deviceQueueProperties.data());

					device.FamilyProperties.resize(count);
					for(size_t family = 0; family < count; family++)
						device.FamilyProperties[family] = deviceQueueProperties[family];
				}

				next2:
				continue;
			}
		}

		// Создадим окно
		glfwDefaultWindowHints();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		Graphics.Window = glfwCreateWindow(Screen.Width, Screen.Height, "LuaVox", nullptr, nullptr);
		if (!Graphics.Window)
		{
			const char *error_msg;
			int err_code = glfwGetError(&error_msg);
			error << "Не удалось создать окно GLFW, код: "
					<< err_code << ", ошибка: " << error_msg << '\n';
			goto onError;
		}

		glfwSetWindowUserPointer(Graphics.Window, this);
		glfwSetFramebufferSizeCallback(Graphics.Window, &glfwCallbackOnResize);
		glfwSetMouseButtonCallback(Graphics.Window, &glfwCallbackOnMouseButton);
		glfwSetCursorPosCallback(Graphics.Window, &glfwCallbackOnCursorPos);
		glfwSetWindowContentScaleCallback(Graphics.Window, &glfwCallbackOnScale);
		glfwSetKeyCallback(Graphics.Window, &glfwCallbackOnKey);
		glfwSetWindowFocusCallback(Graphics.Window, &glfwCallbackOnFocus);

		VkResult res = glfwCreateWindowSurface(localInstance.getInstance(), Graphics.Window, nullptr, &Graphics.Surface);
		if(res)
		{
			error << "glfwCreateWindowSurface == " << res << '\n';
			goto onError;
		}

		// Определим наилучшие настройки
		std::vector<int32_t> scores;
		int32_t maxScore = 0;
		vkDevice *devicePtr = nullptr;
		uint32_t dpQueueSurface = -1, dpQueueGraphics = -1;

		scores.resize(Graphics.Devices.size());
		std::fill(scores.begin(), scores.end(), 0);
		std::stringstream report;
		report << "Отчёт о доступном оборудовании:\n";

		for(size_t iter = 0; iter < Graphics.Devices.size(); iter++)
		{
			auto &device = Graphics.Devices[iter];
			auto &score = scores[iter];

			report << std::string(48, '~') << "\n\n" << device.getDeviceName() << '\n';

			bool canContinue = true;

			// Необходимый функционал
			if(!device.DeviceFeatures.geometryShader)
			{
				report << "\t*geometryShader: "
				<< device.DeviceFeatures.imageCubeArray
				<< '\n';
				canContinue = false;
			}

			// Проверка наличия необходимых расширений
			for(auto ext : NeedExtensions)
				if(std::find_if(device.Extensions.begin(), device.Extensions.end(), [&](const vkDeviceExtension &obj) { return obj.ExtensionName == ext; }) == device.Extensions.end())
				{
					report << "\t*Расширение " << ext << ": отсутствует"
					<< '\n';
					canContinue = false;
				}

			// Дополнительный функционал
			if(!device.DeviceFeatures.fullDrawIndexUint32)
			{
				report << "\tfullDrawIndexUint32: "
				<< device.DeviceFeatures.fullDrawIndexUint32
				<< " : " << device.Limits.maxDrawIndexedIndexValue << '\n';
			} else
				score++;

			// Поиск очередей
			uint32_t queueSurface = -1, queueGraphics = -1;
			uint32_t value = false;

			for(size_t queue = 0; queue < device.FamilyProperties.size(); queue++)
			{
				vkGetPhysicalDeviceSurfaceSupportKHR(devices[iter], queue, Graphics.Surface, &value);

				if(queueSurface == -1 && value == VK_TRUE)
					queueSurface = queue;

				if(queueGraphics == -1 && (device.FamilyProperties[queue].queueFlags & VK_QUEUE_GRAPHICS_BIT))
					queueGraphics = queue;

				if(queueSurface != -1 && queueGraphics != -1)
					break;
			}

			report << "\nQueueSurface: ";
			if(queueSurface == -1)
			{
				report << "очередь не найдена\n";
				canContinue = false;
			} else
				report << queueSurface << '\n';

			report << "QueueGraphics: ";
			if(queueGraphics == -1)
			{
				report << "очередь не найдена\n";
				canContinue = false;
			} else
				report << queueGraphics << '\n';

			if(queueSurface != queueGraphics)
			{
				report << "Разные очереди для вывода и рендера не поддерживаются\n";
				canContinue = false;
			}

			if(!canContinue)
			{
				score = -1;
				report << "Устройство не подходит\n\n";
				continue;
			}

			// Ограничения
			//device.Limits.

			report << "\nОценка устройства: " << score << "\n\n";
			if(score > maxScore)
			{
				maxScore = score;
				devicePtr = &device;

				dpQueueSurface = queueSurface;
				dpQueueGraphics = queueGraphics;
			}
		}

		LOGGER.debug() << report.str() << std::string(48, '~');

		if(maxScore == 0 || !devicePtr)
		{
			error << "Не найдено подходящее устройство\n";
			goto onError;
		}

		// Заполняем наилучшие настройки
		SettingsBest = {{devicePtr->VendorID, devicePtr->DeviceID}, dpQueueGraphics, dpQueueSurface};

		// Удаляем surface окна по скольку instance будет пересоздан
		if(Graphics.Surface)
		{
			vkDestroySurfaceKHR(localInstance.getInstance(), Graphics.Surface, nullptr);
			Graphics.Surface = nullptr;
		}

		SettingsState = SettingsBest;

	} catch(const std::exception &exc) {
		error << "Отловлена глобальная ошибка\n";
		error << exc.what();
		goto onError;
	}

	if(error.tellp())
		LOGGER.warn() << "Ошибки во время инициализации графической подсистемы:\n" << error.str();

	return;

	onError:
	// ДеИнициализация
	try {
		if(Graphics.Window)
		{
			glfwDestroyWindow(Graphics.Window);
			Graphics.Window = nullptr;
		}
	} catch(const std::exception &exc)
	{
		error << "\nОшибки при деинициализации графической подсистемы:\n" << exc.what();
	}

	MAKE_ERROR("Vulkan: Ошибка инициализации графической подсистемы:\n" << error.str());
}

void Vulkan::initNextSettings()
{
	if(!SettingsNext.isValid())
		MAKE_ERROR("Невалидные настройки");

	if(needFullVulkanRebuild())
	{
		deInitVulkan();
	} else {
		for(const std::shared_ptr<IVulkanDependent> &dependent : ROS_Dependents)
		{
			if(/*dependent->needRebuild() != EnumRebuildType::None || */ dynamic_cast<Pipeline*>(dependent.get()))
				dependent->free(this);
		}

		freeSwapchains();
	}

	if(!Graphics.Instance)
	{
		std::vector<std::string_view> knownDebugLayers =
		{
			"VK_LAYER_GOOGLE_threading",
			"VK_LAYER_GOOGLE_unique_objects",
			"VK_LAYER_LUNARG_parameter_validation",
			"VK_LAYER_LUNARG_object_tracker",
			"VK_LAYER_LUNARG_image",
			"VK_LAYER_LUNARG_core_validation",
			"VK_LAYER_LUNARG_swapchain",
			"VK_LAYER_LUNARG_standard_validation",
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_monitor"
		};

		//if(!SettingsNext.Debug)
		//	knownDebugLayers.clear();

		std::vector<vkInstanceLayer> enableDebugLayers;

		for(auto &name : knownDebugLayers)
			for(const vkInstanceLayer &ext : Graphics.InstanceLayers)
				if(ext.LayerName == name)
				{
					enableDebugLayers.push_back(ext);
					break;
				}

		Graphics.Instance.emplace(this, enableDebugLayers);
	}

	if(!Graphics.Surface)
	{
		assert(Graphics.Window);
		VkResult res = glfwCreateWindowSurface(Graphics.Instance->getInstance(), Graphics.Window, nullptr, &Graphics.Surface);
		if(res)
			MAKE_ERROR("glfwCreateWindowSurface: " << res);
	}

	// Найдём PhysicalDevice
	if(!Graphics.PhysicalDevice) //Graphics.PhysicalDevice = nullptr;
	{
		uint32_t count = -1;
		VkResult res;

		assert(!vkEnumeratePhysicalDevices(Graphics.Instance->getInstance(), &count, nullptr));
		if(!count)
			MAKE_ERROR("vkEnumeratePhysicalDevices сообщил об отсутствии подходящего устройства");

		std::vector<VkPhysicalDevice> devices(count);
		assert(!vkEnumeratePhysicalDevices(Graphics.Instance->getInstance(), &count, devices.data()));
		for(size_t iter = 0; iter < count; iter++)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(devices[iter], &deviceProperties);

			if(deviceProperties.vendorID == SettingsNext.DeviceMain.VendorId
					&& deviceProperties.deviceID == SettingsNext.DeviceMain.DeviceId)
			{
				Graphics.PhysicalDevice = devices[iter];
				break;
			}
		}
	}

	if(!Graphics.PhysicalDevice)
		MAKE_ERROR("Требуемое устройство не найдено, VendorId: "
			<< SettingsNext.DeviceMain.VendorId << ", DeviceId: "
			<< SettingsNext.DeviceMain.DeviceId);

	// Создаём виртуальное устройство
	// TODO: добавить поддержку разделённых очередей
	if(!Graphics.Device)
	{
		float queue_priorities[1] = { 0.0 };
		const VkDeviceQueueCreateInfo infoQueue =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = SettingsNext.QueueGraphics,
			.queueCount = 1,
			.pQueuePriorities = queue_priorities
		};

		VkPhysicalDeviceVulkan11Features feat11;

		memset(&feat11, 0, sizeof(feat11));
		feat11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

		VkPhysicalDeviceFeatures2 features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &feat11,
			.features = {0}
		};

		features.features.geometryShader = true;

		feat11.uniformAndStorageBuffer16BitAccess = true;
		feat11.storageBuffer16BitAccess = true;

		VkDeviceCreateInfo infoDevice =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features,
			.flags = 0,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &infoQueue,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = (uint32_t) NeedExtensions.size(),
			.ppEnabledExtensionNames = (const char* const*) NeedExtensions.data(),
			.pEnabledFeatures = nullptr
		};

		assert(!vkCreateDevice(Graphics.PhysicalDevice, &infoDevice, nullptr, &Graphics.Device));
		vkGetDeviceQueue(Graphics.Device, SettingsNext.QueueGraphics, 0, &Graphics.DeviceQueueGraphic);
	}

	// Определяемся с форматом экранного буфера
	uint32_t count = -1;
	assert(!vkGetPhysicalDeviceSurfaceFormatsKHR(Graphics.PhysicalDevice, Graphics.Surface, &count, VK_NULL_HANDLE));
	std::vector<VkSurfaceFormatKHR> surfFormats(count);
	assert(!vkGetPhysicalDeviceSurfaceFormatsKHR(Graphics.PhysicalDevice, Graphics.Surface, &count, surfFormats.data()));
	assert(surfFormats.size());

	if (count == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
		Graphics.SurfaceFormat = VK_FORMAT_R32G32B32_UINT;
		Graphics.SurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		LOGGER.debug() << "Формат экранного буфера не определён устройством, используется: VK_FORMAT_B8G8R8A8_UNORM & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";

	} else {
		assert(count >= 1 && "Отсутствуют подходящие форматы экранного буфера vkGetPhysicalDeviceSurfaceFormatsKHR");

		bool find = false;
		for(size_t iter = 0; iter < count; iter++)
			if(surfFormats[iter].format == VK_FORMAT_B8G8R8A8_UNORM
					&& surfFormats[iter].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				Graphics.SurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
				Graphics.SurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				find = true;
				LOGGER.debug() << "Формат экранного буфера по умолчанию: VK_FORMAT_B8G8R8A8_UNORM & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
				break;
			}

		if(!find)
		{
			LOGGER.debug() << "Не найден формат экранного буфера VK_FORMAT_B8G8R8A8_UNORM & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, используется "
					<< int(surfFormats[0].format) << " & " << int(surfFormats[0].colorSpace);
			Graphics.SurfaceFormat = surfFormats[0].format;
			Graphics.SurfaceColorSpace = surfFormats[0].colorSpace;
		}
	}

	// Запрос настроек памяти для данного устройства
	vkGetPhysicalDeviceMemoryProperties(Graphics.PhysicalDevice, &Graphics.DeviceMemoryProperties);

	// Создание пула команд (видимо для одной очереди)
	if(!Graphics.Pool)
	{
		const VkCommandPoolCreateInfo infoCmdPool =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = SettingsNext.QueueGraphics
		};

		assert(!vkCreateCommandPool(Graphics.Device, &infoCmdPool, nullptr, &Graphics.Pool));
	}

	// Создание буферов команд (подготовка данных CommandBufferData, рендер CommandBufferRender)
	{
		const VkCommandBufferAllocateInfo infoCmd =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = Graphics.Pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		if(!Graphics.CommandBufferData)
		{
			assert(!vkAllocateCommandBuffers(Graphics.Device, &infoCmd, &Graphics.CommandBufferData));

			// Старт очереди команд
			VkCommandBufferBeginInfo infoCmdBuffer =
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr
			};

			assert(!vkBeginCommandBuffer(Graphics.CommandBufferData, &infoCmdBuffer));
		}

		if(!Graphics.CommandBufferRender)
			assert(!vkAllocateCommandBuffers(Graphics.Device, &infoCmd, &Graphics.CommandBufferRender));
	}

	// Создание RenderPass для экранного буфера
	if(!Graphics.RenderPass)
	{
		const VkAttachmentDescription attachments[2] =
		{
			{
				.flags = 0,
				.format = Graphics.SurfaceFormat,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			}, {
				.flags = 0,
				.format = Graphics.DepthFormat,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			}
		};

		const VkAttachmentReference referenceColor1 =
		{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		const VkAttachmentReference referenceDepth =
		{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};

		const VkSubpassDescription subpass[1] =
		{
			{
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = nullptr,
				.colorAttachmentCount = 1,
				.pColorAttachments = &referenceColor1,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = &referenceDepth,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr
			}
		};

		// Зависимости между подпроходами
		// const VkSubpassDependency rp_info_dep[2] = 
		// {
		// 	{
		// 		.srcSubpass = VK_SUBPASS_EXTERNAL,
		// 		.dstSubpass = 0,
		// 		.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		// 		.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		// 		.srcAccessMask = 0,
		// 		.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		// 						VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		// 						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		// 						VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		// 						VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		// 		.dependencyFlags = 0
		// 	},
		// 	{
		// 		.srcSubpass = 0,
		// 		.dstSubpass = 1,
		// 		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		// 		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		// 		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		// 		.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		// 		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		// 	},
		// };

		const VkRenderPassCreateInfo rp_info =
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.subpassCount = 1,
			.pSubpasses = subpass,
			.dependencyCount = 0,
			.pDependencies = nullptr
		};

		assert(!vkCreateRenderPass(Graphics.Device, &rp_info, nullptr, &Graphics.RenderPass));
	}

	// Цепочки рендера
	buildSwapchains();

	// Пул дескрипторов для ImGui
	#ifdef HAS_IMGUI
	if(!Graphics.ImGuiDescPool)
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1024 }
		};

		VkDescriptorPoolCreateInfo descriptor_pool = {};
		descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptor_pool.maxSets = 1024;
		descriptor_pool.poolSizeCount = (uint32_t) IM_ARRAYSIZE(pool_sizes);
		descriptor_pool.pPoolSizes = pool_sizes;

		assert(!vkCreateDescriptorPool(Graphics.Device, &descriptor_pool, nullptr, &Graphics.ImGuiDescPool));

		ImGui::CreateContext();

		ImGui::StyleColorsDark();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		
		ImGui_ImplGlfw_InitForVulkan(Graphics.Window, true);

		ImGui_ImplVulkan_InitInfo ImGuiInfo = 
		{
			.Instance = Graphics.Instance->getInstance(),
			.PhysicalDevice = Graphics.PhysicalDevice,
			.Device = Graphics.Device,
			.QueueFamily = SettingsNext.QueueGraphics,
			.Queue = Graphics.DeviceQueueGraphic,
			.DescriptorPool = Graphics.ImGuiDescPool,
			.RenderPass = Graphics.RenderPass,
			.MinImageCount = Graphics.DrawBufferCount,
			.ImageCount = Graphics.DrawBufferCount,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.PipelineCache = nullptr,
			.Subpass = 0,
			.UseDynamicRendering = false,
			.PipelineRenderingCreateInfo = {},
			.Allocator= nullptr,
			.CheckVkResultFn = check_vk_result,
			.MinAllocationSize = 1024*1024
		};
		
		ImGui_ImplVulkan_Init(&ImGuiInfo);

		// ImFontConfig fontConfig;
		// fontConfig.MergeMode = false;
		// fontConfig.PixelSnapH = true;
		// fontConfig.OversampleH = 1;
		// fontConfig.OversampleV = 1;

		// Buffer<byte> fontFile = File("assets/client/fonts/UZSans-Bold.ttf").openReader()->readAsBuffer();
		// byte *fontPtr = new byte[fontFile.getSize()];
		// std::copy(fontFile.getData(), fontFile.getDataBack()+1, fontPtr);
		// try{
		// 	io.Fonts->AddFontFromMemoryTTF(fontPtr, fontFile.getSize(), 16.0f, &fontConfig, io.Fonts->GetGlyphRangesCyrillic());
		// } catch(...) {
		// 	delete[] fontPtr;
		// 	throw;
		// }
		// //io.Fonts->AddFontFromFileTTF("assets/client/fonts/UZSans-Bold.ttf", 16.0f, &fontConfig, io.Fonts->GetGlyphRangesCyrillic());
		// //io.Fonts->AddFontDefault(&fontConfig);

		// ImGui_ImplVulkan_CreateFontsTexture(CBSetup);
		// flushInitCMD();
		// ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	#endif

	for(const std::shared_ptr<IVulkanDependent> &dependent : ROS_Dependents)
		dependent->init(this);

	LOGGER.debug() << Graphics.SwapchainChangeReport;
	Graphics.SwapchainChangeReport = "";

	SettingsState = SettingsNext;
}

void Vulkan::deInitVulkan()
{
	#ifdef HAS_IMGUI
	if(ImGui::GetCurrentContext())
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	#endif

	for(std::shared_ptr<IVulkanDependent> dependent : ROS_Dependents)
		dependent->free(this);

	freeSwapchains();

	{
		std::lock_guard locK(Screen.BeforeDrawMtx);
		while(!Screen.BeforeDraw.empty())
		{
			Screen.BeforeDraw.front()(this);
			Screen.BeforeDraw.pop();
		}
	}

	if(Graphics.Instance)
	{
		if(Graphics.Device)
		{
			if(Graphics.ImGuiDescPool)
				vkDestroyDescriptorPool(Graphics.Device, Graphics.ImGuiDescPool, nullptr);

			// Удаляем SwapChain
			if(Graphics.Swapchain)
				vkDestroySwapchainKHR(Graphics.Device, Graphics.Swapchain, nullptr);

			// Очистка буферов команд
			if(Graphics.CommandBufferData)
				vkFreeCommandBuffers(Graphics.Device, Graphics.Pool, 1, &Graphics.CommandBufferData);

			if(Graphics.CommandBufferRender)
				vkFreeCommandBuffers(Graphics.Device, Graphics.Pool, 1, &Graphics.CommandBufferRender);

			// Освобождение пула команд
			if(Graphics.Pool)
				vkDestroyCommandPool(Graphics.Device, Graphics.Pool, nullptr);

			// Удаляем RenderPass
			if(Graphics.RenderPass)
				vkDestroyRenderPass(Graphics.Device, Graphics.RenderPass, nullptr);

			// Освобождение виртуального устройства
			//if(Graphics.Device)
			//	vkDestroyDevice(Graphics.Device, nullptr);
		}

		if(Graphics.Surface)
			vkDestroySurfaceKHR(Graphics.Instance->getInstance(), Graphics.Surface, nullptr);
	}

	Graphics.ImGuiDescPool = nullptr;
	Graphics.Swapchain = nullptr;
	Graphics.CommandBufferData = nullptr;
	Graphics.CommandBufferRender = nullptr;
	Graphics.Pool = nullptr;
	Graphics.RenderPass = nullptr;
	Graphics.Device = nullptr;
	Graphics.PhysicalDevice = nullptr;
	Graphics.DeviceQueueGraphic = nullptr;
	Graphics.Surface = nullptr;
	Graphics.Instance.reset();
}

void Vulkan::flushCommandBufferData()
{
	assert(!vkEndCommandBuffer(Graphics.CommandBufferData));

	const VkCommandBuffer cmd_bufs[] = { Graphics.CommandBufferData };
	VkFence nullFence = { VK_NULL_HANDLE };
	VkSubmitInfo submit_info =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = 1,
		.pCommandBuffers = cmd_bufs,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr
	};

	assert(!vkQueueSubmit(Graphics.DeviceQueueGraphic, 1, &submit_info, nullFence));
	assert(!vkQueueWaitIdle(Graphics.DeviceQueueGraphic));

	VkCommandBufferBeginInfo infoCmdBuffer =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	assert(!vkBeginCommandBuffer(Graphics.CommandBufferData, &infoCmdBuffer));
}

Settings Vulkan::getBestSettings()
{
	checkLibrary();
	return SettingsBest;
}

void Vulkan::reInit()
{
	checkLibrary();
	initNextSettings();
    addImGUIFont(LV::getResource("default.ttf")->makeView());
}

bool Vulkan::needFullVulkanRebuild()
{
	return false;
}

std::shared_ptr<ShaderModule> Vulkan::createShader(std::string_view data)
{
	assert(Graphics.Device);
	assert(data.size());
	std::shared_ptr<ShaderModule> module = std::make_shared<ShaderModule>(data);
	std::dynamic_pointer_cast<IVulkanDependent>(module)->init(this);
	ROS_Dependents.insert(module);
	return module;
}

void Vulkan::registerDependent(std::shared_ptr<IVulkanDependent> dependent)
{
	assert(Graphics.Device);
	dependent->init(this);
	ROS_Dependents.insert(dependent);
}

Vulkan& Vulkan::operator<<(std::shared_ptr<IVulkanDependent> dependent)
{
	registerDependent(dependent);
	return *this;
}

void Vulkan::addImGUIFont(std::string_view view) {
	ImFontConfig fontConfig;
	fontConfig.MergeMode = false;
	fontConfig.PixelSnapH = true;
	fontConfig.OversampleH = 1;
	fontConfig.OversampleV = 1;

	auto &io = ImGui::GetIO();
	uint8_t *fontPtr = (uint8_t*) malloc(view.size());
	if(!fontPtr)
		MAKE_ERROR("Not enough memory");

	std::copy(view.begin(), view.end(), fontPtr);
	try{
		io.Fonts->AddFontFromMemoryTTF(fontPtr, view.size(), 16.0f, &fontConfig, io.Fonts->GetGlyphRangesCyrillic());
	} catch(...) {
		free(fontPtr);
		throw;
	}
}

void Vulkan::gui_MainMenu() {
	if(!ImGui::Begin("MainMenu", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
		return;

	static struct {
		char Address[256] = "localhost", Username[256], Password[256];
		bool Cancel = false, InProgress = false;
		std::string Progress;

		std::unique_ptr<Net::AsyncSocket> Socket;

		coro<> connect(asio::io_context &ioc) {
			try {
				std::string a(Address, strlen(Address));
				std::string u(Username, strlen(Username));
				std::string p(Password, strlen(Password));

				tcp::socket sock = co_await Net::asyncConnectTo(a, [&](const std::string &text) {
					Progress += text;
				});

				co_await Client::ServerSession::asyncAuthorizeWithServer(sock, u, p, 1, [&](const std::string &text) {
					Progress += text;
				});
				
				Socket = co_await Client::ServerSession::asyncInitGameProtocol(ioc, std::move(sock), [&](const std::string &text) {
					Progress += text;
				});
			} catch(const std::exception &exc) {
				Progress += "\n-> ";
				Progress += exc.what();
			}

			InProgress = false;

			co_return;
		}
	} ConnectionProgress;

	ImGui::InputText("Address", ConnectionProgress.Address, sizeof(ConnectionProgress.Address));
	ImGui::InputText("Username", ConnectionProgress.Username, sizeof(ConnectionProgress.Username));
	ImGui::InputText("Password", ConnectionProgress.Password, sizeof(ConnectionProgress.Password), ImGuiInputTextFlags_Password);
	
	if(!ConnectionProgress.InProgress && !ConnectionProgress.Socket) {
		if(ImGui::Button("Подключиться")) {
			ConnectionProgress.InProgress = true;
			ConnectionProgress.Cancel = false;
			ConnectionProgress.Progress.clear();
			co_spawn(ConnectionProgress.connect(IOC));
		} 
		
		if(!Game.Server) {
			if(ImGui::Button("Запустить сервер")) {
				try {
					Game.Server = std::make_unique<ServerObj>(IOC);
					ConnectionProgress.Progress = "Сервер запущен на порту " + std::to_string(Game.Server->LS.getPort());
				} catch(const std::exception &exc) {
					ConnectionProgress.Progress = "Не удалось запустить внутренний сервер: " + std::string(exc.what());
				}
			}
		} else {
			if(!Game.Server->GS.isAlive())
				Game.Server = nullptr;
			else if(ImGui::Button("Остановить сервер")) {
				Game.Server->GS.shutdown("Сервер останавливается по запросу интерфейса");
			}
		}
	}

	if(ConnectionProgress.InProgress) {
		if(ImGui::Button("Отмена")) 
			ConnectionProgress.Cancel = true;
	}
	
	if(!ConnectionProgress.Progress.empty()) {
		if(ImGui::BeginChild("Прогресс", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar)) {
			ImGui::Text("Прогресс:\n%s", ConnectionProgress.Progress.c_str());
			ImGui::EndChild();
		}
	}

	if(ConnectionProgress.Socket) {
		std::unique_ptr<Net::AsyncSocket> sock = std::move(ConnectionProgress.Socket);
		Game.RSession = std::make_unique<VulkanRenderSession>();
		*this << Game.RSession;
		Game.Session = std::make_unique<ServerSession>(IOC, std::move(sock), Game.RSession.get());
		Game.RSession->setServerSession(Game.Session.get());
		Game.ImGuiInterfaces.push_back(&Vulkan::gui_ConnectedToServer);
	}


	ImGui::End();
}

void Vulkan::gui_ConnectedToServer() {
	if(Game.Session) {
		if(Game.Session->isConnected())
			return;

		Game.RSession = nullptr;
		Game.Session = nullptr;
		Game.ImGuiInterfaces.pop_back();
	}
}


IVulkanDependent::~IVulkanDependent() = default;

void Vulkan::updateResources()
{
	// for(const std::shared_ptr<IVulkanDependent> &dependent : ROS_Dependents)
	// {
	// 	if(dependent->needRebuild() != EnumRebuildType::None)
	// 	{
	// 		dependent->free(this);
	// 		dependent->init(this);
	// 	}
	// }
}


// --------------- vkInstance ---------------


Vulkan::vkInstance::vkInstance(Vulkan *handler, std::vector<vkInstanceLayer> layers, std::vector<vkInstanceExtension> extensions)
	: Handler(handler)
{
	assert(handler);
	//TOS_ASSERT(getShared(), "Должен быть Shared");

	size_t glfwExtensions = handler->Graphics.GLFWExtensions.size();
	std::vector<const char*> rawLayers(layers.size()), rawExtensions(extensions.size()+glfwExtensions);

	// Подготавливаем слои
	{
		const char **ptrTo = rawLayers.data();
		const vkInstanceLayer *ptrFrom = layers.data();

		for(size_t iter = 0; iter < layers.size(); iter++, ptrFrom++, ptrTo++)
			*ptrTo = ptrFrom->LayerName.c_str();
	}

	// Подготавливаем расширения
	{
		const char **ptrTo = rawExtensions.data();
		const vkInstanceExtension *ptrFrom = extensions.data();

		size_t iter = 0;
		for(iter = 0; iter < extensions.size(); iter++, ptrFrom++, ptrTo++)
			*ptrTo = ptrFrom->ExtensionName.c_str();

		for(; iter < glfwExtensions + extensions.size(); iter++, ptrTo++)
			*ptrTo = handler->Graphics.GLFWExtensions[iter-extensions.size()].c_str();
	}

	const VkApplicationInfo infoApp =
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "NFB_DUCKOUT",
		.applicationVersion = 1,
		.pEngineName = "NFB_DUCKOUT",
		.engineVersion = 1,
		.apiVersion = VK_API_VERSION_1_2
	};

	VkInstanceCreateInfo infoInstance =
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &infoApp,
		.enabledLayerCount = (uint32_t) rawLayers.size(),
		.ppEnabledLayerNames = (const char* const*) rawLayers.data(),
		.enabledExtensionCount = (uint32_t) rawExtensions.size(),
		.ppEnabledExtensionNames = (const char* const*) rawExtensions.data()
	};

	//if (portability_enumeration)
	//	inst_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

	VkResult err = vkCreateInstance(&infoInstance, nullptr, &Instance);
	if (err == VK_ERROR_INCOMPATIBLE_DRIVER)
		MAKE_ERROR("Не найден подходящий драйвер клиента Vulkan (ICD)");
	else if (err == VK_ERROR_LAYER_NOT_PRESENT)
		MAKE_ERROR("Не удалось найти требуемые слои библиотеки-клиента Vulkan (ICD)");
	else if (err == VK_ERROR_EXTENSION_NOT_PRESENT)
		MAKE_ERROR("Не удалось найти требуемые расширения библиотеки-клиента Vulkan (ICD)");
	else if (err)
		MAKE_ERROR("Не удалось создать VulkanInstance по неизвестной ошибке");
}

Vulkan::vkInstance::~vkInstance()
{
	if(!Instance)
		return;

	//vkDestroyInstance(Instance, nullptr);
}


// --------------- DescriptorLayout--------------

/*DescriptorLayout::DescriptorLayout()
{
	// Информация о дескрипторах
	ShaderLayoutBindings =
	{
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		}, {
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr
		}
	};

	Settings.ShaderPushConstants =
	{
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = 204 //uint32_t(sizeof(PipelineWorld.UBO))
		}
	};
}*/

DescriptorLayout::DescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding> &layout, const std::vector<VkPushConstantRange> &pushConstants)
	: ShaderLayoutBindings(layout), ShaderPushConstants(pushConstants)
{}

DescriptorLayout::~DescriptorLayout() = default;

void DescriptorLayout::init(Vulkan *instance)
{
	if(!DescLayout)
	{
		const VkDescriptorSetLayoutCreateInfo descriptor_layout =
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t) ShaderLayoutBindings.size(),
			.pBindings = ShaderLayoutBindings.data()
		};

		assert(!vkCreateDescriptorSetLayout(instance->Graphics.Device, &descriptor_layout, nullptr, &DescLayout));
	}

	if(!Layout)
	{
		// Дескрипторы в шейдере
		const VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &DescLayout,
			.pushConstantRangeCount = (uint32_t) ShaderPushConstants.size(),
			.pPushConstantRanges = ShaderPushConstants.data()
		};

		assert(!vkCreatePipelineLayout(instance->Graphics.Device, &pPipelineLayoutCreateInfo, nullptr, &Layout));
	}
}

void DescriptorLayout::free(Vulkan *instance)
{
	if(instance->Graphics.Device)
	{
		if(DescLayout)
			vkDestroyDescriptorSetLayout(instance->Graphics.Device, DescLayout, nullptr);

		if(Layout)
			vkDestroyPipelineLayout(instance->Graphics.Device, Layout, nullptr);
	} 

	DescLayout = nullptr;
	Layout = nullptr;
}



// --------------- Pipeline ---------------



Pipeline::Pipeline(std::shared_ptr<DescriptorLayout> layout)
{
	assert(layout);
	Settings.ShaderLayoutBindings = layout;

	// Информация о буферах и размере вершины
	Settings.ShaderVertexBindings =
	{
		{
			.binding = 0,
			.stride = sizeof(uint32_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		}
	};

	// Парсинг данных из буферов
	Settings.ShaderVertexAttribute =
	{
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32_UINT,
			.offset = 0
		}
	};

	Settings.ShaderStages =
	{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = nullptr, //vertexShader.getModule(),
			.pName = "main",
			.pSpecializationInfo = nullptr
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = nullptr, //fragmentShader.getModule(),
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};

	Settings.Rasterization = VkPipelineRasterizationStateCreateInfo
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

	Settings.Multisample = VkPipelineMultisampleStateCreateInfo
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

	Settings.ColorBlend =
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

	for(int i = 0; i < 4; i++)
		Settings.ColorBlendConstants[i] = 0.0f;
	Settings.ColorBlendLogicOp = VK_LOGIC_OP_CLEAR;

	Settings.DepthStencil = VkPipelineDepthStencilStateCreateInfo
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
		.back = Settings.DepthStencil.front,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 0.0f
	};

	Settings.DepthStencil.back = Settings.DepthStencil.front;

	Settings.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
}

Pipeline::~Pipeline() = default;

void Pipeline::free(Vulkan *instance)
{
	if(instance && instance->Graphics.Device)
	{
		if(PipelineObj)
			vkDestroyPipeline(instance->Graphics.Device, PipelineObj, nullptr);
	}

	PipelineObj = nullptr;
}

void Pipeline::init(Vulkan *instance)
{
	assert(instance);
	assert(Settings.ShaderLayoutBindings && Settings.ShaderLayoutBindings->DescLayout);

	// Топология вершин на входе (треугольники, линии, точки)
	VkPipelineInputAssemblyStateCreateInfo ia =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = Topology,
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

	// Настройки конвейера, которые могут быть изменены без пересоздания конвейера
	VkPipelineDynamicStateCreateInfo dynamicState =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.dynamicStateCount = (uint32_t) Settings.DynamicStates.size(),
		.pDynamicStates = Settings.DynamicStates.data(),
	};

	// Логика смешивания цветов
	VkPipelineColorBlendStateCreateInfo cb =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = (VkBool32) (bool) Settings.ColorBlendLogicOp,
		.logicOp = Settings.ColorBlendLogicOp,
		.attachmentCount = (uint32_t) Settings.ColorBlend.size(),
		.pAttachments = Settings.ColorBlend.data(),
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	std::copy(Settings.ColorBlendConstants, Settings.ColorBlendConstants + 4, cb.blendConstants);

	// Вершины шейдера
	VkPipelineVertexInputStateCreateInfo createInfoVertexInput =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = (uint32_t) Settings.ShaderVertexBindings.size(),
		.pVertexBindingDescriptions = Settings.ShaderVertexBindings.data(),
		.vertexAttributeDescriptionCount = (uint32_t) Settings.ShaderVertexAttribute.size(),
		.pVertexAttributeDescriptions = Settings.ShaderVertexAttribute.data()
	};

	for(auto &obj : Settings.ShaderStages)
		assert(obj.module && "Шейдер не назначен");

	VkGraphicsPipelineCreateInfo pipeline =
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = (uint32_t) Settings.ShaderStages.size(),
		.pStages = Settings.ShaderStages.data(),
		.pVertexInputState = &createInfoVertexInput,
		.pInputAssemblyState = &ia,
		.pTessellationState = nullptr,
		.pViewportState = &vp,
		.pRasterizationState = &Settings.Rasterization,
		.pMultisampleState = &Settings.Multisample,
		.pDepthStencilState = &Settings.DepthStencil,
		.pColorBlendState = &cb,
		.pDynamicState = &dynamicState,
		.layout = *Settings.ShaderLayoutBindings,
		.renderPass = instance->Graphics.RenderPass,
		.subpass = Settings.Subpass,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};

	VkPipelineCacheCreateInfo infoPipelineCache;
	memset(&infoPipelineCache, 0, sizeof(infoPipelineCache));
	infoPipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	assert(!vkCreateGraphicsPipelines(instance->Graphics.Device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &PipelineObj));
}


// Shader


ShaderModule::ShaderModule(std::string_view view)
	: Source(view)
{}

void ShaderModule::free(Vulkan *instance)
{
	if(Module && instance->Graphics.Device)
		vkDestroyShaderModule(instance->Graphics.Device, Module, nullptr);

	Module = nullptr;
}

void ShaderModule::init(Vulkan *instance)
{
	if(!Module)
	{
		VkShaderModuleCreateInfo moduleCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.codeSize = Source.size(),
			.pCode = (const uint32_t*) Source.data()
		};

		if(vkCreateShaderModule(instance->Graphics.Device, &moduleCreateInfo, nullptr, &Module))
			MAKE_ERROR("VkHandler: Ошибка загрузки шейдера для устройства " << (void*) instance->Graphics.Device);
	}
}

ShaderModule::~ShaderModule() = default;


Buffer::Buffer(Vulkan *instance, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
	: Instance(instance)
{
	assert(instance);
	assert(instance->Graphics.Device);

	const VkBufferCreateInfo buf_info =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = std::max<uint32_t>(bufferSize, 1),
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr
	};

	Size = bufferSize;

	VkResult res = vkCreateBuffer(instance->Graphics.Device, &buf_info, nullptr, &Buff);
	if(res)
		MAKE_ERROR("Vulkan: ошибка создания буфера");

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(instance->Graphics.Device, Buff, &memReqs);

	VkMemoryAllocateInfo memAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = 0
	};

	memAlloc.memoryTypeIndex = instance->memoryTypeFromProperties(memReqs.memoryTypeBits, flags);

	vkAllocateMemory(instance->Graphics.Device, &memAlloc, nullptr, &Memory);
	if(res)
		MAKE_ERROR("VkHandler: ошибка выделения памяти на устройстве");

	vkBindBufferMemory(instance->Graphics.Device, Buff, Memory, 0);
}

Buffer::~Buffer()
{
	if(Instance && Instance->Graphics.Device)
	{
		Instance->beforeDraw([buff = Buff, memory = Memory](Vulkan *instance)
		{
			if(buff) 
				vkDestroyBuffer(instance->Graphics.Device, buff, nullptr);
			if(memory) 
				vkFreeMemory(instance->Graphics.Device, memory, nullptr);
		});
	}
}

Buffer::Buffer(Buffer &&obj)
{
	Instance 	= obj.Instance;
	Buff 		= obj.Buff;
	Memory 		= obj.Memory;
	Size 		= obj.Size;

	obj.Instance = nullptr;
}

Buffer& Buffer::operator=(Buffer &&obj)
{
	std::swap(Instance, obj.Instance);
	std::swap(Buff, obj.Buff);
	std::swap(Memory, obj.Memory);
	std::swap(Size, obj.Size);
	return *this;
}

uint8_t* Buffer::mapMemory(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) const
{
	void *data;
	if(vkMapMemory(Instance->Graphics.Device, Memory, offset, size, flags, &data))
		MAKE_ERROR("Ошибка выделения памяти устройства на хосте;");

	return (uint8_t*) data;
}

void Buffer::unMapMemory() const
{
	vkUnmapMemory(Instance->Graphics.Device, Memory);
}


CommandBuffer::CommandBuffer(Vulkan *instance)
	: Instance(instance)
{
	assert(instance);
	assert(instance->Graphics.Device);

	if(!instance->isRenderThread())
	{
		// Используем собственный пулл
		const VkCommandPoolCreateInfo infoCmdPool =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = instance->getSettings().QueueGraphics
		};

		assert(!vkCreateCommandPool(instance->Graphics.Device, &infoCmdPool, nullptr, &OffthreadPool));
	}

	assert(instance->Graphics.Pool);
	const VkCommandBufferAllocateInfo infoCmd =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = OffthreadPool ? OffthreadPool : instance->Graphics.Pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	assert(!vkAllocateCommandBuffers(instance->Graphics.Device, &infoCmd, &Buffer));
	
	VkCommandBufferBeginInfo infoCmdBuffer =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};
	
	assert(!vkBeginCommandBuffer(Buffer, &infoCmdBuffer));
}

CommandBuffer::~CommandBuffer()
{
	if(Buffer && Instance && Instance->Graphics.Device)
	{
		if(Instance->Graphics.DeviceQueueGraphic)
		{
			assert(!vkEndCommandBuffer(Buffer));
			const VkCommandBuffer cmd_bufs[] = { Buffer };
			VkFence nullFence = { VK_NULL_HANDLE };
			VkSubmitInfo submit_info =
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = 0,
				.pWaitSemaphores = nullptr,
				.pWaitDstStageMask = nullptr,
				.commandBufferCount = 1,
				.pCommandBuffers = cmd_bufs,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = nullptr
			};

			assert(!vkQueueSubmit(Instance->Graphics.DeviceQueueGraphic, 1, &submit_info, nullFence));
			assert(!vkQueueWaitIdle(Instance->Graphics.DeviceQueueGraphic));
			
			auto toExecute = std::move(AfterExecute);
			for(auto &iter : toExecute)
				try { iter(); } catch(const std::exception &exc) { Logger("CommandBuffer").error() << exc.what(); }
		}

		vkFreeCommandBuffers(Instance->Graphics.Device, OffthreadPool ? OffthreadPool : Instance->Graphics.Pool, 1, &Buffer);
	
		if(OffthreadPool)
			vkDestroyCommandPool(Instance->Graphics.Device, OffthreadPool, nullptr);
	}
}

void CommandBuffer::execute()
{
	assert(Instance->Graphics.DeviceQueueGraphic);
	assert(!vkEndCommandBuffer(Buffer));

	const VkCommandBuffer cmd_bufs[] = { Buffer };
	VkFence nullFence = { VK_NULL_HANDLE };
	VkSubmitInfo submit_info =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = 1,
		.pCommandBuffers = cmd_bufs,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr
	};

	assert(!vkQueueSubmit(Instance->Graphics.DeviceQueueGraphic, 1, &submit_info, nullFence));
	assert(!vkQueueWaitIdle(Instance->Graphics.DeviceQueueGraphic));
	VkCommandBufferBeginInfo infoCmdBuffer =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	assert(!vkBeginCommandBuffer(Buffer, &infoCmdBuffer));

	auto toExecute = std::move(AfterExecute);
	for(auto &iter : toExecute)
		iter();
}

void CommandBuffer::executeNoAwait()
{
	execute();
}

//VkSimpleImage
void SimpleImage::postInit(const ByteBuffer &pixels, size_t width, size_t height)
{
	CommandBuffer buffer(Instance);

	Width = width;
	Height = height;

	constexpr VkFormat tex_format = VK_FORMAT_B8G8R8A8_UNORM;
	ImageLayout = VK_IMAGE_LAYOUT_GENERAL; // То как будем использовать графический буфер, в данном случае как текстура
	VkFormatProperties props;

	vkGetPhysicalDeviceFormatProperties(Instance->Graphics.PhysicalDevice, tex_format, &props);

	VkImageCreateInfo infoImageCreate =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = tex_format,
		.extent =  { (uint32_t) width, (uint32_t) height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_MAX_ENUM,
		.usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = 0,
		.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
	};

	VkMemoryAllocateInfo memoryAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = 0,
		.memoryTypeIndex = 0
	};

	VkMemoryRequirements memoryReqs;
	const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

	VkImageMemoryBarrier infoImageMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = VK_NULL_HANDLE,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	/*
		Сначала загрузим текстуру на стороне хоста
		Создаём временную картинку
	*/

	VkImage tempImage;
	VkDeviceMemory tempMemory;

	infoImageCreate.tiling = VK_IMAGE_TILING_LINEAR;
	infoImageCreate.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	infoImageCreate.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &tempImage));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, tempImage, &memoryReqs);

	memoryAlloc.allocationSize = memoryReqs.size;
	memoryAlloc.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &tempMemory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, tempImage, tempMemory, 0));

	// Заполняем данными
	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(Instance->Graphics.Device, tempImage, &memorySubres, &layout);

	void *data;
	assert(!vkMapMemory(Instance->Graphics.Device, tempMemory, 0, memoryAlloc.allocationSize, 0, &data));

	for (size_t y = 0; y < Height; y++) {
		uint32_t *row = (uint32_t*) (((uint8_t*) data) + layout.rowPitch * y);
		uint32_t *color = ((uint32_t*) pixels.data()) + Width*y;
		for (size_t  x = 0; x < Width; x++, row++, color++)
			*row = *color;
	}

	vkUnmapMemory(Instance->Graphics.Device, tempMemory);

	// Создаём барьер, чтобы следующие команды выполнялись только после того 
	// как будут завершены операции по переносу данных с хоста к драйверу
	// И меняем layout на нужный
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = infoImageCreate.initialLayout;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Будет использоваться как источник данных
	infoImageMemoryBarrier.image = tempImage;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	// Формат конечной картинки
	if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
		infoImageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
	else if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
		infoImageCreate.tiling = VK_IMAGE_TILING_LINEAR;
	else
		/* Can't support VK_FORMAT_B8G8R8A8_UNORM */
		assert(!"No support for B8G8R8A8_UNORM as texture image format");

	/* Создаём конечную картинку */
	infoImageCreate.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &Image));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, Image, &memoryReqs);

	memoryAlloc.allocationSize = memoryReqs.size;
	memoryAlloc.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &Memory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, Image, Memory, 0));

	// Задаём нужный layout
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.image = Image;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	VkImageCopy copy_region =
	{
		.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.srcOffset = { 0, 0, 0 },
		.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.dstOffset = { 0, 0, 0 },
		.extent = { uint32_t(Width), uint32_t(Height), 1 },
	};

	vkCmdCopyImage(buffer, tempImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.newLayout = ImageLayout; // Используем как текстуру
	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	// Выполняем все команды
	buffer.execute();

	// И удаляем не нужную картинку
	vkDestroyImage(Instance->Graphics.Device, tempImage, nullptr);
	vkFreeMemory(Instance->Graphics.Device, tempMemory, nullptr);
	
	{
		// Способ чтения картинки
		const VkSamplerCreateInfo ciSampler =
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 1,
			.compareEnable = 0,
			.compareOp = VK_COMPARE_OP_NEVER,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			.unnormalizedCoordinates = VK_FALSE
		};

		assert(!vkCreateSampler(Instance->Graphics.Device, &ciSampler, nullptr, &Sampler));
	}

	{
		// Порядок пикселей и привязка к картинке
		VkImageViewCreateInfo ciView =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = tex_format,
			.components =
			{
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_A
			},
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		assert(!vkCreateImageView(Instance->Graphics.Device, &ciView, nullptr, &View));
	}
}

SimpleImage::SimpleImage(Vulkan *instance, std::filesystem::path filePNG)
{
	Instance = instance;
	size_t width, height;
	bool alpha;
	ByteBuffer pixels = loadTexture(filePNG, width, height, alpha);
	postInit(std::move(pixels), width, height);
}

ByteBuffer SimpleImage::loadTexture(std::filesystem::path file, size_t &width, size_t &height, bool &alpha)
{
	int _width, _height;
	ByteBuffer buff = loadPNG(std::ifstream(file), _width, _height, alpha, false);
	width = _width;
	height = _height;

	return buff;
}

SimpleImage::SimpleImage(Vulkan *instance, const ByteBuffer &pixels, size_t width, size_t height)
{
	Instance = instance;
	postInit(pixels, width, height);
}

SimpleImage::~SimpleImage()
{
	if(Instance && Instance->Graphics.Device)
	{
		if(Image) 
			vkDestroyImage(Instance->Graphics.Device, Image, nullptr);
		if(Memory) 
			vkFreeMemory(Instance->Graphics.Device, Memory, nullptr);
		if(Sampler) 
			vkDestroySampler(Instance->Graphics.Device, Sampler, nullptr);
		if(View) 
			vkDestroyImageView(Instance->Graphics.Device, View, nullptr);
	}
}

DynamicImage::DynamicImage(Vulkan *instance, uint32_t width, uint32_t height, const uint32_t *rgba)
	: Instance(instance)
{
	assert(instance);

	ImageLayout = VK_IMAGE_LAYOUT_GENERAL; // То как будем использовать графический буфер, в данном случае как текстура	

	
	// Способ чтения картинки
	const VkSamplerCreateInfo ciSampler =
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1,
		.compareEnable = 0,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		.unnormalizedCoordinates = VK_FALSE
	};

	changeSampler(&ciSampler);
	//TOS_ASSERT(!vkCreateSampler(Instance->Graphics.Device, sampler, nullptr, &Sampler), "vkCreateSampler");
	
	// При создании картинки вне потока графики, нужно дождаться, когда картинка будет создана
	if(!Instance->isRenderThread() && Instance->isAlive())
	{
		bool flag = false;
		std::exception_ptr exc;
		size_t start = Time::getSeconds();

		Instance->beforeDraw([obj = this, rgba, width, height, &exc, &flag, start](Vulkan *instance) {
			try {
				obj->recreateImage(width, height, rgba);
			} catch(...) {
				exc = std::current_exception();
			}

			if(Time::getSeconds() - start < 30)
				flag = true;
		});

		for(size_t timer = 0; timer < 31500000 && !flag; timer++)
			Time::sleep6(1);

		if(!flag)
			MAKE_ERROR("WatchDog: Превышено время ожидания (30 секунд) потока графики Vulkan");

		if(exc)
			std::rethrow_exception(exc);
	} else
		recreateImage(width, height, rgba);


	IsFirstCreate = false;
}

DynamicImage::~DynamicImage()
{
	if(Instance && Instance->Graphics.Device)
	{
		Instance->beforeDraw([image = Image, memory = Memory, sampler = Sampler, view = View](Vulkan *instance){
			if(image)
				vkDestroyImage(instance->Graphics.Device, image, nullptr);
			if(memory) 
				vkFreeMemory(instance->Graphics.Device, memory, nullptr);
			if(sampler)
				vkDestroySampler(instance->Graphics.Device, sampler, nullptr);
			if(view)
				vkDestroyImageView(instance->Graphics.Device, view, nullptr);
		});


		// if(Image)
		// 	vkDestroyImage(Instance->Graphics.Device, Image, nullptr);
		// if(Memory) 
		// 	vkFreeMemory(Instance->Graphics.Device, Memory, nullptr);
		// if(Sampler)
		// 	vkDestroySampler(Instance->Graphics.Device, Sampler, nullptr);
		// if(View)
		// 	vkDestroyImageView(Instance->Graphics.Device, View, nullptr);
	}
}

void DynamicImage::changeSampler(const VkSamplerCreateInfo *sampler)
{
	if(!IsFirstCreate && (Instance->Screen.State == Vulkan::DrawState::Drawing || !Instance->isRenderThread()) && Instance->isAlive())
	{
		// Нельзя пересоздать картинку сейчас
		std::shared_ptr<DynamicImage> obj = shared_from_this();

		if(!IsFirstCreate)
			assert(obj && "Чтобы изменять картинку во время рендера сцены, она должна быть Shared");

		Instance->beforeDraw([obj, info = *sampler](Vulkan *instance)
		{
			obj->changeSampler(&info);
		});

		return;
	}

	auto oldSampler = Sampler;
	assert(!vkCreateSampler(Instance->Graphics.Device, sampler, nullptr, &Sampler));

	// Обновляем дескрипторы
	for(size_t iter = 0; iter < AfterRecreate.size(); iter++)
	{
		if(!AfterRecreate[iter]())
		{
			AfterRecreate.erase(AfterRecreate.begin() + iter);
			iter--;
		}
	}

	if(oldSampler)
		vkDestroySampler(Instance->Graphics.Device, oldSampler, nullptr);
}

void DynamicImage::recreateImage(uint16_t width, uint16_t height, const uint32_t *rgba)
{
	if(!IsFirstCreate && (Instance->Screen.State == Vulkan::DrawState::Drawing || !Instance->isRenderThread()) && Instance->isAlive())
	{
		// Нельзя пересоздать картинку сейчас
		std::shared_ptr<DynamicImage> obj = shared_from_this();

		if(!IsFirstCreate)
			assert(obj && "Чтобы изменять картинку во время рендера сцены, она должна быть Shared");

		ByteBuffer buff(size_t(width)*size_t(height)*4);
		if(rgba)
			std::copy(rgba, rgba+buff.size()/4, (uint32_t*) buff.data());

		Width = width;
		Height = height;

		Instance->beforeDraw([obj, buff, width, height](Vulkan *instance) {
			obj->recreateImage(width, height, (uint32_t*) buff.data());
		});

		return;
	}

	Width = width;
	Height = height;

	if(width == 0)
		width = 1;

	if(height == 0)
		height = 1;

	// Нужно удалить их после того как будут обновлены дексрипторы
	auto oldImage = Image;
	Image = nullptr;
	auto oldMemory = Memory;
	Memory = nullptr;
	auto oldView = View;
	View = nullptr;

	// Создаём конечную картинку
	VkImageCreateInfo infoImageCreate =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent =  { (uint32_t) width, (uint32_t) height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_MAX_ENUM,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = 0,
		.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
	};
	
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(Instance->Graphics.PhysicalDevice, infoImageCreate.format, &props);

	if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
		infoImageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
	else if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
		infoImageCreate.tiling = VK_IMAGE_TILING_LINEAR;
	else
		/* Can't support VK_FORMAT_B8G8R8A8_UNORM */
		assert(!"No support for B8G8R8A8_UNORM as texture image format");
	
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &Image));
	
	// Выделяем память
	VkMemoryRequirements memoryReqs;
	const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

	vkGetImageMemoryRequirements(Instance->Graphics.Device, Image, &memoryReqs);

	VkMemoryAllocateInfo memoryAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memoryReqs.size,
		.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &Memory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, Image, Memory, 0));

	// Порядок пикселей и привязка к картинке
	VkImageViewCreateInfo ciView =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = Image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = infoImageCreate.format,
		.components =
		{
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	assert(!vkCreateImageView(Instance->Graphics.Device, &ciView, nullptr, &View));

	// Обновляем дескрипторы
	for(size_t iter = 0; iter < AfterRecreate.size(); iter++)
	{
		if(!AfterRecreate[iter]())
		{
			AfterRecreate.erase(AfterRecreate.begin() + iter);
			iter--;
		}
	}

	// Удаляем старые объекты
	Instance->beforeDraw([oldImage, oldMemory, oldView](Vulkan *instance){
		if(oldImage)
			vkDestroyImage(instance->Graphics.Device, oldImage, nullptr);
		if(oldMemory) 
			vkFreeMemory(instance->Graphics.Device, oldMemory, nullptr);
		if(oldView) 
			vkDestroyImageView(instance->Graphics.Device, oldView, nullptr);
	});

	changeData(0, 0, Width, Height, rgba);
}

void DynamicImage::changeData(const uint32_t *rgba)
{
	changeData(0, 0, Width, Height, rgba);
}

void DynamicImage::changeData(int32_t x, int32_t y, uint16_t width, uint16_t height, const uint32_t *rgba)
{
	assert(width <= Width && height <= Height && "Превышен размер обновляемых данных width <= Width && height <= Height");

	if(IsFirstCreate)
	{
		VkImageMemoryBarrier infoImageMemoryBarrier =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_NONE,
			.dstAccessMask = VK_ACCESS_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = ImageLayout,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image = Image,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		CommandBuffer buffer(Instance);
		vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);
		buffer.execute();
		return;
	} else if(width == 0 || height == 0)
		return;

	if(Instance->isAlive() && (Instance->Screen.State == Vulkan::DrawState::Drawing || !Instance->isRenderThread()))
	{
		// Нельзя обновить картинку сейчас
		std::shared_ptr<DynamicImage> obj = shared_from_this();
		assert(obj && "Чтобы изменять картинку во время рендера сцены, она должна быть Shared");

		ByteBuffer buff(size_t(width)*size_t(height)*4, (const uint8_t*) rgba);

		Instance->beforeDraw([obj, buff, x, y, width, height](Vulkan *instance) {
			obj->changeData(x, y, width, height, (uint32_t*) buff.data());
		});

		return;
	}

	CommandBuffer buffer(Instance);
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(Instance->Graphics.PhysicalDevice, VK_FORMAT_B8G8R8A8_UNORM, &props);

	VkImageMemoryBarrier infoImageMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = VK_NULL_HANDLE,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

	// Создаём временную картинку
	VkImage tempImage;
	VkDeviceMemory tempMemory;
	
	VkImageCreateInfo infoImageCreate =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent =  { (uint32_t) width, (uint32_t) height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = 0,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkMemoryRequirements memoryReqs;
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &tempImage));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, tempImage, &memoryReqs);

	VkMemoryAllocateInfo memoryAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memoryReqs.size,
		.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	
	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &tempMemory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, tempImage, tempMemory, 0));

	// Заполняем данными
	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(Instance->Graphics.Device, tempImage, &memorySubres, &layout);

	void *data;
	assert(!vkMapMemory(Instance->Graphics.Device, tempMemory, 0, memoryAlloc.allocationSize, 0, &data));

	if(rgba)
	{
		for (size_t y = 0; y < height; y++)
		{
			uint32_t *row = (uint32_t*) (((uint8_t*) data) + layout.rowPitch * y);
			uint32_t *color = ((uint32_t*) rgba) + uint32_t(width)*y;
			for (size_t  x = 0; x < width; x++, row++, color++)
				*row = *color;
		}
	} else {
		uint32_t *start = (uint32_t*) data;
		for(size_t begin = 0, end = memoryAlloc.allocationSize/4; begin != end; begin++, start++)
			*start = 0;
	}

	vkUnmapMemory(Instance->Graphics.Device, tempMemory);

	// Создаём барьер, чтобы следующие команды выполнялись только после того 
	// как будут завершены операции по переносу данных с хоста к драйверу
	// И меняем layout на нужный
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = infoImageCreate.initialLayout;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Будет использоваться как источник данных
	infoImageMemoryBarrier.image = tempImage;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	// Задаём нужный layout
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.image = Image;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	VkImageCopy copy_region =
	{
		.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.srcOffset = { 0, 0, 0 },
		.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.dstOffset = { x, y, 0 },
		.extent = { width, height, 1 },
	};

	vkCmdCopyImage(buffer, tempImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.newLayout = ImageLayout; // Используем как текстуру
	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	// Выполняем все команды
	buffer.execute();

	// Удаляем не нужную картинку
	vkDestroyImage(Instance->Graphics.Device, tempImage, nullptr);
	vkFreeMemory(Instance->Graphics.Device, tempMemory, nullptr);
}


void DynamicImage::readData(int32_t x, int32_t y, uint16_t width, uint16_t height, uint32_t *rgba)
{
	const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

	CommandBuffer buffer(Instance);
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(Instance->Graphics.PhysicalDevice, VK_FORMAT_B8G8R8A8_UNORM, &props);

	VkImageMemoryBarrier infoImageMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = VK_NULL_HANDLE,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	// Создаём временную картинку
	VkImage tempImage;
	VkDeviceMemory tempMemory;
	
	VkImageCreateInfo infoImageCreate =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent =  { (uint32_t) width, (uint32_t) height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = 0,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkMemoryRequirements memoryReqs;
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &tempImage));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, tempImage, &memoryReqs);

	VkMemoryAllocateInfo memoryAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memoryReqs.size,
		.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	
	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &tempMemory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, tempImage, tempMemory, 0));

	// Подготавливаем изображение к приёму данных
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = infoImageCreate.initialLayout;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.image = tempImage;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	infoImageMemoryBarrier.oldLayout = ImageLayout;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	infoImageMemoryBarrier.image = Image;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	// Копируем нужный участок
	VkImageCopy copy_region =
	{
		.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.srcOffset = { x, y, 0 },
		.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.dstOffset = { 0, 0, 0 },
		.extent = { uint32_t(width), uint32_t(height), 1 },
	};

	vkCmdCopyImage(buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			tempImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	buffer.execute();

	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	infoImageMemoryBarrier.newLayout = ImageLayout;
	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	buffer.execute();

	// Забираем данные
	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(Instance->Graphics.Device, tempImage, &memorySubres, &layout);

	void *data;
	assert(!vkMapMemory(Instance->Graphics.Device, tempMemory, 0, memoryAlloc.allocationSize, 0, &data));

	for (size_t y = 0; y < height; y++)
	{
		uint32_t *row = (uint32_t*) (((uint8_t*) data) + layout.rowPitch * y);
		uint32_t *color = ((uint32_t*) rgba) + uint32_t(width)*y;
		for (size_t  x = 0; x < width; x++, row++, color++)
			*color = *row;
	}

	vkUnmapMemory(Instance->Graphics.Device, tempMemory);

	// И удаляем не нужную картинку
	vkDestroyImage(Instance->Graphics.Device, tempImage, nullptr);
	vkFreeMemory(Instance->Graphics.Device, tempMemory, nullptr);
}



AtlasImage::AtlasImage(Vulkan *instance)
	: DynamicImage(instance), 
	  UniformSchema(instance, sizeof(UniformInfo), 
	  	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		HostBuffer(instance, 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
{
}

AtlasImage::~AtlasImage() {}

void AtlasImage::claimTexture(uint16_t id, uint16_t width, uint16_t height)
{
	auto &info = SubTextures[id];
	info.Width = width;
	info.Height = height;

	// Всё равно будет пересборка
	if(NeedRebuild)
	{
		if(!CachedData.contains(id))
			CachedData.insert({id, {}});

		return;
	}

	uint16_t NowHeight = 0;
	uint16_t nextHeight;

	for(; NowHeight < Heights.size(); NowHeight++)
	{
		nextHeight = Heights.size();

		for(uint32_t x = 0; x < Heights.size(); x++)
		{
			// Текстура уже не влазит по высоте
			if(height > Heights.size()-NowHeight)
			{
				NeedRebuild = true;
				CachedData[id] = {};
				return;
			}

			// Найти свободную точку
			for(; x < Heights.size() && Heights[x] > NowHeight; x++)
				if(Heights[x] > NowHeight && Heights[x] < nextHeight)
					nextHeight = Heights[x];

			// Определить сколько места свободно
			uint32_t offset;
			for(offset = x; offset < Heights.size() && Heights[offset] <= NowHeight; offset++)
				if(Heights[offset] > NowHeight && Heights[offset] < nextHeight)
					nextHeight = Heights[offset];

			if(offset-x >= width)
			{
				// Нашли место
				info.PosX = x;
				info.PosY = NowHeight;

				for(uint32_t iter = 0; iter < width; iter++, x++)
					Heights[x] = NowHeight+height;

				NeedUpdateSchema = true;

				return;
			}

			// Мало места, поищем ещё
			x = offset-1;
		}

		// На этом уровне не нашли, взбираемся повыше
		NowHeight = nextHeight-1;
	}


	// Нету места :(
	NeedRebuild = true;
	CachedData[id] = {};
}

void AtlasImage::optimizeFreeIds()
{
	if(IdsChanges++ < 16)
		return;

	if(SubTextures.empty())
	{
		FreeIds.clear();
		return;
	}

	// Последний занятый идентификатор
	uint16_t maxId = SubTextures.rend()->first;

	// Всё, что выше максимального идентификатора будет удалено
	std::vector<uint16_t> idToDelete;

	uint16_t *obj = FreeIds.data();
	for(size_t iter = 0; iter != FreeIds.size(); iter++, obj++)
		if(*obj > maxId)
			idToDelete.push_back(iter);

	std::vector<uint16_t> newFreeIds(FreeIds.size() - idToDelete.size());
	obj = FreeIds.data();
	uint16_t *newIds = newFreeIds.data();
	auto toDelete = idToDelete.begin();

	for(size_t iter = 0; iter != FreeIds.size(); iter++, obj++)
		if(toDelete == idToDelete.end() || *obj != *toDelete)
			*(newIds++) = *obj;
		else
			toDelete++;

	FreeIds = std::move(newFreeIds);
}

uint16_t AtlasImage::atlasAddTexture(uint16_t width, uint16_t height)
{
	std::lock_guard lock(Changes);

	if(FreeIds.size())
	{
		uint16_t id = FreeIds.back();
		FreeIds.pop_back();

		claimTexture(id, width, height);
		return id;
	}

	if(SubTextures.empty())
	{
		claimTexture(0, width, height);
		return 0;
	}

	uint16_t id = SubTextures.rbegin()->first;
	if(id == 65535)
		MAKE_ERROR("Закончились свободные идентификаторы в атласе");

	id++;
	claimTexture(id, width, height);
	return id;
}

void AtlasImage::atlasRemoveTexture(uint16_t id)
{
	std::lock_guard lock(Changes);

	auto iter = SubTextures.find(id);
	if(iter == SubTextures.end())
		MAKE_ERROR("Идентификатор не существует");

	SubTextures.erase(iter);
	FreeIds.push_back(id);

	optimizeFreeIds();
}

void AtlasImage::atlasResizeTexture(uint16_t id, uint16_t width, uint16_t height)
{
	std::lock_guard lock(Changes);

	InfoSubTexture *info = const_cast<InfoSubTexture*>(atlasGetTextureInfo(id));

	{
		auto iter = CachedData.find(id);
		if(iter != CachedData.end())
		{
			// Убираем данные, запланированные на запись в атлас, только если текстура уже находится в атласе
			if(iter->second.size())
				CachedData.erase(iter);
		}
	}

	// Если атлас будет пересобран, то просто меняем размер
	if(NeedRebuild)
	{
		info->Width = width;
		info->Height = height;
	} else /* Используем уже выделенное место */ if(width <= info->Width && height <= info->Height)
	{
		info->Width = width;
		info->Height = height;

		NeedUpdateSchema = true;
	} else /* Выделим новое место */ {
		claimTexture(id, width, height);
		// Либо повезёт и найдётся место в текущем атласе, либо придётся его весь пересоздать
	}
}

void AtlasImage::atlasChangeTextureData(uint16_t id, const uint32_t *rgba)
{
	std::lock_guard lock(Changes);

	InfoSubTexture *info = const_cast<InfoSubTexture*>(atlasGetTextureInfo(id));

	auto iter = CachedData.find(id);
	// Если есть данные в кэше, то меняем их
	if(iter != CachedData.end())
	{
		if(iter->second.size() == 0)
			iter->second.resize(size_t(info->Width)*size_t(info->Height)*4);

		std::copy(rgba, rgba+iter->second.size()/4, (uint32_t*) iter->second.data());
	} else {
		// Или меняем напрямую в атласе
		changeData(info->PosX, info->PosY, info->Width, info->Height, rgba);
	}
}

void AtlasImage::atlasSetAnimation(uint16_t id, bool enabled, uint16_t frames, float timePerFrame)
{
	std::lock_guard lock(Changes);

	InfoSubTexture *info = const_cast<InfoSubTexture*>(atlasGetTextureInfo(id));

	info->Animation.Enabled = enabled;
	info->Animation.Frames = frames;
	info->Animation.TimePerFrame = timePerFrame;

	NeedUpdateSchema = true;
}

void AtlasImage::atlasClear()
{
	std::lock_guard lock(Changes);

	FreeIds.clear();
	CachedData.clear();
	Heights.clear();
	SubTextures.clear();

	recreateImage(0, 0, nullptr);

	NeedUpdateSchema = true;
}

void AtlasImage::atlasAddCallbackOnUniformChange(std::function<bool()> &&callback)
{
	AfterUniformChange.push_back(std::move(callback));
}

void AtlasImage::atlasUpdateDynamicData()
{
	assert(Instance->isRenderThread() && "Обновление должно вызываться в потоке рендера");

	std::lock_guard lock(Changes);

	// Если необходимо, то пересобрать атлас
	if(NeedRebuild)
	{
		NeedRebuild = false;

		// 16×2^10 предел
		size_t edge = 16;

		// Сортируем идентификаторы по ширине текстуры
		// И вычислим площадь всех текстур, чтобы лишний раз не пробывать размер

		size_t square = 0;
		uint16_t maxEdge = 0;

		std::vector<uint16_t> ids(SubTextures.size());
		{
			uint16_t *id = ids.data();
			for(auto &iter : SubTextures)
			{
				*(id++) = iter.first;
				square += size_t(iter.second.Width)*size_t(iter.second.Height);
				maxEdge = std::max(maxEdge, iter.second.Width);
				maxEdge = std::max(maxEdge, iter.second.Height);
			}
		}

		std::sort(ids.begin(), ids.end(), [&](const uint16_t left, const uint16_t right) { return int(SubTextures[left].Width) > int(SubTextures[right].Width); });

		uint8_t pow = std::ceil(std::log2(std::sqrt(float(square))));
		if(pow > 32)
			MAKE_ERROR("В атласе недостаточно места, требуется объём 2^" << pow);

		pow = std::max<uint8_t>(pow, std::ceil(std::log2(float(maxEdge))));

		if(pow > 4)
			edge <<= (pow-4);

		// Нужно извлечь текстуры из атласа
		for(auto &iter : SubTextures)
		{	
			if(CachedData.contains(iter.first))
				continue;

			ByteBuffer &cache = CachedData[iter.first];
			cache.resize(size_t(iter.second.Width)*size_t(iter.second.Height)*4);
			readData(iter.second.PosX, iter.second.PosY, iter.second.Width, iter.second.Height, (uint32_t*) cache.data());
		}

		bool end = false;
		for(uint8_t powder = std::max<uint8_t>(pow, 4); powder < 14; powder++, edge *= 2)
		{
			NeedRebuild = false;

			// Подбираем подходящий размер атласа
			Heights.resize(edge);
			std::fill((uint64_t*) Heights.data(), ((uint64_t*) Heights.data())+edge/4, 0);

			// Теперь размещаем текстуры
			for(uint16_t id : ids)
			{
				auto &info = SubTextures[id];
				
				ByteBuffer temp = std::move(CachedData[id]);
				claimTexture(id, info.Width, info.Height);
				CachedData[id] = std::move(temp);

				if(NeedRebuild)
				{
					// Текстура не влезла
					break;
				}
			}

			if(NeedRebuild)
				continue;

			break;
		}

		if(NeedRebuild)
		{
			NeedRebuild = false;
			MAKE_ERROR("Текстуры не уместились в атласе");
		}

		// Заполним атлас из кеша
		recreateImage(uint16_t(edge), uint16_t(edge), nullptr);

		for(auto &iter : SubTextures)
		{
			ByteBuffer &buff = CachedData[iter.first];

			if(buff.empty())
				continue;

			changeData(iter.second.PosX, iter.second.PosY, iter.second.Width, iter.second.Height, (const uint32_t*) buff.data());
		}

		CachedData.clear();
		NeedUpdateSchema = true;

		// Обновим uniform буфер
	}


	// Если необходимо, обновить схему в uniform буфере
	if(NeedUpdateSchema)
	{
		NeedUpdateSchema = false;
		uint16_t count = SubTextures.empty() ? 0 : SubTextures.rbegin()->first+1;

		// Удалим буфер после обновления дескрипторов
		auto oldBuffer = VK::Buffer(Instance, 
			sizeof(UniformInfo) + size_t(count)*sizeof(InfoSubTexture), 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		std::swap(oldBuffer, UniformSchema);

		UniformInfo *data = (UniformInfo*) HostBuffer.mapMemory();
		data->SubsCount = count;
		data->Counter = uint32_t(glfwGetTime()*256);
		data->Width = Width;
		data->Height = Height;
		
		VkBufferCopy region = {
       		.srcOffset = 0,
    		.dstOffset = 0,
    		.size = sizeof(UniformInfo)
		};

		CommandBuffer cmd(Instance);
		vkCmdCopyBuffer(cmd, HostBuffer, UniformSchema, 1, &region);
		cmd.execute();

		InfoSubTexture *subs = (InfoSubTexture*) (data);
		//std::fill((uint64_t*) subs, (uint64_t*) (subs+count), 0);

		region.dstOffset = sizeof(UniformInfo);
		size_t maxCount = HostBuffer.getSize()/sizeof(InfoSubTexture);
		for(size_t sub = 0; sub < count; sub++, subs++)
		{
			auto iter = SubTextures.find(sub);
			if(iter == SubTextures.end())
				subs->isExist = 0;
			else {
				*subs = iter->second;
				subs->isExist = 1;
			}
			
			// Кончилось место
			if((sub+1) % maxCount == 0)
			{
				region.size = maxCount*sizeof(InfoSubTexture);
				vkCmdCopyBuffer(cmd, HostBuffer, UniformSchema, 1, &region);
				cmd.execute();
				region.dstOffset += maxCount*sizeof(InfoSubTexture);
				subs = (InfoSubTexture*) (data);
			}
		}

		if(count % maxCount)
		{
			maxCount = count - (count / maxCount) * maxCount;
			region.size = maxCount*sizeof(InfoSubTexture);
			vkCmdCopyBuffer(cmd, HostBuffer, UniformSchema, 1, &region);
			cmd.execute();
		}

		// for(auto &iter : SubTextures)
		// {
		// 	TOS_ASSERT(iter.first < count, "Ключи таблицы отсортированы не верно");

		// 	subs[iter.first] = iter.second;
		// 	subs[iter.first].isExist = 1;
		// }
		
		HostBuffer.unMapMemory();

		// Обновляем дескрипторы
		for(size_t iter = 0; iter < AfterUniformChange.size(); iter++)
		{
			if(!AfterUniformChange[iter]())
			{
				AfterUniformChange.erase(AfterUniformChange.begin() + iter);
				iter--;
			}
		}

		// По выходу из области видимости oldBuffer удалится

	} else {
		// Просто обновить счётчик анимаций
		* (decltype(UniformInfo::Counter)*) HostBuffer.mapMemory(0, sizeof(UniformInfo::Counter)) = uint32_t(glfwGetTime()*256);
		VkBufferCopy region = {
       		.srcOffset = 0,
    		.dstOffset = offsetof(UniformInfo, Counter),
    		.size = sizeof(UniformInfo::Counter)
		};

		CommandBuffer cmd(Instance);
		vkCmdCopyBuffer(cmd, HostBuffer, UniformSchema, 1, &region);
		cmd.execute();

		HostBuffer.unMapMemory();
	}
}

ByteBuffer AtlasImage::atlasSchemaUnload() const
{
	return {};
}

void AtlasImage::atlasSchemaLoad(const ByteBuffer &buff)
{

}

const AtlasImage::InfoSubTexture* AtlasImage::atlasGetTextureInfo(uint16_t id)
{
	auto iter = SubTextures.find(id);
	if(iter == SubTextures.end())
		MAKE_ERROR("Идентификатор не существует");

	return &iter->second;
}

ArrayImage::ArrayImage(Vulkan *instance, std::filesystem::path directory)
	: Instance(instance)
{
	CommandBuffer buffer(instance);

	ByteBuffer buff;
	size_t width, height;
	bool hasAlpha;

	std::vector<ByteBuffer> images;

	// Загрузим все текстуры в память и пsроверим их
	for(auto file : std::filesystem::directory_iterator(directory))
	{
		buff = SimpleImage::loadTexture(file, width, height, hasAlpha);

		if(!Width)
			Width = width;

		if(width != Width || Width != height)
		{
			MAKE_ERROR("Все текстуры должны быть одинаковые по размеру, " << file.path().filename() 
				<< " размер: " << width << " X " << height);
		}

		if(std::optional<std::vector<std::optional<std::string>>> groups = Str::match(file.path().filename().c_str(), "^(.*)\\..*$"))
		{
			TextureMapping[*groups.value()[1]] = images.size();
			images.push_back(std::move(buff));
		} else
			MAKE_ERROR("Не удалось именовать текстуру " << file.path().filename());
		
	}

	constexpr VkFormat tex_format = VK_FORMAT_B8G8R8A8_UNORM;
	ImageLayout = VK_IMAGE_LAYOUT_GENERAL; // То как будем использовать графический буфер, в данном случае как текстура
	VkFormatProperties props;

	vkGetPhysicalDeviceFormatProperties(Instance->Graphics.PhysicalDevice, tex_format, &props);

	VkImageCreateInfo infoImageCreate =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = tex_format,
		.extent =  { (uint32_t) width, (uint32_t) height, 1 },
		.mipLevels = 1,
		.arrayLayers = uint32_t(images.size()),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_MAX_ENUM,
		.usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = 0,
		.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
	};
	
	VkMemoryAllocateInfo memoryAlloc =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = 0,
		.memoryTypeIndex = 0
	};

	VkMemoryRequirements memoryReqs;
	const VkImageSubresource memorySubres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };

	VkImageMemoryBarrier infoImageMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = VK_NULL_HANDLE,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	// Создаём временную 2D картинку
	VkImage tempImage;
	VkDeviceMemory tempMemory;
	VkMemoryRequirements memoryReqsTemp = memoryReqs;

	infoImageCreate.tiling = VK_IMAGE_TILING_LINEAR;
	infoImageCreate.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	infoImageCreate.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	infoImageCreate.arrayLayers = 1,
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &tempImage));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, tempImage, &memoryReqsTemp);

	memoryAlloc.allocationSize = memoryReqsTemp.size;
	memoryAlloc.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqsTemp.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &tempMemory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, tempImage, tempMemory, 0));

	// Создаём конечную картинку
	if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
		infoImageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
	else
		infoImageCreate.tiling = VK_IMAGE_TILING_LINEAR;

	infoImageCreate.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	infoImageCreate.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	infoImageCreate.arrayLayers = uint32_t(images.size()),
	assert(!vkCreateImage(Instance->Graphics.Device, &infoImageCreate, nullptr, &Image));
	vkGetImageMemoryRequirements(Instance->Graphics.Device, Image, &memoryReqs);

	memoryAlloc.allocationSize = memoryReqs.size;
	memoryAlloc.memoryTypeIndex = Instance->memoryTypeFromProperties(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	assert(!vkAllocateMemory(Instance->Graphics.Device, &memoryAlloc, nullptr, &Memory));
	assert(!vkBindImageMemory(Instance->Graphics.Device, Image, Memory, 0));

	// Задаём нужный layout
	infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	infoImageMemoryBarrier.image = Image;
	infoImageMemoryBarrier.subresourceRange.layerCount = uint32_t(images.size());

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

	infoImageMemoryBarrier.subresourceRange.layerCount = 1;
	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(Instance->Graphics.Device, tempImage, &memorySubres, &layout);

	for(size_t iter = 0; iter < images.size(); iter++)
	{
		// Загружаем по одной картинке
		void *data;
		assert(!vkMapMemory(Instance->Graphics.Device, tempMemory, 0, memoryReqsTemp.size, 0, &data));

		for (int32_t y = 0; y < Width; y++) 
		{
			uint32_t *row = (uint32_t*) (((uint8_t*) data) + layout.rowPitch * y);
			uint32_t *color = ((uint32_t*) images[iter].data()) + Width*y;
			for (uint32_t x = 0; x < Width; x++, row++, color++)
				*row = *color;
		}

		vkUnmapMemory(Instance->Graphics.Device, tempMemory);

		infoImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
		infoImageMemoryBarrier.oldLayout = infoImageCreate.initialLayout;
		infoImageCreate.initialLayout = infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		infoImageMemoryBarrier.image = tempImage;

		vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

		VkImageCopy copy_region =
		{
			.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			.srcOffset = { 0, 0, 0 },
			.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, uint32_t(iter), 1 },
			.dstOffset = { 0, 0, 0 },
			.extent = { Width, Width, 1 },
		};

		// Настриваем слой на приём данных
		infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
		infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
		infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		infoImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		infoImageMemoryBarrier.image = Image;
		infoImageMemoryBarrier.subresourceRange.baseArrayLayer = iter;
		vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

		vkCmdCopyImage(buffer, tempImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
	
		// Теперь слой будет использоваться как картинка
		infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		infoImageMemoryBarrier.newLayout = ImageLayout;

		vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);

		// Выполняем все команды
		buffer.execute();
	}


	// Удаляем не нужную картинку
	vkDestroyImage(Instance->Graphics.Device, tempImage, nullptr);
	vkFreeMemory(Instance->Graphics.Device, tempMemory, nullptr);

	// infoImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	// infoImageMemoryBarrier.subresourceRange.layerCount = uint32_t(images.size());
	// infoImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
	// infoImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
	// infoImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	// infoImageMemoryBarrier.newLayout = ImageLayout; // Используем как текстуру
	// infoImageMemoryBarrier.image = Image;
	// vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	// 		0, 0, nullptr, 0, nullptr, 1, &infoImageMemoryBarrier);
	
	// buffer.execute();


	{
		// Способ чтения картинки
		const VkSamplerCreateInfo ciSampler =
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 1,
			.compareEnable = 0,
			.compareOp = VK_COMPARE_OP_NEVER,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			.unnormalizedCoordinates = VK_FALSE
		};

		assert(!vkCreateSampler(Instance->Graphics.Device, &ciSampler, nullptr, &Sampler));
	}

	{
		// Порядок пикселей
		VkImageViewCreateInfo ciView =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = tex_format,
			.components =
			{
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_A
			},
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, uint32_t(images.size()) }
		};

		assert(!vkCreateImageView(Instance->Graphics.Device, &ciView, nullptr, &View));
	}
}

ArrayImage::~ArrayImage()
{
	if(Instance && Instance->Graphics.Device)
	{
		Instance->beforeDraw([image = Image, memory = Memory, 
			sampler = Sampler, view = View](Vulkan *instance)
		{
			if(image) 
				vkDestroyImage(instance->Graphics.Device, image, nullptr);
			if(memory) 
				vkFreeMemory(instance->Graphics.Device, memory, nullptr);
			if(sampler) 
				vkDestroySampler(instance->Graphics.Device, sampler, nullptr);
			if(view) 
				vkDestroyImageView(instance->Graphics.Device, view, nullptr);
		});
	}
}

uint16_t ArrayImage::getTextureId(const std::string &name)
{
	auto iter = TextureMapping.find(name);
	if(iter == TextureMapping.end())
		return 0;

	return iter->second;
}

FontAtlas::FontAtlas(Vulkan *instance)
	: AtlasImage(instance)
{
	FT_Init_FreeType(&FontLibrary);

	const VkSamplerCreateInfo ciSampler =
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1,
		.compareEnable = 0,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		.unnormalizedCoordinates = VK_FALSE
	};

	changeSampler(&ciSampler);
}

FontAtlas::~FontAtlas()
{
	FontList.clear();

	if(FontLibrary)
		FT_Done_FreeType(FontLibrary);
}

void FontAtlas::clearInvalidGlyphs()
{
	std::vector<uint64_t> toDelete;
	for(auto &iter : CharToInfo)
		if(!iter.second.IsValid)
			toDelete.push_back(iter.first);

	for(auto &iter : toDelete)
		CharToInfo.erase(CharToInfo.find(iter));

	LastUpdate += 1;
}

void FontAtlas::pushFont(const std::variant<std::filesystem::path, ByteBuffer> &font)
{
	FontList.push_back({});
	auto &face = FontList.back();
	if(font.index() == 0)
		face.Data = std::ifstream(std::get<std::filesystem::path>(font));
	else
	 	face.Data = std::move(std::get<ByteBuffer>(font));
	
	FT_New_Memory_Face(FontLibrary, face.Data.data(), face.Data.size(), 0, &face.Obj);

	clearInvalidGlyphs();
}

void FontAtlas::setFontList(const std::vector<std::variant<std::filesystem::path, ByteBuffer>> &fonts)
{
	atlasClear();
	FontList.clear();
	for(auto &obj : fonts)
	{
		FontList.push_back({});
		auto &face = FontList.back();
		if(obj.index() == 0)
			face.Data = std::ifstream(std::get<std::filesystem::path>(obj));
		else
			face.Data = std::move(std::get<ByteBuffer>(obj));
		
		FT_New_Memory_Face(FontLibrary, face.Data.data(), face.Data.size(), 0, &face.Obj);
	}

	clearInvalidGlyphs();
}

FontAtlas::GlyphInfo FontAtlas::getGlyph(UChar wc, uint16_t size)
{
	uint64_t glyphId = (uint64_t(wc) << sizeof(uint16_t)*8) | size;
	auto iter = CharToInfo.find(glyphId);
	if(iter == CharToInfo.end())
	{
		for (const Face &face : FontList)
		{
			GlyphInfo info;

			{
				FT_Set_Pixel_Sizes(face.Obj, 0, size);
				FT_Load_Char(face.Obj, wc, FT_LOAD_RENDER);

				FT_UInt glyph_index = FT_Get_Char_Index(face.Obj, wc);

				if (glyph_index == 0) // Нет символа в шрифте, посмотрим в других
					continue;

				FT_Load_Glyph(face.Obj, glyph_index, FT_LOAD_DEFAULT);
				FT_Render_Glyph(face.Obj->glyph, FT_RENDER_MODE_NORMAL);
				FT_Glyph glyph;
				FT_Get_Glyph(face.Obj->glyph, &glyph);

				FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_LCD, 0, 1);
				FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph) glyph;
				FT_Bitmap bitmap = bitmap_glyph->bitmap;

				try {
					ByteBuffer data((bitmap.width+2)*(bitmap.rows+2)*sizeof(uint32_t));
				
					std::fill((uint32_t*) data.data(), (uint32_t*) (data.data() + data.size()), 0xffffff);
					
					uint8_t *ptrFrom = (uint8_t*) bitmap.buffer;
					uint32_t *ptrTo = ((uint32_t*) data.data()) + (bitmap.width+2);
					for (int y = 0; y < bitmap.rows; y++)
					{
						ptrTo++;
						for(int x = 0; x < bitmap.width; x++, ptrFrom++, ptrTo++)
							*ptrTo |= uint32_t(*ptrFrom) << 24;

						ptrTo++;
					}

					info.IsValid = true;
					info.Width = glyph->advance.x >> 16;
					info.Height = bitmap_glyph->top;
					info.PosX = bitmap_glyph->left;
					info.PosY = bitmap_glyph->top-int(bitmap.rows);

					if(wc == L'?')
						info.IsValid = false;

					info.TexId = atlasAddTexture(bitmap.width+2, bitmap.rows+2);
					atlasChangeTextureData(info.TexId, (uint32_t*) data.data());
				} catch(...) {
					FT_Done_Glyph(glyph);
					throw;
				}

				FT_Done_Glyph(glyph);
			}

			return CharToInfo[glyphId] = info;
		}
	} else 
		return iter->second;

	if(wc != L'?')
		return getGlyph(L'?', size);

	GlyphInfo info;

	info.IsValid = false;
	info.TexId = 65535;
	info.Width = size;
	info.Height = size;
	info.PosX = 0;
	info.PosY = 0;

	return CharToInfo[glyphId] = info;
}



PipelineVF::PipelineVF(std::shared_ptr<DescriptorLayout> layout, const std::string &vertex, 
	const std::string &fragment)
	: Pipeline(layout), PathVertex(vertex), PathFragment(fragment)
{
}

PipelineVF::~PipelineVF() = default;

void PipelineVF::init(Vulkan *instance)
{
	if(!ShaderVertex)
		ShaderVertex = instance->createShader(getResource(PathVertex)->makeView());

	if(!ShaderFragment)
		ShaderFragment = instance->createShader(getResource(PathFragment)->makeView());

	Settings.ShaderStages =
	{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = *ShaderVertex,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = *ShaderFragment,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};

	Pipeline::init(instance);
}

PipelineVGF::PipelineVGF(std::shared_ptr<DescriptorLayout> layout, const std::string &vertex, 
	const std::string &geometry, const std::string &fragment)
	: Pipeline(layout), PathVertex(vertex), PathGeometry(geometry), PathFragment(fragment)
{
	// Settings.ShaderVertexBindings =
	// {
	// 	{
	// 		.binding = 0,
	// 		.stride = sizeof(Client::Chunk::Vertex),
	// 		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	// 	}
	// };

	// Settings.ShaderVertexAttribute =
	// {
	// 	{
	// 		.location = 0,
	// 		.binding = 0,
	// 		.format = VK_FORMAT_R32_UINT,
	// 		.offset = 0
	// 	}
	// };
}

PipelineVGF::~PipelineVGF() = default;

void PipelineVGF::init(Vulkan *instance)
{
	if(!ShaderVertex)
		ShaderVertex = instance->createShader(getResource(PathVertex)->makeView());

	if(!ShaderGeometry)
		ShaderGeometry = instance->createShader(getResource(PathGeometry)->makeView());

	if(!ShaderFragment)
		ShaderFragment = instance->createShader(getResource(PathFragment)->makeView());

	Settings.ShaderStages =
	{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = *ShaderVertex,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_GEOMETRY_BIT,
			.module = *ShaderGeometry,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = *ShaderFragment,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};

	Pipeline::init(instance);
}

}
