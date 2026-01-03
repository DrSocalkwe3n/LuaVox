#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Client/Vulkan/AtlasPipeline/TexturePipelineProgram.hpp"
#include "Common/Abstract.hpp"
#include "Common/Async.hpp"
#include "TOSAsync.hpp"
#include "boost/asio/executor.hpp"
#include "boost/asio/experimental/channel.hpp"
#include "boost/asio/this_coro.hpp"
#include "sha2.hpp"

/*
    Класс отвечает за отслеживание изменений и подгрузки медиаресурсов в указанных директориях.
    Медиаресурсы, собранные из папки assets или зарегистрированные модами.
    Хранит все данные в оперативной памяти.
*/

static constexpr const char* EnumAssetsToDirectory(LV::EnumAssets value) {
    switch(value) {
    case LV::EnumAssets::Nodestate: return "nodestate";
    case LV::EnumAssets::Particle:  return "particle";
    case LV::EnumAssets::Animation: return "animation";
    case LV::EnumAssets::Model:     return "model";
    case LV::EnumAssets::Texture:   return "texture";
    case LV::EnumAssets::Sound:     return "sound";
    case LV::EnumAssets::Font:      return "font";
    default:
        break;
    }

    assert(!"Неизвестный тип медиаресурса");
    return "";
}

namespace LV {

namespace fs = std::filesystem;
using AssetType = EnumAssets;

struct ResourceFile {
    using Hash_t = sha2::sha256_hash; // boost::uuids::detail::sha1::digest_type;

    Hash_t Hash;
    std::vector<uint8_t> Data;

    void calcHash() {
        Hash = sha2::sha256(Data.data(), Data.size());
    }
};

class AssetsPreloader : public TOS::IAsyncDestructible {
public:
    using Ptr = std::shared_ptr<AssetsPreloader>;
    using IdTable = std::unordered_map<
        AssetType, // Тип ресурса
        std::unordered_map<
            std::string, // Domain
            std::unordered_map<
                std::string, // Key
                uint32_t     // ResourceId
            >
        >
    >;

    // 
    /*
        Ресурс имеет бинарную часть, из который вырезаны все зависимости.
        Вторая часть это заголовок, которые всегда динамично передаётся с сервера.
        В заголовке хранятся зависимости от ресурсов.
    */
    struct MediaResource {
        std::string Domain, Key;

        fs::file_time_type Timestamp;
        // Обезличенный ресурс
        std::shared_ptr<std::vector<uint8_t>> Resource;
        // Хэш ресурса
        ResourceFile::Hash_t Hash;

        // Скомпилированный заголовок
        std::vector<uint8_t> Dependencies;
    };

    struct PendingDependency {
        std::string Domain, Key;
    };

    struct PendingResource {
        std::string Domain, Key;
        fs::file_time_type Timestamp;
        std::shared_ptr<std::vector<uint8_t>> Resource;
        ResourceFile::Hash_t Hash;
        std::vector<PendingDependency> ModelDeps;
        std::vector<PendingDependency> TextureDeps;
        std::vector<uint8_t> Extra;
    };

    struct ResourceChangeObj {
        std::unordered_map<std::string, std::vector<std::string>> Lost[(int) AssetType::MAX_ENUM];
        std::unordered_map<std::string, std::vector<PendingResource>> NewOrChange[(int) AssetType::MAX_ENUM];
    };

    struct Out_applyResourceChange {
        std::vector<uint32_t> Lost[(int) AssetType::MAX_ENUM];
        std::vector<std::pair<uint32_t, MediaResource>> NewOrChange[(int) AssetType::MAX_ENUM];
    };

    struct ReloadStatus {
        /// TODO: callback'и для обновления статусов
        /// TODO: многоуровневый статус std::vector<std::string>. Этапы/Шаги/Объекты 
    };

    struct AssetsRegister {
        /*
            Пути до активных папок assets, соответствую порядку загруженным модам.
            От последнего мода к первому.
            Тот файл, что был загружен раньше и будет использоваться
        */
        std::vector<fs::path> Assets;
        /*
            У этих ресурсов приоритет выше, если их удастся получить,
            то использоваться будут именно они
            Domain -> {key + data}
        */
        std::unordered_map<std::string, std::unordered_map<std::string, void*>> Custom[(int) AssetType::MAX_ENUM];
    };

public:
    static coro<Ptr> Create(asio::io_context& ioc);
    explicit AssetsPreloader(asio::io_context& ioc)
    : TOS::IAsyncDestructible(ioc)
    {
    }
    ~AssetsPreloader() = default;

    AssetsPreloader(const AssetsPreloader&) = delete;
    AssetsPreloader(AssetsPreloader&&) = delete;
    AssetsPreloader& operator=(const AssetsPreloader&) = delete;
    AssetsPreloader& operator=(AssetsPreloader&&) = delete;

    // Пересматривает ресурсы и выдаёт изменения.
    // Одновременно можно работать только один такой вызов.
    // instances -> пути к директории с assets или архивы с assets внутри. От низшего приоритета к высшему.
    // status -> обратный отклик о процессе обновления ресурсов.
    // ReloadStatus <- новые и потерянные ресурсы.
    coro<ResourceChangeObj> reloadResources(const std::vector<fs::path>& instances, ReloadStatus* status = nullptr) {
        bool expected = false;
        assert(Reloading_.compare_exchange_strong(expected, true) && "Двойной вызов reloadResources");
        struct ReloadGuard {
            std::atomic<bool>& Flag;
            ~ReloadGuard() { Flag.exchange(false); }
        } guard{Reloading_};

        try {
            ReloadStatus secondStatus;
            co_return co_await _reloadResources(instances, status ? *status : secondStatus);
        } catch(...) {
            assert(!"reloadResources: здесь не должно быть ошибок");
        }
    }

    /*
        Перепроверка изменений ресурсов по дате изменения, пересчёт хешей.
        Обнаруженные изменения должны быть отправлены всем клиентам.
        Ресурсы будут обработаны в подходящий формат и сохранены в кеше.
        Одновременно может выполнятся только одна такая функция
        Используется в GameServer
    */
    coro<ResourceChangeObj> recheckResources(const AssetsRegister& info) {
        return reloadResources(info.Assets);
    }

    // Синхронный вызов reloadResources
    ResourceChangeObj reloadResourcesSync(const std::vector<fs::path>& instances, ReloadStatus* status = nullptr) {
        asio::io_context ioc;
        std::optional<ResourceChangeObj> result;
        std::exception_ptr error;

        asio::co_spawn(ioc, [this, &instances, status, &result, &error]() -> coro<> {
            try {
                ReloadStatus localStatus;
                result = co_await reloadResources(instances, status ? status : &localStatus);
            } catch(...) {
                error = std::current_exception();
            }
            co_return;
        }, asio::detached);

        ioc.run();

        if (error)
            std::rethrow_exception(error);
        if (!result)
            return {};
        return std::move(*result);
    }

    // Синхронный вызов recheckResources
    ResourceChangeObj recheckResourcesSync(const AssetsRegister& info, ReloadStatus* status = nullptr) {
        return reloadResourcesSync(info.Assets, status);
    }

    /*
        Применяет расчитанные изменения.
        Раздаёт идентификаторы ресурсам и записывает их в таблицу
    */
    Out_applyResourceChange applyResourceChange(const ResourceChangeObj& orr);

    /*
        Выдаёт идентификатор ресурса, даже если он не существует или был удалён.
        resource должен содержать домен и путь
    */
    ResourceId getId(AssetType type, const std::string& domain, const std::string& key);

    // Выдаёт ресурс по идентификатору
    const MediaResource* getResource(AssetType type, uint32_t id) const;

    // Выдаёт ресурс по хешу
    std::optional<std::tuple<AssetType, uint32_t, const MediaResource*>> getResource(const ResourceFile::Hash_t& hash);

    // Выдаёт зависимости к ресурсам профиля ноды
    std::tuple<AssetsNodestate, std::vector<AssetsModel>, std::vector<AssetsTexture>>
        getNodeDependency(const std::string& domain, const std::string& key);

private:
    struct ResourceFirstStageInfo {
        // Путь к архиву (если есть), и путь до ресурса
        fs::path ArchivePath, Path;
        // Время изменения файла
        fs::file_time_type Timestamp;
    };

    struct ResourceSecondStageInfo : public ResourceFirstStageInfo {
        // Обезличенный ресурс
        std::shared_ptr<std::vector<uint8_t>> Resource;
        ResourceFile::Hash_t Hash;
        // Сырой заголовок
        std::vector<uint8_t> Dependencies;
    };

    // Текущее состояние reloadResources
    std::atomic<bool> Reloading_ = false;

    struct HeaderWriter {
        std::vector<uint8_t> Data;

        void writeU8(uint8_t value) {
            Data.push_back(value);
        }

        void writeU32(uint32_t value) {
            Data.push_back(uint8_t(value & 0xff));
            Data.push_back(uint8_t((value >> 8) & 0xff));
            Data.push_back(uint8_t((value >> 16) & 0xff));
            Data.push_back(uint8_t((value >> 24) & 0xff));
        }

        void writeBytes(const uint8_t* data, size_t size) {
            if (size == 0)
                return;
            const uint8_t* ptr = data;
            Data.insert(Data.end(), ptr, ptr + size);
        }
    };

    static ResourceFile readFileBytes(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Не удалось открыть файл: " + path.string());

        file.seekg(0, std::ios::end);
        std::streamoff size = file.tellg();
        if (size < 0)
            size = 0;
        file.seekg(0, std::ios::beg);

        ResourceFile out;
        out.Data.resize(static_cast<size_t>(size));
        if (size > 0) {
            file.read(reinterpret_cast<char*>(out.Data.data()), size);
            if (!file)
                throw std::runtime_error("Не удалось прочитать файл: " + path.string());
        }

        out.calcHash();
        return out;
    }

    static std::vector<uint8_t> toBytes(std::u8string_view data) {
        std::vector<uint8_t> out(data.size());
        if (!out.empty())
            std::memcpy(out.data(), data.data(), out.size());
        return out;
    }

    // Dependency lists are ordered by placeholder index used in the resource data.
    static std::vector<uint8_t> buildHeader(AssetType type, const std::vector<uint32_t>& modelDeps, const std::vector<uint32_t>& textureDeps, const std::vector<uint8_t>& extra) {
        HeaderWriter writer;
        writer.writeU8('a');
        writer.writeU8('h');
        writer.writeU8(1);
        writer.writeU8(static_cast<uint8_t>(type));

        writer.writeU32(static_cast<uint32_t>(modelDeps.size()));
        for (uint32_t id : modelDeps)
            writer.writeU32(id);

        writer.writeU32(static_cast<uint32_t>(textureDeps.size()));
        for (uint32_t id : textureDeps)
            writer.writeU32(id);

        writer.writeU32(static_cast<uint32_t>(extra.size()));
        writer.writeBytes(extra.data(), extra.size());

        return std::move(writer.Data);
    }

    static std::vector<uint8_t> readOptionalMeta(const fs::path& path) {
        fs::path metaPath = path;
        metaPath += ".meta";
        if (!fs::exists(metaPath) || !fs::is_regular_file(metaPath))
            return {};

        ResourceFile meta = readFileBytes(metaPath);
        return std::move(meta.Data);
    }

    static std::optional<uint32_t> findId(const IdTable& table, AssetType type, const std::string& domain, const std::string& key) {
        auto iterType = table.find(type);
        if (iterType == table.end())
            return std::nullopt;
        auto iterDomain = iterType->second.find(domain);
        if (iterDomain == iterType->second.end())
            return std::nullopt;
        auto iterKey = iterDomain->second.find(key);
        if (iterKey == iterDomain->second.end())
            return std::nullopt;
        return iterKey->second;
    }

    void ensureNextId(AssetType type) {
        size_t index = static_cast<size_t>(type);
        if (NextIdInitialized[index])
            return;

        uint32_t maxId = 0;
        auto iterType = DKToId.find(type);
        if (iterType != DKToId.end()) {
            for (const auto& [domain, keys] : iterType->second) {
                for (const auto& [key, id] : keys) {
                    maxId = std::max(maxId, id + 1);
                }
            }
        }
        NextId[index] = maxId;
        NextIdInitialized[index] = true;
    }

    struct HashHasher {
        std::size_t operator()(const ResourceFile::Hash_t& hash) const noexcept {
            std::size_t v = 14695981039346656037ULL;
            for (const auto& byte : hash) {
                v ^= static_cast<std::size_t>(byte);
                v *= 1099511628211ULL;
            }
            return v;
        }
    };

    struct ParsedHeader {
        AssetType Type = AssetType::MAX_ENUM;
        std::vector<uint32_t> ModelDeps;
        std::vector<uint32_t> TextureDeps;
        std::vector<uint8_t> Extra;
    };

    static std::optional<ParsedHeader> parseHeader(const std::vector<uint8_t>& data) {
        size_t pos = 0;
        auto readU8 = [&](uint8_t& out) -> bool {
            if (pos + 1 > data.size())
                return false;
            out = data[pos++];
            return true;
        };
        auto readU32 = [&](uint32_t& out) -> bool {
            if (pos + 4 > data.size())
                return false;
            out = uint32_t(data[pos]) |
                  (uint32_t(data[pos + 1]) << 8) |
                  (uint32_t(data[pos + 2]) << 16) |
                  (uint32_t(data[pos + 3]) << 24);
            pos += 4;
            return true;
        };

        ParsedHeader out;
        uint8_t c0, c1, version, type;
        if (!readU8(c0) || !readU8(c1) || !readU8(version) || !readU8(type))
            return std::nullopt;
        if (c0 != 'a' || c1 != 'h' || version != 1)
            return std::nullopt;
        out.Type = static_cast<AssetType>(type);

        uint32_t count = 0;
        if (!readU32(count))
            return std::nullopt;
        out.ModelDeps.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t id;
            if (!readU32(id))
                return std::nullopt;
            out.ModelDeps.push_back(id);
        }

        if (!readU32(count))
            return std::nullopt;
        out.TextureDeps.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t id;
            if (!readU32(id))
                return std::nullopt;
            out.TextureDeps.push_back(id);
        }

        uint32_t extraSize = 0;
        if (!readU32(extraSize))
            return std::nullopt;
        if (pos + extraSize > data.size())
            return std::nullopt;
        out.Extra.assign(data.begin() + pos, data.begin() + pos + extraSize);
        return out;
    }

    // Пересмотр ресурсов
    coro<ResourceChangeObj> _reloadResources(const std::vector<fs::path>& instances, ReloadStatus& status) const {
        ResourceChangeObj result;

        // 1) Поиск всех ресурсов и построение конечной карты ресурсов (timestamps, path, name, size)
        // Карта найденных ресурсов
        std::unordered_map<
            AssetType, // Тип ресурса
            std::unordered_map<
                std::string, // Domain
                std::unordered_map<
                    std::string, // Key
                    ResourceFirstStageInfo // ResourceInfo
                >
            >
        > resourcesFirstStage;

        for (const fs::path& instance : instances) {
            try {
                if (fs::is_regular_file(instance)) {
                    // Может архив
                    /// TODO: пока не поддерживается
                } else if (fs::is_directory(instance)) {
                    // Директория
                    fs::path assetsRoot = instance;
                    fs::path assetsCandidate = instance / "assets";
                    if (fs::exists(assetsCandidate) && fs::is_directory(assetsCandidate))
                        assetsRoot = assetsCandidate;

                    // Директория assets существует, перебираем домены в ней 
                    for (auto begin = fs::directory_iterator(assetsRoot), end = fs::directory_iterator(); begin != end; begin++) {
                        if (!begin->is_directory())
                            continue;

                        /// TODO: выглядит всё не очень асинхронно
                        co_await asio::post(co_await asio::this_coro::executor);

                        fs::path domainPath = begin->path();
                        std::string domain = domainPath.filename().string();
                        
                        // Перебираем по типу ресурса
                        for (int type = 0; type < static_cast<int>(AssetType::MAX_ENUM); type++) {
                            AssetType assetType = static_cast<AssetType>(type);
                            fs::path assetPath = domainPath / EnumAssetsToDirectory(assetType);
                            if (!fs::exists(assetPath) || !fs::is_directory(assetPath))
                                continue;

                            std::unordered_map<
                                std::string, // Key
                                ResourceFirstStageInfo // ResourceInfo
                            >& firstStage = resourcesFirstStage[assetType][domain];

                            // Исследуем все ресурсы одного типа
                            for (auto begin = fs::recursive_directory_iterator(assetPath), end = fs::recursive_directory_iterator(); begin != end; begin++) {
                                if (begin->is_directory())
                                    continue;

                                fs::path file = begin->path();
                                if (assetType == AssetType::Texture && file.extension() == ".meta")
                                    continue;
                                std::string key = fs::relative(file, assetPath).string();
                                if (firstStage.contains(key))
                                    continue;

                                fs::file_time_type timestamp = fs::last_write_time(file);
                                if (assetType == AssetType::Texture) {
                                    fs::path metaPath = file;
                                    metaPath += ".meta";
                                    if (fs::exists(metaPath) && fs::is_regular_file(metaPath)) {
                                        auto metaTime = fs::last_write_time(metaPath);
                                        if (metaTime > timestamp)
                                            timestamp = metaTime;
                                    }
                                }

                                // Работаем с ресурсом
                                firstStage[key] = ResourceFirstStageInfo{
                                    .Path = file,
                                    .Timestamp = timestamp
                                };
                            }
                        }
                    }
                } else {
                    throw std::runtime_error("Неизвестный тип инстанса медиаресурсов");
                }
            } catch (const std::exception& exc) {
                /// TODO: Логгировать в статусе

            }
        }

        auto buildResource = [&](AssetType type, const std::string& domain, const std::string& key, const ResourceFirstStageInfo& info) -> PendingResource {
            PendingResource out;
            out.Domain = domain;
            out.Key = key;
            out.Timestamp = info.Timestamp;

            if (type == AssetType::Nodestate) {
                ResourceFile file = readFileBytes(info.Path);
                std::string_view view(reinterpret_cast<const char*>(file.Data.data()), file.Data.size());
                js::object obj = js::parse(view).as_object();

                PreparedNodeState pns(domain, obj);
                pns.LocalToModel.reserve(pns.LocalToModelKD.size());
                uint32_t placeholder = 0;
                for (const auto& [subDomain, subKey] : pns.LocalToModelKD) {
                    pns.LocalToModel.push_back(placeholder++);
                    out.ModelDeps.push_back({subDomain, subKey});
                }

                std::vector<uint8_t> data = toBytes(pns.dump());
                out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(data));
                out.Hash = sha2::sha256(out.Resource->data(), out.Resource->size());
            } else if (type == AssetType::Model) {
                const std::string ext = info.Path.extension().string();
                if (ext == ".json") {
                    ResourceFile file = readFileBytes(info.Path);
                    std::string_view view(reinterpret_cast<const char*>(file.Data.data()), file.Data.size());
                    js::object obj = js::parse(view).as_object();

                    PreparedModel pm(domain, obj);
                    pm.CompiledTextures.clear();
                    pm.CompiledTextures.reserve(pm.Textures.size());

                    std::unordered_map<std::string, uint32_t> textureIndex;
                    auto getTexturePlaceholder = [&](const std::string& texDomain, const std::string& texKey) -> uint32_t {
                        std::string token;
                        token.reserve(texDomain.size() + texKey.size() + 1);
                        token.append(texDomain);
                        token.push_back(':');
                        token.append(texKey);

                        auto iter = textureIndex.find(token);
                        if (iter != textureIndex.end())
                            return iter->second;

                        uint32_t placeholderId = static_cast<uint32_t>(out.TextureDeps.size());
                        textureIndex.emplace(std::move(token), placeholderId);
                        out.TextureDeps.push_back({texDomain, texKey});
                        return placeholderId;
                    };

                    for (const auto& [tkey, pipeline] : pm.Textures) {
                        TexturePipeline compiled;

                        if (pipeline.IsSource) {
                            TexturePipelineProgram program;
                            std::string source(reinterpret_cast<const char*>(pipeline.Pipeline.data()), pipeline.Pipeline.size());
                            std::string err;
                            if (!program.compile(source, &err)) {
                                throw std::runtime_error("Ошибка компиляции pipeline: " + err);
                            }

                            auto resolver = [&](std::string_view name) -> std::optional<uint32_t> {
                                auto [texDomain, texKey] = parseDomainKey(std::string(name), domain);
                                return getTexturePlaceholder(texDomain, texKey);
                            };

                            if (!program.link(resolver, &err)) {
                                throw std::runtime_error("Ошибка линковки pipeline: " + err);
                            }

                            const std::vector<uint8_t> bytes = program.toBytes();
                            compiled.Pipeline.resize(bytes.size());
                            if (!bytes.empty()) {
                                std::memcpy(compiled.Pipeline.data(), bytes.data(), bytes.size());
                            }
                        } else {
                            compiled.Pipeline = pipeline.Pipeline;
                        }

                        for (const auto& [texDomain, texKey] : pipeline.Assets) {
                            uint32_t placeholderId = getTexturePlaceholder(texDomain, texKey);
                            compiled.BinTextures.push_back(placeholderId);
                        }

                        pm.CompiledTextures[tkey] = std::move(compiled);
                    }

                    for (const auto& sub : pm.SubModels) {
                        out.ModelDeps.push_back({sub.Domain, sub.Key});
                    }

                    std::vector<uint8_t> data = toBytes(pm.dump());
                    out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(data));
                    out.Hash = sha2::sha256(out.Resource->data(), out.Resource->size());
                } else if (ext == ".gltf" || ext == ".glb") {
                    ResourceFile file = readFileBytes(info.Path);
                    out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(file.Data));
                    out.Hash = file.Hash;
                } else {
                    throw std::runtime_error("Не поддерживаемый формат модели: " + info.Path.string());
                }
            } else if (type == AssetType::Texture) {
                ResourceFile file = readFileBytes(info.Path);
                out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(file.Data));
                out.Hash = file.Hash;
                out.Extra = readOptionalMeta(info.Path);
            } else {
                ResourceFile file = readFileBytes(info.Path);
                out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(file.Data));
                out.Hash = file.Hash;
            }
            return out;
        };

        // 2) Обрабатываться будут только изменённые (новый timestamp) или новые ресурсы

        // Текстуры, шрифты, звуки хранить как есть
        // У моделей, состояний нод, анимации, частиц обналичить зависимости

        for (const auto& [type, resources] : MediaResources) {
            auto iterType = resourcesFirstStage.find(type);
            for (const auto& [id, resource] : resources) {
                if (iterType == resourcesFirstStage.end()) {
                    result.Lost[(int) type][resource.Domain].push_back(resource.Key);
                    continue;
                }

                auto iterDomain = iterType->second.find(resource.Domain);
                if (iterDomain == iterType->second.end()) {
                    result.Lost[(int) type][resource.Domain].push_back(resource.Key);
                    continue;
                }

                if (!iterDomain->second.contains(resource.Key)) {
                    result.Lost[(int) type][resource.Domain].push_back(resource.Key);
                }
            }
        }

        for (const auto& [type, domains] : resourcesFirstStage) {
            for (const auto& [domain, table] : domains) {
                for (const auto& [key, info] : table) {
                    bool needsUpdate = true;
                    if (auto existingId = findId(DKToId, type, domain, key)) {
                        auto iterType = MediaResources.find(type);
                        if (iterType != MediaResources.end()) {
                            auto iterRes = iterType->second.find(*existingId);
                            if (iterRes != iterType->second.end() && iterRes->second.Timestamp == info.Timestamp) {
                                needsUpdate = false;
                            }
                        }
                    }

                    if (!needsUpdate)
                        continue;

                    co_await asio::post(co_await asio::this_coro::executor);
                    PendingResource resource = buildResource(type, domain, key, info);
                    result.NewOrChange[(int) type][domain].push_back(std::move(resource));
                }
            }
        }

        co_return result;
    }

    IdTable DKToId;
    std::unordered_map<AssetType, std::unordered_map<uint32_t, MediaResource>> MediaResources;
    std::unordered_map<ResourceFile::Hash_t, std::pair<AssetType, uint32_t>, HashHasher> HashToId;
    std::array<uint32_t, static_cast<size_t>(AssetType::MAX_ENUM)> NextId = {};
    std::array<bool, static_cast<size_t>(AssetType::MAX_ENUM)> NextIdInitialized = {};
};

inline AssetsPreloader::Out_applyResourceChange AssetsPreloader::applyResourceChange(const ResourceChangeObj& orr) {
    Out_applyResourceChange result;

    // Удаляем ресурсы
    /*
        Удаляются только ресурсы, при этом за ними остаётся бронь на идентификатор
        Уже скомпилированные зависимости к ресурсам не будут
        перекомпилироваться для смены идентификатора. Если нужный ресурс
        появится, то привязка останется. Новые клиенты не получат ресурс
        которого нет, но он может использоваться
    */
    for (int type = 0; type < (int) AssetType::MAX_ENUM; type++) {
        for (const auto& [domain, keys] : orr.Lost[type]) {
            auto iterType = DKToId.find(static_cast<AssetType>(type));
            if (iterType == DKToId.end())
                continue;
            auto iterDomain = iterType->second.find(domain);
            if (iterDomain == iterType->second.end())
                continue;

            for (const auto& key : keys) {
                auto iterKey = iterDomain->second.find(key);
                if (iterKey == iterDomain->second.end())
                    continue;
                uint32_t id = iterKey->second;
                result.Lost[type].push_back(id);

                auto iterResType = MediaResources.find(static_cast<AssetType>(type));
                if (iterResType == MediaResources.end())
                    continue;
                auto iterRes = iterResType->second.find(id);
                if (iterRes == iterResType->second.end())
                    continue;

                HashToId.erase(iterRes->second.Hash);
                iterResType->second.erase(iterRes);
            }
        }
    }

    // Добавляем
    for (int type = 0; type < (int) AssetType::MAX_ENUM; type++) {
        for (const auto& [domain, resources] : orr.NewOrChange[type]) {
            for (const PendingResource& pending : resources) {
                uint32_t id = getId(static_cast<AssetType>(type), pending.Domain, pending.Key);

                std::vector<uint32_t> modelIds;
                modelIds.reserve(pending.ModelDeps.size());
                for (const auto& dep : pending.ModelDeps)
                    modelIds.push_back(getId(AssetType::Model, dep.Domain, dep.Key));

                std::vector<uint32_t> textureIds;
                textureIds.reserve(pending.TextureDeps.size());
                for (const auto& dep : pending.TextureDeps)
                    textureIds.push_back(getId(AssetType::Texture, dep.Domain, dep.Key));

                MediaResource resource;
                resource.Domain = pending.Domain;
                resource.Key = pending.Key;
                resource.Timestamp = pending.Timestamp;
                resource.Resource = pending.Resource;
                resource.Hash = pending.Hash;
                resource.Dependencies = buildHeader(static_cast<AssetType>(type), modelIds, textureIds, pending.Extra);

                auto& table = MediaResources[static_cast<AssetType>(type)];
                if (auto iter = table.find(id); iter != table.end())
                    HashToId.erase(iter->second.Hash);

                table[id] = resource;
                HashToId[resource.Hash] = {static_cast<AssetType>(type), id};

                result.NewOrChange[type].push_back({id, std::move(resource)});
            }
        }

        std::unordered_set<uint32_t> changed;
        for (const auto& [id, _] : result.NewOrChange[type])
            changed.insert(id);

        auto& lost = result.Lost[type];
        lost.erase(std::remove_if(lost.begin(), lost.end(),
            [&](uint32_t id) { return changed.contains(id); }), lost.end());
    }

    return result;
}

inline ResourceId AssetsPreloader::getId(AssetType type, const std::string& domain, const std::string& key) {
    auto& typeTable = DKToId[type];
    auto& domainTable = typeTable[domain];
    if (auto iter = domainTable.find(key); iter != domainTable.end())
        return iter->second;

    ensureNextId(type);
    uint32_t id = NextId[static_cast<size_t>(type)]++;
    domainTable[key] = id;
    return id;
}

inline const AssetsPreloader::MediaResource* AssetsPreloader::getResource(AssetType type, uint32_t id) const {
    auto iterType = MediaResources.find(type);
    if (iterType == MediaResources.end())
        return nullptr;
    auto iterRes = iterType->second.find(id);
    if (iterRes == iterType->second.end())
        return nullptr;
    return &iterRes->second;
}

inline std::optional<std::tuple<AssetType, uint32_t, const AssetsPreloader::MediaResource*>> AssetsPreloader::getResource(const ResourceFile::Hash_t& hash) {
    auto iter = HashToId.find(hash);
    if (iter == HashToId.end())
        return std::nullopt;

    auto [type, id] = iter->second;
    const MediaResource* res = getResource(type, id);
    if (!res) {
        HashToId.erase(iter);
        return std::nullopt;
    }
    if (res->Hash != hash) {
        HashToId.erase(iter);
        return std::nullopt;
    }

    return std::tuple<AssetType, uint32_t, const MediaResource*>{type, id, res};
}

inline std::tuple<AssetsNodestate, std::vector<AssetsModel>, std::vector<AssetsTexture>>
AssetsPreloader::getNodeDependency(const std::string& domain, const std::string& key) {
    if (domain == "core" && key == "none") {
        return {0, {}, {}};
    }

    AssetsNodestate nodestateId = getId(AssetType::Nodestate, domain, key + ".json");
    const MediaResource* nodestate = getResource(AssetType::Nodestate, nodestateId);
    if (!nodestate)
        return {nodestateId, {}, {}};

    auto parsed = parseHeader(nodestate->Dependencies);
    if (!parsed)
        return {nodestateId, {}, {}};

    std::vector<AssetsModel> models;
    std::vector<AssetsTexture> textures;
    std::unordered_set<AssetsModel> visited;
    std::unordered_set<AssetsTexture> seenTextures;

    std::function<void(AssetsModel)> visitModel = [&](AssetsModel modelId) {
        if (!visited.insert(modelId).second)
            return;

        models.push_back(modelId);

        const MediaResource* modelRes = getResource(AssetType::Model, modelId);
        if (!modelRes)
            return;
        auto header = parseHeader(modelRes->Dependencies);
        if (!header)
            return;

        for (uint32_t texId : header->TextureDeps) {
            if (seenTextures.insert(texId).second)
                textures.push_back(texId);
        }

        for (uint32_t subId : header->ModelDeps)
            visitModel(subId);
    };

    for (uint32_t modelId : parsed->ModelDeps)
        visitModel(modelId);

    return {nodestateId, std::move(models), std::move(textures)};
}

}
