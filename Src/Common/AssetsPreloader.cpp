#include "AssetsPreloader.hpp"
#include "Common/Abstract.hpp"
#include "Common/TexturePipelineProgram.hpp"
#include "sha2.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace LV {

static TOS::Logger LOG = "AssetsPreloader";

static ResourceFile readFileBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file)
        throw std::runtime_error("Не удалось открыть файл: " + path.string());

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if(size < 0)
        size = 0;
    file.seekg(0, std::ios::beg);

    ResourceFile out;
    out.Data.resize(static_cast<size_t>(size));
    if(size > 0) {
        file.read(reinterpret_cast<char*>(out.Data.data()), size);
        if (!file)
            throw std::runtime_error("Не удалось прочитать файл: " + path.string());
    }

    out.calcHash();
    return out;
}

static std::u8string readOptionalMeta(const fs::path& path) {
    fs::path metaPath = path;
    metaPath += ".meta";
    if(!fs::exists(metaPath) || !fs::is_regular_file(metaPath))
        return {};

    ResourceFile meta = readFileBytes(metaPath);
    return std::move(meta.Data);
}

AssetsPreloader::AssetsPreloader() {
    for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); type++) {
        ResourceLinks[type].emplace_back(
            ResourceFile::Hash_t{0},
            ResourceHeader(),
            fs::file_time_type(),
            fs::path{""},
            false
        );
    }
}

AssetsPreloader::Out_checkAndPrepareResourcesUpdate AssetsPreloader::checkAndPrepareResourcesUpdate(
    const AssetsRegister& instances,
    const std::function<ResourceId(EnumAssets type, std::string_view domain, std::string_view key)>& idResolver,
    const std::function<void(std::u8string&& resource, ResourceFile::Hash_t hash, fs::path resPath)>& onNewResourceParsed,
    ReloadStatus* status
) {
    assert(idResolver);

    bool expected = false;
    assert(_Reloading.compare_exchange_strong(expected, true) && "Двойной вызов reloadResources");
    struct ReloadGuard {
        std::atomic<bool>& Flag;
        ~ReloadGuard() { Flag.exchange(false); }
    } guard{_Reloading};

    try {
        ReloadStatus secondStatus;
        return _checkAndPrepareResourcesUpdate(instances, idResolver, onNewResourceParsed, status ? *status : secondStatus);
    } catch(const std::exception& exc) {
        LOG.error() << exc.what();
        assert(!"reloadResources: здесь не должно быть ошибок");
        std::unreachable();
    } catch(...) {
        assert(!"reloadResources: здесь не должно быть ошибок");
        std::unreachable();
    }
}

AssetsPreloader::Out_checkAndPrepareResourcesUpdate AssetsPreloader::_checkAndPrepareResourcesUpdate(
    const AssetsRegister& instances,
    const std::function<ResourceId(EnumAssets type, std::string_view domain, std::string_view key)>& idResolver,
    const std::function<void(std::u8string&& resource, ResourceFile::Hash_t hash, fs::path resPath)>& onNewResourceParsed,
    ReloadStatus& status
) {
    // 1) Поиск всех ресурсов и построение конечной карты ресурсов (timestamps, path, name, size)
    // Карта найденных ресурсов
    std::array<
        std::unordered_map<
            std::string,    // Domain
            std::unordered_map<
                std::string,
                ResourceFindInfo,
                detail::TSVHash,
                detail::TSVEq
            >,
            detail::TSVHash,
            detail::TSVEq
        >,
        static_cast<size_t>(AssetType::MAX_ENUM)
    > resourcesFirstStage;

    for(const fs::path& instance : instances.Assets) {
        try {
            if(fs::is_regular_file(instance)) {
                // Может архив
                /// TODO: пока не поддерживается
            } else if(fs::is_directory(instance)) {
                // Директория
                fs::path assetsRoot = instance;
                fs::path assetsCandidate = instance / "assets";
                if (fs::exists(assetsCandidate) && fs::is_directory(assetsCandidate))
                    assetsRoot = assetsCandidate;

                // Директория assets существует, перебираем домены в ней 
                for(auto begin = fs::directory_iterator(assetsRoot), end = fs::directory_iterator(); begin != end; begin++) {
                    if(!begin->is_directory())
                        continue;

                    fs::path domainPath = begin->path();
                    std::string domain = domainPath.filename().string();
                    
                    // Перебираем по типу ресурса
                    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
                        AssetType assetType = static_cast<AssetType>(type);
                        fs::path assetPath = domainPath / EnumAssetsToDirectory(assetType);
                        if (!fs::exists(assetPath) || !fs::is_directory(assetPath))
                            continue;

                        std::unordered_map<
                            std::string, // Key
                            ResourceFindInfo, // ResourceInfo,
                            detail::TSVHash,
                            detail::TSVEq
                        >& firstStage = resourcesFirstStage[static_cast<size_t>(assetType)][domain];

                        // Исследуем все ресурсы одного типа
                        for(auto begin = fs::recursive_directory_iterator(assetPath), end = fs::recursive_directory_iterator(); begin != end; begin++) {
                            if(begin->is_directory())
                                continue;

                            fs::path file = begin->path();
                            if(assetType == AssetType::Texture && file.extension() == ".meta")
                                continue;

                            std::string key = fs::relative(file, assetPath).generic_string();
                            if(firstStage.contains(key))
                                continue;

                            fs::file_time_type timestamp = fs::last_write_time(file);
                            if(assetType == AssetType::Texture) {
                                fs::path metaPath = file;
                                metaPath += ".meta";
                                if (fs::exists(metaPath) && fs::is_regular_file(metaPath)) {
                                    auto metaTime = fs::last_write_time(metaPath);
                                    if (metaTime > timestamp)
                                        timestamp = metaTime;
                                }
                            }

                            // Работаем с ресурсом
                            firstStage[key] = ResourceFindInfo{
                                .Path = file,
                                .Timestamp = timestamp,
                                .Id = idResolver(assetType, domain, key)
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

    // Функция парсинга ресурсов
    auto buildResource = [&](AssetType type, std::string_view domain, std::string_view key, const ResourceFindInfo& info) -> PendingResource {
        PendingResource out;
        out.Key = key;
        out.Timestamp = info.Timestamp;

        std::function<uint32_t(const std::string_view)> modelResolver
            = [&](const std::string_view model) -> uint32_t
        {
            auto [mDomain, mKey] = parseDomainKey(model, domain);
            return idResolver(AssetType::Model, mDomain, mKey);
        };

        std::function<std::optional<uint32_t>(std::string_view)> textureIdResolver
            = [&](std::string_view texture) -> std::optional<uint32_t>
        {
            auto [mDomain, mKey] = parseDomainKey(texture, domain);
            return idResolver(AssetType::Texture, mDomain, mKey);
        };

        std::function<std::vector<uint8_t>(const std::string_view)> textureResolver
            = [&](const std::string_view texturePipelineSrc) -> std::vector<uint8_t>
        {
            TexturePipelineProgram tpp;
            bool flag = tpp.compile(texturePipelineSrc);
            if(!flag)
                return {};

            tpp.link(textureIdResolver);

            return tpp.toBytes();
        };

        if (type == AssetType::Nodestate) {
            ResourceFile file = readFileBytes(info.Path);
            std::string_view view(reinterpret_cast<const char*>(file.Data.data()), file.Data.size());
            js::object obj = js::parse(view).as_object();

            HeadlessNodeState hns;
            out.Header = hns.parse(obj, modelResolver);
            out.Resource = hns.dump();
            out.Hash = sha2::sha256((const uint8_t*) out.Resource.data(), out.Resource.size());
        } else if (type == AssetType::Model) {
            const std::string ext = info.Path.extension().string();
            if (ext == ".json") {
                ResourceFile file = readFileBytes(info.Path);
                std::string_view view(reinterpret_cast<const char*>(file.Data.data()), file.Data.size());
                js::object obj = js::parse(view).as_object();

                HeadlessModel hm;
                out.Header = hm.parse(obj, modelResolver, textureResolver);
                out.Resource = hm.dump();
                out.Hash = sha2::sha256((const uint8_t*) out.Resource.data(), out.Resource.size());
            // } else if (ext == ".gltf" || ext == ".glb") {
            //     /// TODO: добавить поддержку gltf
            //     ResourceFile file = readFileBytes(info.Path);
            //     out.Resource = std::make_shared<std::vector<uint8_t>>(std::move(file.Data));
            //     out.Hash = file.Hash;
            } else {
                throw std::runtime_error("Не поддерживаемый формат модели: " + info.Path.string());
            }
        } else if (type == AssetType::Texture) {
            ResourceFile file = readFileBytes(info.Path);
            out.Resource = std::move(file.Data);
            out.Hash = file.Hash;
            out.Header = readOptionalMeta(info.Path);
        } else {
            ResourceFile file = readFileBytes(info.Path);
            out.Resource = std::move(file.Data);
            out.Hash = file.Hash;
        }

        out.Id = idResolver(type, domain, key);

        return out;
    }; 

    // 2) Определяем какие ресурсы изменились (новый timestamp) или новые ресурсы
    Out_checkAndPrepareResourcesUpdate result;

    // Собираем идентификаторы, чтобы потом определить какие ресурсы пропали
    std::array<
        std::unordered_set<ResourceId>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > uniqueExists;

    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        auto& uniqueExistsTypes = uniqueExists[type];
        const auto& resourceLinksTyped = ResourceLinks[type];
        result.MaxNewSize[type] = resourceLinksTyped.size();

        {
            size_t allIds = 0;
            for(const auto& [domain, keys] : resourcesFirstStage[type])
                allIds += keys.size();

            uniqueExistsTypes.reserve(allIds);
        }

        for(const auto& [domain, keys] : resourcesFirstStage[type]) {
            for(const auto& [key, res] : keys) {
                uniqueExistsTypes.insert(res.Id);

                if(res.Id >= resourceLinksTyped.size() || !resourceLinksTyped[res.Id].IsExist)
                {   // Если идентификатора нет в таблице или ресурс не привязан
                    PendingResource resource = buildResource(static_cast<AssetType>(type), domain, key, res);
                    if(onNewResourceParsed)
                        onNewResourceParsed(std::move(resource.Resource), resource.Hash, res.Path);
                    result.HashToPathNew[resource.Hash].push_back(res.Path);

                    if(res.Id >= result.MaxNewSize[type])
                        result.MaxNewSize[type] = res.Id+1;

                    result.ResourceUpdates[type].emplace_back(res.Id, resource.Hash, std::move(resource.Header), resource.Timestamp, res.Path);
                } else if(resourceLinksTyped[res.Id].Path != res.Path
                    || resourceLinksTyped[res.Id].LastWrite != res.Timestamp
                ) { // Если ресурс теперь берётся с другого места или изменилось время изменения файла
                    const auto& lastResource = resourceLinksTyped[res.Id];
                    PendingResource resource = buildResource(static_cast<AssetType>(type), domain, key, res);
                    
                    if(lastResource.Hash != resource.Hash) {
                        // Хэш изменился
                        // Сообщаем о новом ресурсе
                        if(onNewResourceParsed)
                            onNewResourceParsed(std::move(resource.Resource), resource.Hash, res.Path);
                        // Старый хэш более не доступен по этому расположению.
                        result.HashToPathLost[lastResource.Hash].push_back(resourceLinksTyped[res.Id].Path);
                        // Новый хеш стал доступен по этому расположению.
                        result.HashToPathNew[resource.Hash].push_back(res.Path);
                    } else if(resourceLinksTyped[res.Id].Path != res.Path) {
                        // Изменился конечный путь.
                        // Хэш более не доступен по этому расположению.
                        result.HashToPathLost[resource.Hash].push_back(resourceLinksTyped[res.Id].Path);
                        // Хеш теперь доступен по этому расположению.
                        result.HashToPathNew[resource.Hash].push_back(res.Path);
                    } else {
                        // Ресурс без заголовка никак не изменился.
                    }

                    // Чтобы там не поменялось, мог поменятся заголовок. Уведомляем о новой привязке.
                    result.ResourceUpdates[type].emplace_back(res.Id, resource.Hash, std::move(resource.Header), resource.Timestamp, res.Path);
                } else {
                    // Ресурс не изменился
                }
            }
        }
    }

    // 3) Определяем какие ресурсы пропали
    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        const auto& resourceLinksTyped = ResourceLinks[type];

        size_t counter = 0;
        for(const auto& [hash, header, timestamp, path, isExist] : resourceLinksTyped) {
            size_t id = counter++;
            if(!isExist)
                continue;

            if(uniqueExists[type].contains(id))
                continue;

            // Ресурс потерян
            // Хэш более не доступен по этому расположению.
            result.HashToPathLost[hash].push_back(path);
            result.LostLinks[type].push_back(id);
        }
    }

    return result;
}

AssetsPreloader::Out_applyResourcesUpdate AssetsPreloader::applyResourcesUpdate(const Out_checkAndPrepareResourcesUpdate& orr) {
    Out_applyResourcesUpdate result;

    for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); ++type) {
        // Затираем потерянные
        for(ResourceId id : orr.LostLinks[type]) {
            assert(id < ResourceLinks[type].size());
            auto& [hash, header, timestamp, path, isExist] = ResourceLinks[type][id];
            hash = {0};
            header = {};
            timestamp = fs::file_time_type();
            path.clear();
            isExist = false;

            result.NewOrUpdates[type].emplace_back(id, hash, header);
        }

        // Увеличиваем размер, если необходимо
        if(orr.MaxNewSize[type] > ResourceLinks[type].size()) {
            ResourceLink def{
                ResourceFile::Hash_t{0},
                ResourceHeader(),
                fs::file_time_type(),
                fs::path{""},
                false
            };

            ResourceLinks[type].resize(orr.MaxNewSize[type], def);
        }

        // Обновляем / добавляем
        for(auto& [id, hash, header, timestamp, path] : orr.ResourceUpdates[type]) {
            ResourceLinks[type][id] = {hash, std::move(header), timestamp, std::move(path), true};
            result.NewOrUpdates[type].emplace_back(id, hash, header);
        }
    }

    return result;
}

}
