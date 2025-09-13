#pragma once

#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vulkan/vulkan_core.h>
#include "Abstract.hpp"
#include "TOSLib.hpp"
#include "VertexPool.hpp"
#include "glm/common.hpp"
#include "glm/fwd.hpp"
#include "../FrustumCull.h"
#include "glm/geometric.hpp"
#include <execution>

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
    Хранит модели и предоставляет их конечные варианты
*/
class ModelProvider {
    struct Model {
        // В вершинах текущей модели TexId ссылается на локальный текстурный ключ
        // 0 -> default_texture -> luavox:grass.png
        std::vector<std::string> TextureKeys;
        // Привязка локальных ключей к глобальным
        std::unordered_map<std::string, TexturePipeline> TextureMap;
        // Вершины со всеми применёнными трансформациями, с CullFace
        std::unordered_map<EnumFace, std::vector<Vertex>> Vertecies;
        // Текстуры этой модели не будут переписаны вышестоящими
        bool UniqueTextures = false;
    };

    struct ModelObject : public Model {
        // Зависимости, их трансформации (здесь может повторятся одна и таже модель)
        // и перезаписи идентификаторов текстур
        std::vector<std::tuple<ResourceId, Transformations>> Depends;

        // Те кто использовали модель как зависимость в ней отметятся
        std::vector<ResourceId> UpUse;
        // При изменении/удалении модели убрать метки с зависимостей
        std::vector<ResourceId> DownUse;
        // Для постройки зависимостей
        bool Ready = false;
    };

public:
    // Предкомпилирует модель
    Model getModel(ResourceId id) {
        std::vector<ResourceId> used;
        return getModel(id, used);
    }

    // Применяет изменения, возвращая все затронутые модели
    std::vector<AssetsModel> onModelChanges(std::vector<std::tuple<AssetsModel, Resource>> newOrChanged, std::vector<AssetsModel> lost) {
        std::vector<AssetsModel> result;

        std::move_only_function<void(ResourceId)> makeUnready;
        makeUnready = [&](ResourceId id) {
            auto iterModel = Models.find(id);
            if(iterModel == Models.end())
                return;

            if(!iterModel->second.Ready)
                return;

            result.push_back(id);

            for(ResourceId downId : iterModel->second.DownUse) {
                auto iterModel = Models.find(downId);
                if(iterModel == Models.end())
                    return;

                auto iter = std::find(iterModel->second.UpUse.begin(), iterModel->second.UpUse.end(), id);
                assert(iter != iterModel->second.UpUse.end());
                iterModel->second.UpUse.erase(iter);
            }

            for(ResourceId upId : iterModel->second.UpUse) {
                makeUnready(upId);
            }

            assert(iterModel->second.UpUse.empty());

            iterModel->second.Ready = false;
        };

        for(ResourceId lostId : lost) {
            makeUnready(lostId);
        }

        for(ResourceId lostId : lost) {
            auto iterModel = Models.find(lostId);
            if(iterModel == Models.end())
                continue;

            Models.erase(iterModel);
        }
        
        for(const auto& [key, resource] : newOrChanged) {
            makeUnready(key);
            ModelObject model;
            std::string type = "unknown";
                
            try {
                std::u8string_view data((const char8_t*) resource.data(), resource.size());
                if(data.starts_with((const char8_t*) "bm")) {
                    type = "InternalBinary";
                    // Компилированная модель внутреннего формата
                    LV::PreparedModel pm((std::u8string) data);
                    model.TextureMap = pm.CompiledTextures;
                    model.TextureKeys = {};

                    for(const PreparedModel::Cuboid& cb : pm.Cuboids) {
                        glm::vec3 min = glm::min(cb.From, cb.To), max = glm::max(cb.From, cb.To);
                        

                        for(const auto& [face, params] : cb.Faces) {
                            glm::vec2 from_uv = {params.UV[0], params.UV[1]}, to_uv = {params.UV[2], params.UV[3]};

                            uint32_t texId;
                            {
                                auto iter = std::find(model.TextureKeys.begin(), model.TextureKeys.end(), params.Texture);
                                if(iter == model.TextureKeys.end()) {
                                    texId = model.TextureKeys.size();
                                    model.TextureKeys.push_back(params.Texture);
                                } else {
                                    texId = iter-model.TextureKeys.begin();
                                }
                            }

                            std::vector<Vertex> v;
                            
                            switch(face) {
                            case EnumFace::Down:
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, max.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, min.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, max.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            case EnumFace::Up:
                                v.emplace_back(glm::vec3{min.x, max.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, min.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, max.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            case EnumFace::North:
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, min.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, min.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, min.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, min.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            case EnumFace::South:
                                v.emplace_back(glm::vec3{min.x, min.y, max.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, max.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, max.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, max.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            case EnumFace::West:
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, max.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{min.x, max.y, min.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            case EnumFace::East:
                                v.emplace_back(glm::vec3{max.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, max.z}, glm::vec2{from_uv.x, to_uv.y}, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{max.x, min.y, min.z}, from_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, max.z}, to_uv, texId);
                                v.emplace_back(glm::vec3{max.x, max.y, min.z}, glm::vec2{to_uv.x, from_uv.y}, texId);
                            break;
                            default:
                                MAKE_ERROR("EnumFace::None");
                            }

                            cb.Trs.apply(v);
                            model.Vertecies[params.Cullface].append_range(v);
                        }
                    }

        // struct Face {
        //     int TintIndex = -1;
        //     int16_t Rotation = 0;
        // };
    
        // std::vector<Transformation> Transformations;
                    
                } else if(data.starts_with((const char8_t*) "glTF")) {
                    type = "glb";

                } else if(data.starts_with((const char8_t*) "bgl")) {
                    type = "InternalGLTF";

                } else if(data.starts_with((const char8_t*) "{")) {
                    type = "InternalJson или glTF";
                    // Модель внутреннего формата или glTF
                }
            } catch(const std::exception& exc) {
                LOG.warn() << "Не удалось распарсить модель " << type << ":\n\t" << exc.what();
            }

            Models.insert({key, std::move(model)});
        }

        std::sort(result.begin(), result.end());
        auto eraseIter = std::unique(result.begin(), result.end());
        result.erase(eraseIter, result.end());
        return result;
    }

private:
    Logger LOG = "Client>ModelProvider";
    // Таблица моделей
    std::unordered_map<ResourceId, ModelObject> Models;
    uint64_t UniqId = 0;

    Model getModel(ResourceId id, std::vector<ResourceId>& used) {
        auto iterModel = Models.find(id);
        if(iterModel == Models.end()) {
            // Нет такой модели, ну и хрен с ним
            return {};
        }

        ModelObject& model = iterModel->second;
        if(!model.Ready) {
            std::vector<ResourceId> deps;
            for(const auto&[id, _] : model.Depends) {
                deps.push_back(id);
            }

            std::sort(deps.begin(), deps.end());
            auto eraseIter = std::unique(deps.begin(), deps.end());
            deps.erase(eraseIter, deps.end());

            // Отмечаемся в зависимостях
            for(ResourceId subId : deps) {
                auto iterModel = Models.find(subId);
                if(iterModel == Models.end())
                    continue;

                iterModel->second.UpUse.push_back(id);
            }

            model.Ready = true;
        }

        // Собрать зависимости
        std::vector<Model> subModels;
        used.push_back(id);

        for(const auto&[id, trans] : model.Depends) {
            if(std::find(used.begin(), used.end(), id) != used.end()) {
                // Цикл зависимостей
                continue;
            }

            Model model = getModel(id, used);

            for(auto& [face, vertecies] : model.Vertecies)
                trans.apply(vertecies);

            subModels.emplace_back(std::move(model));
        }

        subModels.push_back(model);
        used.pop_back();

        // Собрать всё воедино
        Model result;

        for(Model& subModel : subModels) {
            std::vector<ResourceId> localRelocate;

            if(subModel.UniqueTextures) {
                std::string extraKey = "#" + std::to_string(UniqId++);
                for(std::string& key : subModel.TextureKeys) {
                    key += extraKey;
                }

                std::unordered_map<std::string, TexturePipeline> newTable;
                for(auto& [key, _] : subModel.TextureMap) {
                    newTable[key + extraKey] = _;
                }

                subModel.TextureMap = std::move(newTable);
            }

            for(const std::string& key : subModel.TextureKeys) {
                auto iterKey = std::find(result.TextureKeys.begin(), result.TextureKeys.end(), key);
                if(iterKey == result.TextureKeys.end()) {
                    localRelocate.push_back(result.TextureKeys.size());
                    result.TextureKeys.push_back(key);
                } else {
                    localRelocate.push_back(iterKey-result.TextureKeys.begin());
                }
            }

            for(const auto& [face, vertecies] : subModel.Vertecies) {
                auto& resVerts = result.Vertecies[face];

                for(Vertex v : vertecies) {
                    v.TexId = localRelocate[v.TexId];
                    resVerts.push_back(v);
                }
            }

            for(auto& [key, dk] : subModel.TextureMap) {
                result.TextureMap[key] = dk;
            }
        }

        return result;
    }
};

/*
    Хранит информацию о моделях при различных состояниях нод
*/
class NodestateProvider {
public:
    struct Model {
        // В вершинах текущей модели TexId ссылается на локальный текстурный ключ
        // 0 -> default_texture -> luavox:grass.png
        std::vector<std::string> TextureKeys;
        // Привязка локальных ключей к глобальным
        std::unordered_map<std::string, TexturePipeline> TextureMap;
        // Вершины со всеми применёнными трансформациями, с CullFace
        std::unordered_map<EnumFace, std::vector<NodeVertexStatic>> Vertecies;
    };

public:
    NodestateProvider(ModelProvider& mp)
        : MP(mp)
    {}

    // Применяет изменения, возвращает изменённые описания состояний
    std::vector<AssetsNodestate> onNodestateChanges(std::vector<std::tuple<AssetsNodestate, Resource>> newOrChanged, std::vector<AssetsNodestate> lost, std::vector<AssetsModel> changedModels) {
        std::vector<AssetsNodestate> result;




        return result;
    }

    struct StateInfo {
        std::string Name;
        std::vector<std::string> Variable;
        int Variations = 0;
    };

    // Выдаёт модели в зависимости от состояний
    // statesInfo - Описание состояний ноды
    // states     - Текущие значения состояний ноды
    std::vector<Model> getModelsForNode(AssetsNodestate id, const std::vector<StateInfo>& statesInfo, const std::unordered_map<std::string, int>& states) {
        
    }

private:
    Logger LOG = "Client>NodestateProvider";
    ModelProvider& MP;
    std::unordered_map<AssetsNodestate, PreparedNodeState> Nodestates;
};

/*
    Объект, занимающийся генерацией меша на основе нод и вокселей
    Требует доступ к профилям в ServerSession (ServerSession должен быть заблокирован только на чтение)
    Также доступ к идентификаторам текстур в VulkanRenderSession и моделей по состояниям
    Очередь чанков, ожидающих перерисовку. Возвращает готовые вершинные данные.
*/
struct ChunkMeshGenerator {
    // Данные рендера чанка
    struct ChunkObj_t {
        // Идентификатор запроса (на случай если запрос просрочился и чанк уже был удалён)
        uint32_t RequestId = 0;
        // Мир
        WorldId_t WId;
        // Позиция чанка в мире
        Pos::GlobalChunk Pos;
        // Сортированный список уникальных значений
        std::vector<DefVoxelId> VoxelDefines;
        // Вершины
        std::vector<VoxelVertexPoint> VoxelVertexs;
        // Ноды
        std::vector<DefNodeId> NodeDefines;
        // Вершины нод
        std::vector<NodeVertexStatic> NodeVertexs;
        // Индексы
        std::variant<std::vector<uint16_t>, std::vector<uint32_t>> NodeIndexes;
    };

    // Очередь чанков на перерисовку
    TOS::SpinlockObject<std::queue<std::tuple<WorldId_t, Pos::GlobalChunk, uint32_t>>> Input;
    // Выход
    TOS::SpinlockObject<std::vector<ChunkObj_t>> Output;

public:
    ChunkMeshGenerator(IServerSession* serverSession)
        :   SS(serverSession) 
    {
        assert(serverSession);
    }

    ~ChunkMeshGenerator() {
        assert(Threads.empty());
    }

    // Меняет количество обрабатывающих потоков
    void changeThreadsCount(uint8_t threads);

    void prepareTickSync() {
        Sync.Stop = true;
    }

    void pushStageTickSync() {
        std::unique_lock lock(Sync.Mutex);
        Sync.CV_CountInRun.wait(lock, [&]() { return Sync.CountInRun == 0; });
    }

    void endTickSync() {
        Sync.Stop = false;
        Sync.CV_CountInRun.notify_all();
    }

private:
    struct {
        std::mutex Mutex;
        // Если нужно остановить пул потоков, вызывается NeedShutdown
        volatile bool NeedShutdown = false, Stop = false;
        volatile uint8_t CountInRun = 0;
        std::condition_variable CV_CountInRun;
    } Sync;

    IServerSession *SS;
    std::vector<std::thread> Threads;

    void run(uint8_t id);
};

/*
    Модуль обрабатывает рендер чанков
*/
class ChunkPreparator {
public:
    struct TickSyncData {
        // Профили на которые повлияли изменения, по ним нужно пересчитать чанки
        std::vector<DefVoxelId> ChangedVoxels;
        std::vector<DefNodeId> ChangedNodes;

        std::unordered_map<WorldId_t, std::vector<Pos::GlobalChunk>> ChangedChunks;
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> LostRegions;
    };

public:
    ChunkPreparator(Vulkan* vkInst, IServerSession* serverSession) 
        :   VkInst(vkInst),
            CMG(serverSession),
            VertexPool_Voxels(vkInst),
            VertexPool_Nodes(vkInst),
            IndexPool_Nodes_16(vkInst),
            IndexPool_Nodes_32(vkInst)
    {
        assert(vkInst);
        assert(serverSession);

        CMG.changeThreadsCount(1);

        const VkCommandPoolCreateInfo infoCmdPool =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = VkInst->getSettings().QueueGraphics
        };

        vkAssert(!vkCreateCommandPool(VkInst->Graphics.Device, &infoCmdPool, nullptr, &CMDPool));
    }

    ~ChunkPreparator() {
        CMG.changeThreadsCount(0);

        if(CMDPool)
            vkDestroyCommandPool(VkInst->Graphics.Device, CMDPool, nullptr);
    }


    void prepareTickSync() {
        CMG.prepareTickSync();
    }

    void pushStageTickSync() {
        CMG.pushStageTickSync();
    }

    void tickSync(const TickSyncData& data);

    // Готовность кадров определяет когда можно удалять ненужные ресурсы, которые ещё используются в рендере
    void pushFrame();

    // Выдаёт буферы для рендера в порядке от ближнего к дальнему. distance - радиус в регионах
    std::pair<
        std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, uint32_t>>,
        std::vector<std::tuple<Pos::GlobalChunk, std::pair<VkBuffer, int>, std::pair<VkBuffer, int>, bool, uint32_t>>
    > getChunksForRender(WorldId_t worldId, Pos::Object pos, uint8_t distance, glm::mat4 projView, Pos::GlobalRegion x64offset);

private:
    static constexpr uint8_t FRAME_COUNT_RESOURCE_LATENCY = 6;

    Vulkan* VkInst;
    VkCommandPool CMDPool = nullptr;

    // Генератор вершин чанков
    ChunkMeshGenerator CMG;

    // Буферы для хранения вершин
    VertexPool<VoxelVertexPoint> VertexPool_Voxels;
    VertexPool<NodeVertexStatic> VertexPool_Nodes;
    IndexPool<uint16_t> IndexPool_Nodes_16;
    IndexPool<uint32_t> IndexPool_Nodes_32;

    struct ChunkObj_t {
        std::vector<DefVoxelId> Voxels;
        VertexPool<VoxelVertexPoint>::Pointer VoxelPointer;
        std::vector<DefNodeId> Nodes;
        VertexPool<NodeVertexStatic>::Pointer NodePointer;
        std::variant<IndexPool<uint16_t>::Pointer, IndexPool<uint32_t>::Pointer> NodeIndexes;
    };

    // Склад указателей на вершины чанков
    std::unordered_map<WorldId_t,
        std::unordered_map<Pos::GlobalRegion, std::array<ChunkObj_t, 4*4*4>>
    > ChunksMesh;

    uint8_t FrameRoulette = 0;
    // Вершины, ожидающие удаления по прошествию какого-то количества кадров
    std::vector<VertexPool<VoxelVertexPoint>::Pointer> VPV_ToFree[FRAME_COUNT_RESOURCE_LATENCY];
    std::vector<std::tuple<
        VertexPool<NodeVertexStatic>::Pointer,
        std::variant<IndexPool<uint16_t>::Pointer, IndexPool<uint32_t>::Pointer>
    >> VPN_ToFree[FRAME_COUNT_RESOURCE_LATENCY];

    // Следующий идентификатор запроса
    uint32_t NextRequest = 0;
    // Список ожидаемых чанков. Если регион был потерян, следующая его запись получит
    // новый идентификатор (при отсутствии записи готовые чанки с MCMG будут проигнорированы)
    std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, uint32_t>> Requests;
};

/*
    Модуль, рисующий то, что предоставляет IServerSession
*/
class VulkanRenderSession : public IRenderSession {
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

    ChunkPreparator CP;
    ModelProvider MP;

    AtlasImage MainTest, LightDummy;
    Buffer TestQuad;
    std::optional<Buffer> TestVoxel;

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

    std::map<AssetsTexture, uint16_t> ServerToAtlas;

public:
    WorldPCO PCO;
    WorldId_t WI = 0;
    glm::vec3 PlayerPos = glm::vec3(0);

public:
    VulkanRenderSession(Vulkan *vkInst, IServerSession *serverSession);
    virtual ~VulkanRenderSession();

    virtual void prepareTickSync() override;
    virtual void pushStageTickSync() override;
    virtual void tickSync(const TickSyncData& data) override;

    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);
    void pushStage(EnumRenderStage stage);

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube>& cubes);

private:
    void updateDescriptor_MainAtlas();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}