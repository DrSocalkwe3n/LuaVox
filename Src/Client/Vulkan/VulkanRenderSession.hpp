#pragma once

#include "Client/AssetsManager.hpp"
#include "Client/Abstract.hpp"
#include "Common/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vulkan/vulkan_core.h>
#include "Client/Vulkan/AtlasPipeline/PipelinedTextureAtlas.hpp"
#include "Abstract.hpp"
#include "TOSLib.hpp"
#include "VertexPool.hpp"
#include "assets.hpp"
#include "glm/common.hpp"
#include "glm/fwd.hpp"
#include "../FrustumCull.h"
#include "glm/geometric.hpp"
#include "png++/image.hpp"
#include <execution>
#include <MaxRectsBinPack.h>

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
public:
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

public:
    // Предкомпилирует модель
    Model getModel(ResourceId id) {
        std::vector<ResourceId> used;
        return getModel(id, used);
    }

    // Применяет изменения, возвращая все затронутые модели
    std::vector<AssetsModel> onModelChanges(std::vector<AssetsModelUpdate> entries) {
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
        
        for(const AssetsModelUpdate& entry : entries) {
            const AssetsModel key = entry.Id;
            result.push_back(key);

            makeUnready(key);
            ModelObject model;
            const HeadlessModel& hm = entry.Model;
            const HeadlessModel::Header& header = entry.Header;
                
            try {
                model.TextureMap.clear();
                model.TextureMap.reserve(hm.Textures.size());
                for(const auto& [tkey, id] : hm.Textures) {
                    TexturePipeline pipe;
                    if(id < header.TexturePipelines.size()) {
                        pipe.Pipeline = header.TexturePipelines[id];
                    } else {
                        LOG.warn() << "Model texture pipeline id out of range: model=" << key
                            << " local=" << id
                            << " pipelines=" << header.TexturePipelines.size();
                        pipe.BinTextures.push_back(id);
                    }
                    model.TextureMap.emplace(tkey, std::move(pipe));
                }
                model.TextureKeys = {};

                for(const HeadlessModel::Cuboid& cb : hm.Cuboids) {
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

                            auto addQuad = [&](const glm::vec3& p0,
                                const glm::vec3& p1,
                                const glm::vec3& p2,
                                const glm::vec3& p3,
                                const glm::vec2& uv0,
                                const glm::vec2& uv1,
                                const glm::vec2& uv2,
                                const glm::vec2& uv3) {
                                v.emplace_back(p0, uv0, texId);
                                v.emplace_back(p1, uv1, texId);
                                v.emplace_back(p2, uv2, texId);
                                v.emplace_back(p0, uv0, texId);
                                v.emplace_back(p2, uv2, texId);
                                v.emplace_back(p3, uv3, texId);
                            };

                            const float x0 = min.x;
                            const float x1 = max.x;
                            const float y0 = min.y;
                            const float y1 = max.y;
                            const float z0 = min.z;
                            const float z1 = max.z;
                            const float u0 = from_uv.x;
                            const float v0 = from_uv.y;
                            const float u1 = to_uv.x;
                            const float v1 = to_uv.y;

                            switch(face) {
                            case EnumFace::Up:
                                addQuad({x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
                                    {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1});
                                break;
                            case EnumFace::Down:
                                addQuad({x0, y0, z1}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
                                    {u0, v0}, {u0, v1}, {u1, v1}, {u1, v0});
                                break;
                            case EnumFace::East:
                                addQuad({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
                                    {u0, v0}, {u0, v1}, {u1, v1}, {u1, v0});
                                break;
                            case EnumFace::West:
                                addQuad({x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {x0, y0, z0},
                                    {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1});
                                break;
                            case EnumFace::South:
                                addQuad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
                                    {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1});
                                break;
                            case EnumFace::North:
                                addQuad({x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0},
                                    {u0, v0}, {u0, v1}, {u1, v1}, {u1, v0});
                                break;
                            default:
                                MAKE_ERROR("EnumFace::None");
                            }

                            cb.Trs.apply(v);
                            model.Vertecies[params.Cullface].append_range(v);
                        }
                }

                if(!hm.SubModels.empty()) {
                    model.Depends.reserve(hm.SubModels.size());
                    for(const auto& sub : hm.SubModels) {
                        if(sub.Id >= header.Models.size()) {
                            LOG.warn() << "Model sub-model id out of range: model=" << key
                                << " local=" << sub.Id
                                << " deps=" << header.Models.size();
                            continue;
                        }
                        model.Depends.emplace_back(header.Models[sub.Id], Transformations{});
                    }
                }

        // struct Face {
        //     int TintIndex = -1;
        //     int16_t Rotation = 0;
        // };
    
        // std::vector<Transformation> Transformations;
                    
            } catch(const std::exception& exc) {
                LOG.warn() << "Не удалось собрать модель:\n\t" << exc.what();
                continue;
            }

            {
                static std::atomic<uint32_t> debugModelLogCount = 0;
                uint32_t idx = debugModelLogCount.fetch_add(1);
                if(idx < 128) {
                    size_t vertexCount = 0;
                    for(const auto& [_, verts] : model.Vertecies)
                        vertexCount += verts.size();
                    LOG.debug() << "Model loaded id=" << key
                        << " verts=" << vertexCount
                        << " texKeys=" << model.TextureKeys.size()
                        << " pipelines=" << header.TexturePipelines.size();
                }
            }

            if(model.Vertecies.empty()) {
                static std::atomic<uint32_t> debugEmptyModelLogCount = 0;
                uint32_t idx = debugEmptyModelLogCount.fetch_add(1);
                if(idx < 128) {
                    LOG.warn() << "Model has empty geometry id=" << key
                        << " pipelines=" << header.TexturePipelines.size();
                }
            }

            Models.insert_or_assign(key, std::move(model));
        }

        std::sort(result.begin(), result.end());
        auto eraseIter = std::unique(result.begin(), result.end());
        result.erase(eraseIter, result.end());
        return result;
    }

private:
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

    Logger LOG = "Client>ModelProvider";
    // Таблица моделей
    std::unordered_map<ResourceId, ModelObject> Models;
    std::unordered_set<ResourceId> MissingModelsLogged;
    uint64_t UniqId = 0;

    Model getModel(ResourceId id, std::vector<ResourceId>& used) {
        auto iterModel = Models.find(id);
        if(iterModel == Models.end()) {
            // Нет такой модели, ну и хрен с ним
            if(MissingModelsLogged.insert(id).second) {
                LOG.warn() << "Missing model id=" << id;
            }
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

/**
    Обработчик текстурных атласов для моделей

    Функция выделения места в полном объёме.

    Перед отрисовкой кадра выставить команды ожидания завершения передачи данных на GPU, до вызова стадии FRAGMENT_STAGE


    Одновременно может проходить два рендера (двойные копии ресурсов)
    Когда одна копия освободилась, в неё начинается запись

    Два атласа, постоянно редактируются меж кадрами
    2D_ARRAY и одну текстуру на стороне хоста для обмена информацией
    Ужатие hd текстур

    Хранить все текстуры в оперативке
*/
class TextureProvider {
public:
    struct TextureUpdate {
        AssetsTexture Id = 0;
        uint16_t Width = 0;
        uint16_t Height = 0;
        std::vector<uint32_t> Pixels;
        std::string Domain;
        std::string Key;
    };

    TextureProvider(Vulkan* inst, VkDescriptorPool descPool)
        : Inst(inst), DescPool(descPool)
    {
        assert(inst);

        {
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
                Inst->Graphics.Device, &descriptorLayout, nullptr, &DescLayout));
        }

        {
            VkDescriptorSetAllocateInfo ciAllocInfo =
            {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = DescPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &DescLayout
            };

            vkAssert(!vkAllocateDescriptorSets(Inst->Graphics.Device, &ciAllocInfo, &Descriptor));
        }

        {
            TextureAtlas::Config cfg;
            cfg.MaxTextureId = 1 << 18;
            AtlasStaging = std::make_shared<SharedStagingBuffer>(
                Inst->Graphics.Device,
                Inst->Graphics.PhysicalDevice
            );
            Atlas = std::make_unique<PipelinedTextureAtlas>(
                TextureAtlas(Inst->Graphics.Device, Inst->Graphics.PhysicalDevice, cfg, {}, AtlasStaging)
            );
        }

        {
            const VkFenceCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            vkAssert(!vkCreateFence(Inst->Graphics.Device, &info, nullptr, &UpdateFence));
        }

        NeedsUpload = true;
    }

    ~TextureProvider() {
        if(UpdateFence)
            vkDestroyFence(Inst->Graphics.Device, UpdateFence, nullptr);

        if(DescLayout)
            vkDestroyDescriptorSetLayout(Inst->Graphics.Device, DescLayout, nullptr);
    }

    VkDescriptorSetLayout getDescriptorLayout() const {
        return DescLayout;
    }

    VkDescriptorSet getDescriptorSet() const {
        return Descriptor;
    }

    uint32_t getTextureId(const TexturePipeline& pipe) {
        std::lock_guard lock(Mutex);
        bool animated = isAnimatedPipeline(pipe);
        if(!animated) {
            auto iter = PipelineToAtlas.find(pipe);
            if(iter != PipelineToAtlas.end())
                return iter->second;
        }

        ::HashedPipeline hashed = makeHashedPipeline(pipe);
        uint32_t atlasId = Atlas->getByPipeline(hashed);

        uint32_t result = atlasId;
        if(Atlas && result >= Atlas->maxTextureId()) {
            LOG.warn() << "Atlas texture id overflow: " << atlasId;
            result = Atlas->reservedOverflowId();
        }

        if(!animated)
            PipelineToAtlas.emplace(pipe, result);
        NeedsUpload = true;
        return result;
    }

    uint32_t getAtlasMaxLayers() const {
        std::lock_guard lock(Mutex);
        return Atlas ? Atlas->maxLayers() : 0u;
    }

    uint32_t getAtlasLayerId(uint32_t layer) const {
        std::lock_guard lock(Mutex);
        if(!Atlas)
            return TextureAtlas::kOverflowId;
        if(layer >= Atlas->maxLayers())
            return Atlas->reservedOverflowId();
        return Atlas->reservedLayerId(layer);
    }

    void requestAtlasLayerCount(uint32_t layers) {
        std::lock_guard lock(Mutex);
        if(Atlas)
            Atlas->requestLayerCount(layers);
    }

    // Применяет изменения, возвращая все затронутые модели
    std::vector<AssetsTexture> onTexturesChanges(std::vector<AssetsTextureUpdate> entries) {
        std::lock_guard lock(Mutex);
        std::vector<AssetsTexture> result;

        for(auto& entry : entries) {
            const AssetsTexture key = entry.Id;
            result.push_back(key);

            if(entry.Width == 0 || entry.Height == 0 || entry.Pixels.empty())
                continue;

            Atlas->updateTexture(key, StoredTexture(
                entry.Width,
                entry.Height,
                std::move(entry.Pixels)
            ));

            bool animated = false;
            AnimatedSources.erase(key);

            NeedsUpload = true;

            static std::atomic<uint32_t> debugTextureLogCount = 0;
            uint32_t idx = debugTextureLogCount.fetch_add(1);
            if(idx < 128) {
                LOG.debug() << "Texture loaded id=" << key
                    << " size=" << entry.Width << 'x' << entry.Height
                    << " animated=" << (animated ? 1 : 0);
            }
        }

        std::sort(result.begin(), result.end());
        auto eraseIter = std::unique(result.begin(), result.end());
        result.erase(eraseIter, result.end());

        return result;
    }

    void update(double timeSeconds) {
        std::lock_guard lock(Mutex);
        if(!Atlas)
            return;

        if(Atlas->updateAnimatedPipelines(timeSeconds))
            NeedsUpload = true;

        if(!NeedsUpload)
            return;

        Atlas->flushNewPipelines();

        VkCommandBufferAllocateInfo allocInfo {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            nullptr,
            Inst->Graphics.Pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1
        };

        VkCommandBuffer commandBuffer;
        vkAssert(!vkAllocateCommandBuffers(Inst->Graphics.Device, &allocInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            nullptr
        };

        vkAssert(!vkBeginCommandBuffer(commandBuffer, &beginInfo));

        TextureAtlas::DescriptorOut desc = Atlas->flushUploadsAndBarriers(commandBuffer);

        vkAssert(!vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            0, nullptr,
            nullptr,
            1,
            &commandBuffer,
            0,
            nullptr
        };

        {
            auto lockQueue = Inst->Graphics.DeviceQueueGraphic.lock();
            vkAssert(!vkQueueSubmit(*lockQueue, 1, &submitInfo, UpdateFence));
        }

        vkAssert(!vkWaitForFences(Inst->Graphics.Device, 1, &UpdateFence, VK_TRUE, UINT64_MAX));
        vkAssert(!vkResetFences(Inst->Graphics.Device, 1, &UpdateFence));

        vkFreeCommandBuffers(Inst->Graphics.Device, Inst->Graphics.Pool, 1, &commandBuffer);

        Atlas->notifyGpuFinished();
        updateDescriptor(desc);

        NeedsUpload = false;
    }

private:
    struct AnimatedSource {
        uint16_t FrameW = 0;
        uint16_t FrameH = 0;
        uint16_t FrameCount = 0;
        uint16_t FpsQ = 0;
        uint16_t Flags = 0;
    };

    static std::optional<AnimatedSource> getDefaultAnimation(std::string_view key, uint32_t width, uint32_t height) {
        if(auto slash = key.find_last_of('/'); slash != std::string_view::npos)
            key = key.substr(slash + 1);

        if(key == "fire_0.png") {
            AnimatedSource anim;
            anim.FrameW = static_cast<uint16_t>(width);
            anim.FrameH = static_cast<uint16_t>(width);
            anim.FrameCount = static_cast<uint16_t>(width ? height / width : 0);
            anim.FpsQ = static_cast<uint16_t>(12 * 256);
            anim.Flags = 0;
            return anim;
        }

        if(key == "lava_still.png") {
            AnimatedSource anim;
            anim.FrameW = static_cast<uint16_t>(width);
            anim.FrameH = static_cast<uint16_t>(width);
            anim.FrameCount = static_cast<uint16_t>(width ? height / width : 0);
            anim.FpsQ = static_cast<uint16_t>(8 * 256);
            anim.Flags = 0;
            return anim;
        }

        if(key == "water_still.png") {
            AnimatedSource anim;
            anim.FrameW = static_cast<uint16_t>(width);
            anim.FrameH = static_cast<uint16_t>(width);
            anim.FrameCount = static_cast<uint16_t>(width ? height / width : 0);
            anim.FpsQ = static_cast<uint16_t>(8 * 256);
            anim.Flags = TexturePipelineProgram::AnimSmooth;
            return anim;
        }

        return std::nullopt;
    }

    bool isAnimatedPipeline(const TexturePipeline& pipe) const {
        if(!pipe.Pipeline.empty())
            return false;
        if(pipe.BinTextures.size() != 1)
            return false;
        return AnimatedSources.contains(pipe.BinTextures.front());
    }

    ::HashedPipeline makeHashedPipeline(const TexturePipeline& pipe) const {
        ::Pipeline pipeline;

        if(!pipe.Pipeline.empty()) {
            const auto* bytes = reinterpret_cast<const ::detail::Word*>(pipe.Pipeline.data());
            pipeline._Pipeline.assign(bytes, bytes + pipe.Pipeline.size());
        }

        if(pipeline._Pipeline.empty()) {
            if(!pipe.BinTextures.empty()) {
                AssetsTexture texId = pipe.BinTextures.front();
                auto animIter = AnimatedSources.find(texId);
                if(animIter != AnimatedSources.end()) {
                    const auto& anim = animIter->second;
                    pipeline._Pipeline.clear();
                    pipeline._Pipeline.reserve(1 + 1 + 3 + 2 + 2 + 2 + 2 + 1 + 1);
                    auto emit16 = [&](uint16_t v) {
                        pipeline._Pipeline.push_back(static_cast<::detail::Word>(v & 0xFFu));
                        pipeline._Pipeline.push_back(static_cast<::detail::Word>((v >> 8) & 0xFFu));
                    };
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>(::detail::Op16::Base_Anim));
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>(::detail::SrcKind16::TexId));
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>(texId & 0xFFu));
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>((texId >> 8) & 0xFFu));
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>((texId >> 16) & 0xFFu));
                    emit16(anim.FrameW);
                    emit16(anim.FrameH);
                    emit16(anim.FrameCount);
                    emit16(anim.FpsQ);
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>(anim.Flags & 0xFFu));
                    pipeline._Pipeline.push_back(static_cast<::detail::Word>(::detail::Op16::End));
                } else {
                    pipeline = ::Pipeline(texId);
                }
            }
        }

        return ::HashedPipeline(pipeline);
    }

    void updateDescriptor(const TextureAtlas::DescriptorOut& desc) {
        VkWriteDescriptorSet writes[2] = {};

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = Descriptor;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &desc.ImageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = Descriptor;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &desc.EntriesInfo;

        vkUpdateDescriptorSets(Inst->Graphics.Device, 2, writes, 0, nullptr);
    }

private:
    Vulkan* Inst = nullptr;
    VkDescriptorPool DescPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescLayout = VK_NULL_HANDLE;
    VkDescriptorSet Descriptor = VK_NULL_HANDLE;
    VkFence UpdateFence = VK_NULL_HANDLE;

    std::shared_ptr<SharedStagingBuffer> AtlasStaging;
    std::unique_ptr<PipelinedTextureAtlas> Atlas;
    std::unordered_map<TexturePipeline, uint32_t> PipelineToAtlas;
    std::unordered_map<AssetsTexture, AnimatedSource> AnimatedSources;

    bool NeedsUpload = false;
    Logger LOG = "Client>TextureProvider";
    mutable std::mutex Mutex;
};


/*
    Хранит информацию о моделях при различных состояниях нод
*/
class NodestateProvider {
public:
    NodestateProvider(ModelProvider& mp, TextureProvider& tp)
        : MP(mp), TP(tp)
    {}

    // Применяет изменения, возвращает изменённые описания состояний
    std::vector<AssetsNodestate> onNodestateChanges(std::vector<AssetsNodestateUpdate> newOrChanged, std::vector<AssetsModel> changedModels) {
        std::vector<AssetsNodestate> result;
        
        for(const AssetsNodestateUpdate& entry : newOrChanged) {
            const AssetsNodestate key = entry.Id;
            result.push_back(key);

            PreparedNodeState nodestate;
            static_cast<HeadlessNodeState&>(nodestate) = entry.Nodestate;
            nodestate.LocalToModel.assign(entry.Header.Models.begin(), entry.Header.Models.end());

            Nodestates.insert_or_assign(key, std::move(nodestate));
            if(key < 64) {
                auto iter = Nodestates.find(key);
                if(iter != Nodestates.end()) {
                    LOG.debug() << "Nodestate loaded id=" << key
                        << " routes=" << iter->second.Routes.size()
                        << " models=" << iter->second.LocalToModel.size();
                }
            }
        }

        if(!changedModels.empty()) {
            std::unordered_set<AssetsModel> changed;
            changed.reserve(changedModels.size());
            for(AssetsModel modelId : changedModels)
                changed.insert(modelId);

            for(const auto& [nodestateId, nodestate] : Nodestates) {
                for(AssetsModel modelId : nodestate.LocalToModel) {
                    if(changed.contains(modelId)) {
                        result.push_back(nodestateId);
                        break;
                    }
                }
            }
        }

        std::sort(result.begin(), result.end());
        auto eraseIter = std::unique(result.begin(), result.end());
        result.erase(eraseIter, result.end());

        return result;
    }

    // Выдаёт модели в зависимости от состояний
    // statesInfo - Описание состояний ноды
    // states     - Текущие значения состояний ноды
    std::vector<std::vector<std::pair<float, std::unordered_map<EnumFace, std::vector<NodeVertexStatic>>>>> getModelsForNode(AssetsNodestate id, const std::vector<NodeStateInfo>& statesInfo, const std::unordered_map<std::string, int32_t>& states) {
        auto iterNodestate = Nodestates.find(id);
        if(iterNodestate == Nodestates.end()) {
            if(MissingNodestateLogged.insert(id).second) {
                LOG.warn() << "Missing nodestate id=" << id;
            }
            return {};
        }

        PreparedNodeState& nodestate = iterNodestate->second;
        std::vector<uint16_t> routes = nodestate.getModelsForState(statesInfo, states);
        if(routes.empty()) {
            int32_t metaValue = 0;
            if(auto iterMeta = states.find("meta"); iterMeta != states.end())
                metaValue = iterMeta->second;
            uint64_t key = (uint64_t(id) << 32) | (uint32_t(metaValue) & 0xffffffffu);
            if(EmptyRouteLogged.insert(key).second) {
                LOG.warn() << "No nodestate routes id=" << id
                    << " meta=" << metaValue
                    << " total_routes=" << nodestate.Routes.size();
            }
        }
        std::vector<std::vector<std::pair<float, std::unordered_map<EnumFace, std::vector<NodeVertexStatic>>>>> result;

        std::unordered_map<TexturePipeline, uint32_t> pipelineResolveCache;

        auto appendModel = [&](AssetsModel modelId, const std::vector<Transformation>& transforms, std::unordered_map<EnumFace, std::vector<NodeVertexStatic>>& out) {
            ModelProvider::Model model = MP.getModel(modelId);
            if(model.Vertecies.empty()) {
                if(MissingModelGeometryLogged.insert(modelId).second) {
                    LOG.warn() << "Model has no geometry id=" << modelId;
                }
            }
            Transformations trf{transforms};

            for(auto& [l, r] : model.Vertecies) {
                trf.apply(r);

                // Позиция -224 ~ 288; 64 позиций в одной ноде, 7.5 метров в ряд
                for(const Vertex& v : r) {
                    NodeVertexStatic vert;

                    vert.FX = (v.Pos.x + 16.0f) * 2.0f + 224.0f;
                    vert.FY = (v.Pos.y + 16.0f) * 2.0f + 224.0f;
                    vert.FZ = (v.Pos.z + 16.0f) * 2.0f + 224.0f;

                    vert.TU = std::clamp<int32_t>(v.UV.x * (1 << 11), 0, (1 << 16) - 1);
                    vert.TV = std::clamp<int32_t>(v.UV.y * (1 << 11), 0, (1 << 16) - 1);

                    const TexturePipeline& pipe = model.TextureMap[model.TextureKeys[v.TexId]];
                    if(auto iterPipe = pipelineResolveCache.find(pipe); iterPipe != pipelineResolveCache.end()) {
                        vert.Tex = iterPipe->second;
                    } else {
                        vert.Tex = TP.getTextureId(pipe);
                        pipelineResolveCache[pipe] = vert.Tex;
                    }

                    out[l].push_back(vert);
                }
            }
        };

        auto resolveModelId = [&](uint16_t localId, AssetsModel& outId) -> bool {
            if(localId >= nodestate.LocalToModel.size())
                return false;
            outId = nodestate.LocalToModel[localId];
            return true;
        };

        for(uint16_t routeId : routes) {
            if(routeId >= nodestate.Routes.size())
                continue;

            std::vector<std::pair<float, std::unordered_map<EnumFace, std::vector<NodeVertexStatic>>>> routeModels;
            const auto& route = nodestate.Routes[routeId];
            for(const auto& [w, m] : route.second) {
                std::unordered_map<EnumFace, std::vector<NodeVertexStatic>> out;

                if(const PreparedNodeState::Model* ptr = std::get_if<PreparedNodeState::Model>(&m)) {
                    AssetsModel modelId;
                    if(resolveModelId(ptr->Id, modelId)) {
                        appendModel(modelId, ptr->Transforms, out);
                    } else {
                        uint64_t missKey = (uint64_t(id) << 32) | ptr->Id;
                        if(MissingLocalModelMapLogged.insert(missKey).second) {
                            LOG.warn() << "Missing model mapping nodestate=" << id
                                << " local=" << ptr->Id
                                << " locals=" << nodestate.LocalToModel.size();
                        }
                    }
                } else if(const PreparedNodeState::VectorModel* ptr = std::get_if<PreparedNodeState::VectorModel>(&m)) {
                    for(const auto& sub : ptr->Models) {
                        AssetsModel modelId;
                        if(!resolveModelId(sub.Id, modelId)) {
                            uint64_t missKey = (uint64_t(id) << 32) | sub.Id;
                            if(MissingLocalModelMapLogged.insert(missKey).second) {
                                LOG.warn() << "Missing model mapping nodestate=" << id
                                    << " local=" << sub.Id
                                    << " locals=" << nodestate.LocalToModel.size();
                            }
                            continue;
                        }

                        std::vector<Transformation> transforms = sub.Transforms;
                        transforms.insert(transforms.end(), ptr->Transforms.begin(), ptr->Transforms.end());
                        appendModel(modelId, transforms, out);
                    }
                }

                /// TODO: uvlock
                routeModels.emplace_back(w, std::move(out));
            }

            result.push_back(std::move(routeModels));
        }

        return result;
    }

    uint32_t getTextureId(AssetsTexture texId) {
        if(texId == 0)
            return 0;

        TexturePipeline pipe;
        pipe.BinTextures.push_back(texId);
        return TP.getTextureId(pipe);
    }

    bool hasNodestate(AssetsNodestate id) const {
        return Nodestates.contains(id);
    }

private:
    Logger LOG = "Client>NodestateProvider";
    ModelProvider& MP;
    TextureProvider& TP;
    std::unordered_map<AssetsNodestate, PreparedNodeState> Nodestates;
    std::unordered_set<AssetsNodestate> MissingNodestateLogged;
    std::unordered_set<uint64_t> MissingLocalModelMapLogged;
    std::unordered_set<AssetsModel> MissingModelGeometryLogged;
    std::unordered_set<uint64_t> EmptyRouteLogged;
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

    void setNodestateProvider(NodestateProvider* provider) {
        NSP = provider;
    }

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
    NodestateProvider* NSP = nullptr;
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

        CMG.changeThreadsCount(3);
    }

    ~ChunkPreparator() {
        CMG.changeThreadsCount(0);
    }


    void prepareTickSync() {
        CMG.prepareTickSync();
    }

    void pushStageTickSync() {
        CMG.pushStageTickSync();
    }

    void setNodestateProvider(NodestateProvider* provider) {
        CMG.setNodestateProvider(provider);
    }

    void tickSync(const TickSyncData& data);
    void notifyGpuFinished() {
        resetVertexStaging();
        VertexPool_Voxels.notifyGpuFinished();
        VertexPool_Nodes.notifyGpuFinished();
        IndexPool_Nodes_16.notifyGpuFinished();
        IndexPool_Nodes_32.notifyGpuFinished();
    }
    void flushUploadsAndBarriers(VkCommandBuffer commandBuffer) {
        VertexPool_Voxels.flushUploadsAndBarriers(commandBuffer);
        VertexPool_Nodes.flushUploadsAndBarriers(commandBuffer);
        IndexPool_Nodes_16.flushUploadsAndBarriers(commandBuffer);
        IndexPool_Nodes_32.flushUploadsAndBarriers(commandBuffer);
    }

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
    std::unique_ptr<TextureProvider> TP;
    std::unique_ptr<NodestateProvider> NSP;

    AtlasImage LightDummy;
    Buffer TestQuad;
    std::optional<Buffer> TestVoxel;
    std::optional<Buffer> AtlasLayersPreview;
    uint32_t AtlasLayersPreviewCount = 0;
    uint32_t EntityTextureId = 0;
    bool EntityTextureReady = false;

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

    /*
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    Текстурный атлас
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            Данные к атласу
    */
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


public:
    WorldPCO PCO;
    WorldId_t WI = 0;
    glm::vec3 PlayerPos = glm::vec3(0);

public:
    VulkanRenderSession(Vulkan *vkInst, IServerSession *serverSession);
    virtual ~VulkanRenderSession();

    virtual void prepareTickSync() override;
    virtual void pushStageTickSync() override;
    virtual void tickSync(TickSyncData& data) override;

    virtual void setCameraPos(WorldId_t worldId, Pos::Object pos, glm::quat quat) override;

    glm::mat4 calcViewMatrix(glm::quat quat, glm::vec3 camOffset = glm::vec3(0)) {
        return glm::translate(glm::mat4(quat), camOffset);
    }

    void beforeDraw(double timeSeconds);
    void onGpuFinished();
    void drawWorld(GlobalTime gTime, float dTime, VkCommandBuffer drawCmd);
    void pushStage(EnumRenderStage stage);

    static std::vector<VoxelVertexPoint> generateMeshForVoxelChunks(const std::vector<VoxelCube>& cubes);

private:
    void updateTestQuadTexture(uint32_t texId);
    void ensureEntityTexture();
    void ensureAtlasLayerPreview();
    void updateDescriptor_VoxelsLight();
    void updateDescriptor_ChunksLight();
};

}
