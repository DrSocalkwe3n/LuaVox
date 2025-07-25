#pragma once

// Cmake
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
static_assert(GLM_CONFIG_CLIP_CONTROL == GLM_CLIP_CONTROL_RH_ZO);

#include "Client/ServerSession.hpp"
#include "Common/Async.hpp"
#include <TOSLib.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <unicode/umachine.h>
#include <unordered_set>

#define HAS_IMGUI

#include "freetype/freetype.h"

#include <vulkan/vulkan_core.h>
#include <map>
#define TOS_VULKAN_NO_VIDEO
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


#define IMGUI_ENABLE_STB_TEXTEDIT_UNICODE

namespace LV::Client::VK {

class VulkanRenderSession;
using namespace TOS;

ByteBuffer loadPNG(std::ifstream &&file, int &width, int &height, bool &hasAlpha, bool flipOver = true);
ByteBuffer loadPNG(std::istream &&file, int &width, int &height, bool &hasAlpha, bool flipOver = true);

struct DeviceId {
	uint32_t VendorId = -1, DeviceId = -1;
};

struct Settings {
	DeviceId DeviceMain;
	uint32_t QueueGraphics = -1, QueueSurface = -1;
	bool Debug = true;

	bool isValid()
	{
		return 	!(DeviceMain.VendorId == -1
				|| DeviceMain.DeviceId == -1
				|| QueueGraphics == -1
				|| QueueSurface == -1);
	}
};

class ServerObj;
class DescriptorLayout;
class Pipeline;
class DescriptorPool;
class ShaderModule;
class IVulkanDependent;
class Buffer;

#define vkAssert(err) if(!bool(err)) { MAKE_ERROR(__FILE__ << ": " << __LINE__ << "//" << __func__); }

/*
	Vulkan.getSettingsNext() = Vulkan.getBestSettings();
	Vulkan.reInit();
*/

class Vulkan : public AsyncObject {
private:
	Logger LOG = "Vulkan";

	struct vkInstanceLayer {
		std::string LayerName = "nullptr", Description = "nullptr";
	    uint32_t SpecVersion = -2, ImplementationVersion = -2;

	    bool operator==(const vkInstanceLayer &obj) { return LayerName == obj.LayerName; }

	    std::string toString()
	    {
	    	return (std::stringstream() << "Имя слоя: " << LayerName
	    			<< "\nОписание: " << Description
					<< "\nСпец версия: " << SpecVersion
					<< "\nВерсия реализации: " << ImplementationVersion).str();
	    }
	};


	const std::vector<const char*> NeedExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
		"VK_KHR_shader_float16_int8",
		"VK_KHR_16bit_storage"
		};

	struct vkInstanceExtension {
		std::string ExtensionName = "nullptr";
		uint32_t SpecVersion = -2;

	    bool operator==(const vkInstanceExtension &obj) { return ExtensionName == obj.ExtensionName; }

	    std::string toString()
	    {
	    	return (std::stringstream() << "Имя расширения: " << ExtensionName
					<< "\nСпец версия: " << SpecVersion).str();
	    }
	};

	struct vkDeviceExtension {
		std::string ExtensionName = "nullptr";
		uint32_t SpecVersion = -2;
	};

	struct vkDevice {
		uint32_t ApiVersion;
		uint32_t DriverVersion;
		uint32_t VendorID;
		uint32_t DeviceID;
		VkPhysicalDeviceType DeviceType;
		std::string DeviceName;
		//int128_t PipelineCacheUUID[VK_UUID_SIZE];
		VkPhysicalDeviceLimits Limits;
		VkPhysicalDeviceSparseProperties SparseProperties;

		VkPhysicalDeviceFeatures DeviceFeatures;
		std::vector<vkDeviceExtension> Extensions;
		std::vector<VkQueueFamilyProperties> FamilyProperties;

		static std::string getTypeAsString(VkPhysicalDeviceType type)
		{
			switch(type)
			{
			case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Неизвестно";
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "ВСТРОЕННЫЙ ГРАФИЧЕСКИЙ процессор";
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "ДИСКРЕТНЫЙ ГРАФИЧЕСКИЙ процессор";
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "ВИРТУАЛЬНЫЙ ГРАФИЧЕСКИЙ процессор";
			case VK_PHYSICAL_DEVICE_TYPE_CPU: return "ЦП";
			default: return "Вышли за рамки определений VkPhysicalDeviceType";
			}
		}

		std::string getDeviceName()
		{
			return (std::stringstream() << "Имя устройства: " << DeviceName
					<< "\nТип устройства: " << getTypeAsString(DeviceType)
					<< "\nId поставщика: " << VendorID
					<< "\nId устройства: " << DeviceID).str();
		}
	};

	class vkInstance {
		Vulkan *Handler;
		VkInstance Instance = nullptr;

	public:
		vkInstance(Vulkan *handler, std::vector<vkInstanceLayer> layers = {}, std::vector<vkInstanceExtension> extensions = {});
		~vkInstance();

		Vulkan* getHandler() const { return Handler; }
		VkInstance getInstance() const { return Instance; }
	};

	struct SwapchainBuffers {
		VkImage Image = nullptr;
		VkImageView View = nullptr;
		VkCommandBuffer Cmd = nullptr;
		VkFramebuffer FrameBuffer = nullptr;
	};
	
	bool NeedShutdown = false;
	asio::executor_work_guard<asio::io_context::executor_type> GuardLock;

public:
	struct {
		std::vector<vkInstanceLayer> InstanceLayers;
		std::vector<vkInstanceExtension> InstanceExtensions;
		std::vector<std::string> GLFWExtensions;
		std::vector<vkDevice> Devices;

		std::optional<vkInstance> Instance;
		GLFWwindow *Window = nullptr;
		VkSurfaceKHR Surface = nullptr;

		VkPhysicalDevice PhysicalDevice = nullptr;
		VkPhysicalDeviceMemoryProperties DeviceMemoryProperties = {0};
		VkDevice Device = nullptr;
		VkQueue DeviceQueueGraphic = VK_NULL_HANDLE;
		VkFormat SurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
		VkColorSpaceKHR SurfaceColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;

		VkCommandPool Pool = VK_NULL_HANDLE;
		VkCommandBuffer CommandBufferData = VK_NULL_HANDLE;
		VkCommandBuffer CommandBufferRender = VK_NULL_HANDLE;

		// RenderPass для экранного буфера
		VkRenderPass RenderPass = VK_NULL_HANDLE;
		VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
		std::vector<SwapchainBuffers> DrawBuffers;
		struct {
			VkImage Image = VK_NULL_HANDLE;
			VkImageView View = VK_NULL_HANDLE;
			VkDeviceMemory Memory = VK_NULL_HANDLE;
			VkFramebuffer Frame = VK_NULL_HANDLE;
		} InlineTexture;
		uint32_t
			// Количество сменяемых буферов
			DrawBufferCount = 0,
			// Текущий рабочий буфер
			DrawBufferCurrent = 0;

		std::string SwapchainChangeReport;

		const VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;
		VkImage DepthImage = VK_NULL_HANDLE;
		VkImageView DepthView = VK_NULL_HANDLE;
		VkDeviceMemory DepthMemory = VK_NULL_HANDLE;

		VkDescriptorPool ImGuiDescPool = VK_NULL_HANDLE;

		// Идентификатор потока графики
		std::thread::id ThisThread = std::this_thread::get_id();
	} Graphics;

	enum struct DrawState {
		Begin,
		Drawing,
		End
	};

	struct {
		uint32_t Width = 960, Height = 540;
		VkExtent2D FrameExtent;
		std::mutex BeforeDrawMtx;
		std::queue<std::function<void(Vulkan*)>> BeforeDraw;
		DrawState State = DrawState::Begin;
	} Screen;
		
	struct {
    	DestroyLock UseLock;
		std::thread MainThread;
		std::shared_ptr<VulkanRenderSession> RSession;
		std::unique_ptr<ServerSession> Session;

		std::list<void (Vulkan::*)()> ImGuiInterfaces;
		std::unique_ptr<ServerObj> Server;

		double MLastPosX, MLastPosY;
	} Game;

private:
	Logger LOGGER = "Vulkan";
	Settings
		// Текущие настройки
		SettingsState,
		// Новые настройки, будут применены после вызова reInit
		SettingsNext,
		// Наилучшие настройки, расчитываются единожды
		SettingsBest;

	std::optional<DynamicLibrary> LibraryVulkan;
	
	std::queue<std::function<void(Vulkan&)>> VulkanContext;

	// Объекты рисовки
	std::unordered_set<std::shared_ptr<IVulkanDependent>> ROS_Dependents;

	friend DescriptorLayout;
	friend Pipeline;
	friend ShaderModule;
	friend DescriptorPool;
	friend Buffer;

	void updateResources();

public:
	// Блокировка рендера кадра
	//SharedMutex MutexRender;

private:
	// Обработчик рендера
	void run();
	// Проверка и инициализация библиотеки Vulkan
	void checkLibrary();

	static void glfwCallbackOnResize(GLFWwindow *window, int width, int height);
	static void glfwCallbackOnMouseButton(GLFWwindow* window, int button, int action, int mods);
	static void glfwCallbackOnCursorPos(GLFWwindow* window, double xpos, double ypos);
	static void glfwCallbackOnScale(GLFWwindow* window, float xscale, float yscale);
	static void glfwCallbackOnKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void glfwCallbackOnFocus(GLFWwindow* window, int focused);
	// Применение новых настроек
	void initNextSettings();
	// Деинициализация вулкана вплоть до удаления instance
	void deInitVulkan();
	// Обработчик ошибок GLFW
	void glfwCallbackError(int error, const char *description);

	// Освобождение объектов цепочек рендера
	void freeSwapchains();
	// Создание цепочек рендера (при изменении размера окна)
	void buildSwapchains();

public:
	Vulkan(asio::io_context &ioc);
	~Vulkan();

	Vulkan(const Vulkan&) = delete;
	Vulkan(Vulkan&&) = delete;
	Vulkan& operator=(const Vulkan&) = delete;
	Vulkan& operator=(Vulkan&&) = delete;
	// Расчитывает оптимальные настройки для местного оборудования
	Settings getBestSettings();

	/* Переинициализация всех объектов вулкан в соответствии
	 * с новыми настройками */
	void reInit();

	/* Если при перестройки под новые настройки понадобится полное
	 * пересоздание контекста Vulkan - вернёт true */
	bool needFullVulkanRebuild();

	// Поиск подходящей памяти
	uint32_t memoryTypeFromProperties(uint32_t bitsOfAcceptableTypes, VkFlags bitsOfAccessMask);
	// Принудительно выполнить команды буфера
	void flushCommandBufferData();

	/* Части графических конвейеров
	Удалить можно только после полной деинициализации вулкана */
	// Загрузка шейдера
	std::shared_ptr<ShaderModule> createShader(std::string_view data);
	// Регистрация объекта зависимого от изменений в графическом конвейере
	void registerDependent(std::shared_ptr<IVulkanDependent> dependent);
	// Регистрация объекта зависимого от изменений в графическом конвейере
	Vulkan& operator<<(std::shared_ptr<IVulkanDependent> dependent);

	// Lock: MutexRender
		// Возвращает настройки текущие настройки
		const Settings& getSettings() { return SettingsState; }
		// Возвращает настройки заготовленные настройки
		Settings& getSettingsNext() { return SettingsNext; }

	bool isAlive() { return false; }

	// Добавить обработчик перед началом рисовки кадра
	void beforeDraw(std::function<void(Vulkan*)> &&callback) {
		std::lock_guard lock(Screen.BeforeDrawMtx);
		Screen.BeforeDraw.push(std::move(callback));
	}

	bool isRenderThread() {
		return std::this_thread::get_id() == Graphics.ThisThread;
	}

	void addImGUIFont(std::string_view view);

	void gui_MainMenu();
	void gui_ConnectedToServer();
};

class IVulkanDependent : public std::enable_shared_from_this<IVulkanDependent> {
protected:
	virtual void free(Vulkan *instance) = 0;
	virtual void init(Vulkan *instance) = 0;

	friend Vulkan;
	friend Pipeline;

public:
	IVulkanDependent() = default;
	virtual ~IVulkanDependent();

	IVulkanDependent(const IVulkanDependent&) = delete;
	IVulkanDependent(IVulkanDependent&&) = delete;
	IVulkanDependent& operator=(const IVulkanDependent&) = delete;
	IVulkanDependent& operator=(IVulkanDependent&&) = delete;
};

/*
	Разметка данных на входе шейдера (юниформы),
	и разметка сетов дексрипторов
*/
class DescriptorLayout : public IVulkanDependent {
protected:
	std::vector<VkDescriptorSetLayoutBinding> ShaderLayoutBindings;
	std::vector<VkPushConstantRange> ShaderPushConstants;

	VkDescriptorSetLayout DescLayout = VK_NULL_HANDLE;
	VkPipelineLayout Layout = VK_NULL_HANDLE;

protected:
	virtual void init(Vulkan *instance) override;
	virtual void free(Vulkan *instance) override;

	friend Pipeline;

public:
	DescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding> &layout = {}, const std::vector<VkPushConstantRange> &pushConstants = {});
	virtual ~DescriptorLayout();

	DescriptorLayout(const DescriptorLayout&) = delete;
	DescriptorLayout(DescriptorLayout&&) = delete;
	DescriptorLayout& operator=(const DescriptorLayout&) = delete;
	DescriptorLayout& operator=(DescriptorLayout&&) = delete;

	operator VkDescriptorSetLayout() const { return DescLayout; }
	operator VkPipelineLayout() const { return Layout; }
};

class Pipeline : public IVulkanDependent {
	std::string PipelineInfo;

protected:
	VkPipeline PipelineObj = VK_NULL_HANDLE;

	VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

protected:
	struct {
		// Настройка формата вершин шейдера
		std::vector<VkVertexInputBindingDescription> ShaderVertexBindings;
		std::vector<VkVertexInputAttributeDescription> ShaderVertexAttribute;
		// Настройка входных дескрипторов шейдера
		std::shared_ptr<DescriptorLayout> ShaderLayoutBindings;
		// Конвейер шейдеров
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

		// Настройки растеризатора
		VkPipelineRasterizationStateCreateInfo Rasterization;
		// Настройка мультисемплинга
		VkPipelineMultisampleStateCreateInfo Multisample;
		// Настройка смешивания цветов
		std::vector<VkPipelineColorBlendAttachmentState> ColorBlend;
		float ColorBlendConstants[4];
		VkLogicOp ColorBlendLogicOp;
		// Тест буфера глубины и трафарета
		VkPipelineDepthStencilStateCreateInfo DepthStencil;
		// Динамичные состояния
		std::vector<VkDynamicState> DynamicStates;
		uint32_t Subpass = 0;
	} Settings;

	virtual void free(Vulkan *instance) override;
	virtual void init(Vulkan *instance) override;

public:
	Pipeline(std::shared_ptr<DescriptorLayout> layout);
	virtual ~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline(Pipeline&&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline& operator=(Pipeline&&) = delete;

	void bind(Vulkan *instance)
	{
		vkCmdBindPipeline(instance->Graphics.CommandBufferRender, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineObj);
	}

	void bindDescriptorSets(Vulkan *instance, VkDescriptorSet *sets, size_t setCount = 1, size_t offset = 0)
	{
		vkCmdBindDescriptorSets(instance->Graphics.CommandBufferRender, VK_PIPELINE_BIND_POINT_GRAPHICS, *Settings.ShaderLayoutBindings, offset, setCount, sets, 0, nullptr);
	}

	std::string getPipelineInfo() { return PipelineInfo; }
};

/*
	Шейдер загружаемый из предкомпилированных файлов,
	планируется, что он будет существовать до конца работы игры
*/
class ShaderModule : public IVulkanDependent {
	VkShaderModule Module = VK_NULL_HANDLE;
	std::string Source;

protected:
	virtual void free(Vulkan *instance) override;
	virtual void init(Vulkan *instance) override;

public:
	ShaderModule(std::string_view view);
	virtual ~ShaderModule();

	VkShaderModule getModule() const { return Module; }
	operator VkShaderModule() const { return Module; }
};

/*
	Используются для хранения вершинных данных,
	доступны к редактированию на хосте
	Не принимает событий от вулкана
*/
class Buffer {
	Vulkan *Instance = VK_NULL_HANDLE;
	VkBuffer Buff = VK_NULL_HANDLE;
	VkDeviceMemory Memory = VK_NULL_HANDLE;
	VkDeviceSize Size = 0;

public:
	Buffer(Vulkan *instance, VkDeviceSize bufferSize, VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	virtual ~Buffer();

	Buffer(const Buffer&) = delete;
	Buffer(Buffer&&);
	Buffer& operator=(const Buffer&) = delete;
	Buffer& operator=(Buffer&&);

	VkBuffer getBuffer() const { return Buff; }
	operator VkBuffer() const { return Buff; }
	VkDeviceMemory getMemory() const { return Memory; }
	operator VkDeviceMemory() const { return Memory; }
	VkDeviceSize getSize() const { return Size; }
	operator VkDeviceSize() const { return Size; }

	operator VkDescriptorBufferInfo() const { return {Buff, 0, Size}; }

	/* Размещает данные в памяти хоста для их редактирования и возврата драйверу через unMapMemory
		Смещение, Размер, флаги
	*/ 
	uint8_t* mapMemory(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags = 0) const;
	/* Размещает данные в памяти хоста для их редактирования и возврата драйверу через unMapMemory */ 
	uint8_t* mapMemory() const { return mapMemory(0, Size, 0); }
	/* Возвращает память под управление драйверу */
	void unMapMemory() const;
};

/*
	Буфер комманд.

*/
class CommandBuffer {
	VkCommandBuffer Buffer = VK_NULL_HANDLE;
	Vulkan *Instance;
	std::vector<std::function<void()>> AfterExecute;

	VkCommandPool OffthreadPool = VK_NULL_HANDLE;

public:
	CommandBuffer(Vulkan *instance);
	~CommandBuffer();

	CommandBuffer(const CommandBuffer&) = delete;
	CommandBuffer(CommandBuffer&&) = delete;
	CommandBuffer& operator=(const CommandBuffer&) = delete;
	CommandBuffer& operator=(CommandBuffer&&) = delete;

	VkCommandBuffer getBuffer() { return Buffer; }
	operator VkCommandBuffer() { return Buffer; }

	// Выполнить команды, после вернуть управление
	void execute();
	// Начать выполнение команд без ожидания
	void executeNoAwait();

	// Выполнить после выполнения буфера
	void afterExecute(const std::function<void()> &&callback) { AfterExecute.push_back(std::move(callback)); }
};

class SimpleImage {
	Vulkan *Instance;

	VkSampler Sampler = VK_NULL_HANDLE;
	VkImage Image = VK_NULL_HANDLE;
	VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;

	VkDeviceMemory Memory = VK_NULL_HANDLE;
	VkImageView View = VK_NULL_HANDLE;
	size_t Width = 0, Height = 0;

	void postInit(const ByteBuffer &pixels, size_t width, size_t height);

public:
	SimpleImage(Vulkan *instance, std::filesystem::path filePNG);
	SimpleImage(Vulkan *instance, const ByteBuffer &pixels, size_t width, size_t height);
	virtual ~SimpleImage();

	VkSampler getSampler() const { return Sampler; }
	operator VkSampler() const { return Sampler; }
	VkImage getImage() const { return Image; }
	operator VkImage() const { return Image; }
	VkImageLayout getImageLayout() const { return ImageLayout; }
	operator VkImageLayout() const { return ImageLayout; }
	VkDeviceMemory getMemory() const { return Memory; }
	VkImageView getView() const { return View; }
	operator VkImageView() const { return View; }

	operator VkDescriptorImageInfo() const { return {Sampler, View, ImageLayout}; }

	size_t getWidth() const { return Width; }
	size_t getHeight() const { return Height; }

	static ByteBuffer loadTexture(std::filesystem::path file, size_t &width, size_t &height, bool &alpha);
};

/*
	Картинка, которую можно обновлять перед кадром
*/
class DynamicImage : public std::enable_shared_from_this<DynamicImage> {
protected:
	Vulkan *Instance;

	VkSampler Sampler = VK_NULL_HANDLE;
	VkImage Image = VK_NULL_HANDLE;
	VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;

	VkDeviceMemory Memory = VK_NULL_HANDLE;
	VkImageView View = VK_NULL_HANDLE;
	uint16_t Width = 0, Height = 0;
	bool IsFirstCreate = true;

	std::vector<std::function<bool()>> AfterRecreate;

	void changeSampler(const VkSamplerCreateInfo *sampler);

public:
	DynamicImage(Vulkan *instance, uint32_t width = 0, uint32_t height = 0, const uint32_t *rgba = nullptr);
	virtual ~DynamicImage();

	DynamicImage(const DynamicImage&) = delete;
	DynamicImage(DynamicImage&&) = delete;
	DynamicImage& operator=(const DynamicImage&) = delete;
	DynamicImage& operator=(DynamicImage&&) = delete;

	/* Пересоздаёт картинку с новым размером и данными */
	void recreateImage(uint16_t width, uint16_t height, const uint32_t *rgba);
	/* Обновляет изображение */
	void changeData(const uint32_t *rgba);
	void changeData(int32_t x, int32_t y, uint16_t width, uint16_t height, const uint32_t *rgba);
	/* Добыть изображение из текстуры */
	void readData(int32_t x, int32_t y, uint16_t width, uint16_t height, uint32_t *rgba);

	VkSampler getSampler() const { return Sampler; }
	operator VkSampler() const { return Sampler; }
	VkImage getImage() const { return Image; }
	operator VkImage() const { return Image; }
	VkImageLayout getImageLayout() const { return ImageLayout; }
	operator VkImageLayout() const { return ImageLayout; }
	VkDeviceMemory getMemory() const { return Memory; }
	VkImageView getView() const { return View; }
	operator VkImageView() const { return View; }

	operator VkDescriptorImageInfo() const { return {Sampler, View, ImageLayout}; }

	size_t getWidth() const { return Width; }
	size_t getHeight() const { return Height; }

	/*
		Если картинка будет пересоздана, то после будет вызван обработчик
		Если обработчик больше не хочет обрабатывать, пусть вернёт false
	*/
	void addCallbackOnImageDescriptorChange(std::function<bool()> &&callback);
};

class AtlasImage : public DynamicImage {
public:
	struct alignas(16) InfoSubTexture {
		uint32_t isExist : 1 = 0, _ : 31 = 0;
		uint16_t PosX = 0, PosY = 0, Width = 0, Height = 0;
		struct {
			uint16_t Enabled : 1 = 0, Frames : 15 = 0;
			uint16_t TimePerFrame = 0;
		} Animation;
	};

protected:
	// Текущие значения в атласе
	std::map<uint16_t, InfoSubTexture> SubTextures;
	std::mutex Changes;

	// Если нужно перераспределить текстуры
	bool NeedRebuild = false;
	// Если нужно перезалить uniform буфер с разметкой атласа
	bool NeedUpdateSchema = false;

	// Высоты занятости. Текстуры можно добавлять на полотно без полного пересчёта
	std::vector<uint16_t> Heights;

	// Свободные идентификаторы
	std::vector<uint16_t> FreeIds;

	// Выделить место под текстуру
	void claimTexture(uint16_t id, uint16_t width, uint16_t height);
	// По возможности сократить количество свободных идентификаторов
	void optimizeFreeIds();
	uint8_t IdsChanges = 0;

	// Здесь хранятся текстуры в чистом виде, их преоритет выше, чем у данных в атласе
	// Если есть объект без данных, значит под текстуру ещё не выделено место в атласе
	std::map<uint16_t, ByteBuffer> CachedData;

	// Если uniform буфер пересоздан
	std::vector<std::function<bool()>> AfterUniformChange;

	// Буфер со схемой
	VK::Buffer UniformSchema, HostBuffer;

	struct UniformInfo {
		// Количество текстур
		uint16_t SubsCount;
		uint16_t _;
		// Счётчик времени с разрешением 8 бит в секунду
		uint32_t Counter;

		uint16_t Width, Height;

		//std::vector<InfoSubTexture> SubsInfo;
	};

public:
	AtlasImage(Vulkan *instance);
	virtual ~AtlasImage();

	AtlasImage(const AtlasImage&) = delete;
	AtlasImage(AtlasImage&&) = delete;
	AtlasImage& operator=(const AtlasImage&) = delete;
	AtlasImage& operator=(AtlasImage&&) = delete;

	// Выделить новую текстуру в атласе
	uint16_t atlasAddTexture(uint16_t width, uint16_t height);
	// Удалить текстуру
	void atlasRemoveTexture(uint16_t id);
	// Изменить размер текстуры
	void atlasResizeTexture(uint16_t id, uint16_t width, uint16_t height);
	// Изменить данные текстуры
	void atlasChangeTextureData(uint16_t id, const uint32_t *rgba);
	// Устанавливает анимацию на текстуру, frames max 2^15
	void atlasSetAnimation(uint16_t id, bool enabled, uint16_t frames = 0, float timePerFrame = 0);
	// Вычищает все текстуры
	void atlasClear();

	// Срабатывает при изменении буфера разметки данных атласа
	void atlasAddCallbackOnUniformChange(std::function<bool()> &&callback);

	// Обновить динамичные данные перед каждым кадром (для анимаций)
	void atlasUpdateDynamicData();

	// Создаёт схему из атласа, сохраняет текстуры атласа и их положения
	ByteBuffer atlasSchemaUnload() const;
	// Восстанавливает атлас по схеме
	void atlasSchemaLoad(const ByteBuffer &buff);

	operator VkDescriptorBufferInfo() const {
		return VkDescriptorBufferInfo {
			.buffer = UniformSchema,
			.offset = 0,
			.range = UniformSchema.getSize()
		};
	}


	// Вернёт указатель на информацию, если она есть
	const InfoSubTexture* atlasGetTextureInfo(uint16_t id);
};

// Массив двумерных текстур одного размера
class ArrayImage {
	Vulkan *Instance;

	VkSampler Sampler = VK_NULL_HANDLE;
	VkImage Image = VK_NULL_HANDLE;
	VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;

	VkDeviceMemory Memory = VK_NULL_HANDLE;
	VkImageView View = VK_NULL_HANDLE;
	uint32_t Width = 0;

    std::map<std::string, uint16_t> TextureMapping;

public:
    ArrayImage(Vulkan *instance, std::filesystem::path directory);
    ~ArrayImage();

    /* Возвращает индекс текстуры в массиве по названию её файла без расширения */
    uint16_t getTextureId(const std::string &name);

	VkSampler getSampler() const { return Sampler; }
	operator VkSampler() const { return Sampler; }
	VkImage getImage() const { return Image; }
	operator VkImage() const { return Image; }
	VkImageLayout getImageLayout() const { return ImageLayout; }
	operator VkImageLayout() const { return ImageLayout; }
	VkDeviceMemory getMemory() const { return Memory; }
	VkImageView getView() const { return View; }
	operator VkImageView() const { return View; }

	operator VkDescriptorImageInfo() const { return {Sampler, View, ImageLayout}; }

	size_t getWidth() const { return Width; }
};

/*
	Накопитель вершин на время кадра, для динамично вычисляемых вершин на время одного RenderPass
*/
template<typename Vertex>
class VertexFrameCapacitor {
	Vulkan *Instance;

	std::vector<Buffer> Buffers; // Всегда должен быть какой-нибудь буфер
	Vertex *LastData = nullptr;
	size_t Offset = 0;
	int Counter = 0;

	const uint32_t CountVertexPerBuffer = 300000; // Делится и на 4 3 и на 2
 
public:
	VertexFrameCapacitor(Vulkan *instance)
		: Instance(instance)
	{
		Buffers.emplace_back(instance, CountVertexPerBuffer*sizeof(Vertex));
	}

	~VertexFrameCapacitor()
	{
		beforeStartDraw();
	}

	// Добавить список вершин
	void pushVertexs(const Vertex *vertexs, size_t count)
	{
		while(count)
		{
			// Текущая позиция в цепочке буферов
			size_t mainOffset = Offset + Counter;
			// Буфер на котором ведётся запись
			size_t bufferIndex = 0;
			for(; bufferIndex < Buffers.size(); bufferIndex++)
				if(Buffers[bufferIndex].getSize() / sizeof(Vertex) > mainOffset)
					break;
				else
					mainOffset -= Buffers[bufferIndex].getSize() / sizeof(Vertex);

			// Буферы кончились
			if(bufferIndex >= Buffers.size())
			{
				if(LastData)
					Buffers.back().unMapMemory();
				LastData = nullptr;

				Buffers.emplace_back(Instance, CountVertexPerBuffer*sizeof(Vertex));
				continue;
			}

			// Сколько места доступно для вершин
			size_t freeCount = Buffers[bufferIndex].getSize()/sizeof(Vertex)-mainOffset;

			// Место кончилось
			vkAssert(freeCount && "Место всегда должно быть");
			// if(!freeCount)
			// {
			// 	if(LastData)
			// 		Buffers[bufferIndex].unMapMemory();
			// 	LastData = nullptr;

			// 	Buffers.emplace_back(Instance, CountVertexPerBuffer*sizeof(Vertex));
			// 	continue;
			// }

			// Сколько будем записывать вершин
			size_t writeCount = std::min(count, freeCount);

			// Запишем данные
			count -= writeCount;
			Counter += writeCount;

			if(!LastData)
				LastData = (Vertex*) Buffers[bufferIndex].mapMemory(mainOffset*sizeof(Vertex), Buffers[bufferIndex].getSize()-mainOffset*sizeof(Vertex));

			std::copy(vertexs, vertexs+writeCount, LastData);
			vertexs += writeCount;
			LastData += writeCount;

			// Место в буфере кончилось
			if(count || freeCount == writeCount)
			{
				Buffers[bufferIndex].unMapMemory();
				LastData = nullptr;
			}
		}
	}

	// Добавить список вершин
	void pushVertexs(const std::vector<Vertex> &vertexs)
	{
		pushVertexs(vertexs.data(), vertexs.size());
	}

	VertexFrameCapacitor& operator<<(const std::vector<Vertex> &vertexs) { pushVertexs(vertexs); return *this; }

	// Добавить вершину
	void pushVertex(const Vertex &vertex)
	{
		pushVertexs(&vertex, 1);
	}

	VertexFrameCapacitor& operator<<(const Vertex &vertex) { pushVertex(vertex); return *this; }

	// Нарисовать то, что загрузили
	void draw(VkCommandBuffer &renderCmd)
	{
		VkDeviceSize vkOffsets =  0;

		while(Counter)
		{
			// Ищем буфер на котором начата запись
			size_t offset = Offset;
			size_t bufferIndex = 0;
			for(; bufferIndex < Buffers.size(); bufferIndex++)
				if(Buffers[bufferIndex].getSize() / sizeof(Vertex) > offset)
					break;
				else
					offset -= Buffers[bufferIndex].getSize() / sizeof(Vertex);

			VkBuffer buff = Buffers[bufferIndex].getBuffer();
			vkOffsets = offset*sizeof(Vertex);
			// Сколько будем рисовать в этом буфере
			size_t willDraw = std::min<size_t>(Counter, Buffers[bufferIndex].getSize() / sizeof(Vertex) - offset);

			vkCmdBindVertexBuffers(renderCmd, 0, 1, &buff, &vkOffsets);
			vkCmdDraw(renderCmd, willDraw, 1, 0, 0);

			// Смещаемся
			Counter -= willDraw;
			Offset += willDraw;
		}
	}

	// Перед тем как начнём отрисовку
	// Чтобы загрузить всё на устройство
	void beforeStartDraw()
	{
		if(LastData)
		{
			Buffers.back().unMapMemory();
			LastData = nullptr;
		}
	}

	void beforeRenderUpdate()
	{
		// Реалоцировать один большой буфер
		//Buffers.clear();
		Offset = 0;
		Counter = 0;
	}
};

class FontAtlas : public AtlasImage {
public:
	struct GlyphInfo {
		uint16_t TexId;
		uint16_t IsValid = false;
		int16_t Width, Height;
		int16_t PosX, PosY;
	};

protected:
	struct Face { 
		FT_Face Obj = nullptr; 
		ByteBuffer Data; 

		~Face()
		{
			if(Obj)
				FT_Done_Face(Obj);
		}
	};

	FT_Library FontLibrary = nullptr;
	std::list<Face> FontList;
	std::map<uint64_t, GlyphInfo> CharToInfo;
	size_t LastUpdate = 0;

	void clearInvalidGlyphs();

public:
	FontAtlas(Vulkan *instace);
	virtual ~FontAtlas();

	FontAtlas(const FontAtlas&) = delete;
	FontAtlas(FontAtlas&&) = delete;
	FontAtlas& operator=(const FontAtlas&) = delete;
	FontAtlas& operator=(FontAtlas&&) = delete;

	// Загружает шрифт в конец списка
	void pushFont(const std::variant<std::filesystem::path, ByteBuffer> &file);
	// Переписывает список шрифтов
	void setFontList(const std::vector<std::variant<std::filesystem::path, ByteBuffer>> &fonts);
	// Получить или сгенерировать идентификатор для символа
	GlyphInfo getGlyph(UChar wc, uint16_t size);
	// Время последнего изменения данных
	size_t getLastUpdate() { return LastUpdate; }
};

class PipelineVF : public Pipeline {
    std::string PathVertex, PathFragment;
	std::shared_ptr<ShaderModule> ShaderVertex, ShaderFragment;

protected:
	virtual void init(Vulkan *instance) override;

public:
	PipelineVF(std::shared_ptr<DescriptorLayout> layout, const std::string &vertex, const std::string &fragment);
	virtual ~PipelineVF();
};

class PipelineVGF : public Pipeline {
    std::string PathVertex, PathGeometry, PathFragment;
	std::shared_ptr<ShaderModule> ShaderVertex, ShaderGeometry, ShaderFragment;

protected:
	virtual void init(Vulkan *instance) override;

public:
	PipelineVGF(std::shared_ptr<DescriptorLayout> layout, const std::string &vertex, const std::string &geometry, const std::string &fragment);
	virtual ~PipelineVGF();
};


enum class EnumRenderStage {
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
};

} /* namespace TOS::Navie::VK */

