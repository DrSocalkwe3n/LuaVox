#include "AssetsPreloader.hpp"
#include <atomic>
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
    std::fill(NextId.begin(), NextId.end(), 1);
    std::fill(LastSendId.begin(), LastSendId.end(), 1);
}

AssetsPreloader::Out_reloadResources AssetsPreloader::reloadResources(const AssetsRegister& instances, ReloadStatus* status) {
    bool expected = false;
    assert(_Reloading.compare_exchange_strong(expected, true) && "Двойной вызов reloadResources");
    struct ReloadGuard {
        std::atomic<bool>& Flag;
        ~ReloadGuard() { Flag.exchange(false); }
    } guard{_Reloading};

    try {
        ReloadStatus secondStatus;
        return _reloadResources(instances, status ? *status : secondStatus);
    } catch(const std::exception& exc) {
        LOG.error() << exc.what();
        assert(!"reloadResources: здесь не должно быть ошибок");
        std::unreachable();
    } catch(...) {
        assert(!"reloadResources: здесь не должно быть ошибок");
        std::unreachable();
    }
}

AssetsPreloader::Out_reloadResources AssetsPreloader::_reloadResources(const AssetsRegister& instances, ReloadStatus& status) {
    Out_reloadResources result;

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

    for (const fs::path& instance : instances.Assets) {
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
                        for (auto begin = fs::recursive_directory_iterator(assetPath), end = fs::recursive_directory_iterator(); begin != end; begin++) {
                            if (begin->is_directory())
                                continue;

                            fs::path file = begin->path();
                            if (assetType == AssetType::Texture && file.extension() == ".meta")
                                continue;

                            std::string key = fs::relative(file, assetPath).generic_string();
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
                            firstStage[key] = ResourceFindInfo{
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

    auto resolveModelInfo = [&](std::string_view domain, std::string_view key) -> const ResourceFindInfo* {
        auto& table = resourcesFirstStage[static_cast<size_t>(AssetType::Model)];
        auto iterDomain = table.find(std::string(domain));
        if(iterDomain == table.end())
            return nullptr;
        auto iterKey = iterDomain->second.find(std::string(key));
        if(iterKey == iterDomain->second.end())
            return nullptr;
        return &iterKey->second;
    };

    std::function<std::optional<js::object>(std::string_view, std::string_view, std::unordered_set<std::string>&)> loadModelProfile;
    loadModelProfile = [&](std::string_view domain, std::string_view key, std::unordered_set<std::string>& visiting)
        -> std::optional<js::object>
    {
        std::string fullKey = std::string(domain) + ':' + std::string(key);
        if(!visiting.insert(fullKey).second) {
            LOG.warn() << "Model parent cycle: " << fullKey;
            return std::nullopt;
        }

        const ResourceFindInfo* info = resolveModelInfo(domain, key);
        if(!info) {
            LOG.warn() << "Model file not found for parent: " << fullKey;
            visiting.erase(fullKey);
            return std::nullopt;
        }

        ResourceFile file = readFileBytes(info->Path);
        std::string_view view(reinterpret_cast<const char*>(file.Data.data()), file.Data.size());
        js::object obj = js::parse(view).as_object();

        if(auto parentVal = obj.if_contains("parent")) {
            if(parentVal->is_string()) {
                std::string parentStr = std::string(parentVal->as_string());
                auto [pDomain, pKeyRaw] = parseDomainKey(parentStr, domain);
                fs::path pKeyPath = fs::path(pKeyRaw);
                if(pKeyPath.extension().empty())
                    pKeyPath += ".json";
                std::string pKey = pKeyPath.generic_string();

                std::optional<js::object> parent = loadModelProfile(pDomain, pKey, visiting);
                if(parent) {
                    auto mergeFieldIfMissing = [&](const char* field) {
                        if(!obj.contains(field) && parent->contains(field))
                            obj[field] = parent->at(field);
                    };

                    mergeFieldIfMissing("cuboids");
                    mergeFieldIfMissing("sub_models");
                    mergeFieldIfMissing("gui_light");
                    mergeFieldIfMissing("ambient_occlusion");

                    if(auto parentTextures = parent->if_contains("textures"); parentTextures && parentTextures->is_object()) {
                        if(auto childTextures = obj.if_contains("textures"); childTextures && childTextures->is_object()) {
                            auto& childObj = childTextures->as_object();
                            const auto& parentObj = parentTextures->as_object();
                            for(const auto& [tkey, tval] : parentObj) {
                                if(!childObj.contains(tkey))
                                    childObj.emplace(tkey, tval);
                            }
                        } else if(!obj.contains("textures")) {
                            obj["textures"] = parentTextures->as_object();
                        }
                    }

                    if(auto parentDisplay = parent->if_contains("display"); parentDisplay && parentDisplay->is_object()) {
                        if(auto childDisplay = obj.if_contains("display"); childDisplay && childDisplay->is_object()) {
                            auto& childObj = childDisplay->as_object();
                            const auto& parentObj = parentDisplay->as_object();
                            for(const auto& [dkey, dval] : parentObj) {
                                if(!childObj.contains(dkey))
                                    childObj.emplace(dkey, dval);
                            }
                        } else if(!obj.contains("display")) {
                            obj["display"] = parentDisplay->as_object();
                        }
                    }
                }
            }
        }

        visiting.erase(fullKey);
        return obj;
    };

    // Функция парсинга ресурсов
    auto buildResource = [&](AssetType type, std::string_view domain, std::string_view key, const ResourceFindInfo& info) -> PendingResource {
        PendingResource out;
        out.Key = key;
        out.Timestamp = info.Timestamp;

        std::function<uint32_t(const std::string_view)> modelResolver
            = [&](const std::string_view model) -> uint32_t
        {
            auto [mDomain, mKey] = parseDomainKey(model, domain);
            return getId(AssetType::Model, mDomain, mKey);
        };

        std::function<std::optional<uint32_t>(std::string_view)> textureIdResolver
            = [&](std::string_view texture) -> std::optional<uint32_t>
        {
            auto [mDomain, mKey] = parseDomainKey(texture, domain);
            return getId(AssetType::Texture, mDomain, mKey);
        };

        auto normalizeTexturePipelineSrc = [](std::string_view src) -> std::string {
            std::string out(src);
            auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
            size_t start = 0;
            while(start < out.size() && isSpace(static_cast<unsigned char>(out[start])))
                ++start;
            if(out.compare(start, 3, "tex") != 0) {
                std::string pref = "tex ";
                pref += out.substr(start);
                return pref;
            }
            return out;
        };

        std::function<std::vector<uint8_t>(const std::string_view)> textureResolver
            = [&](const std::string_view texturePipelineSrc) -> std::vector<uint8_t>
        {
            TexturePipelineProgram tpp;
            bool flag = tpp.compile(normalizeTexturePipelineSrc(texturePipelineSrc));
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
            out.Resource = std::make_shared<std::u8string>(hns.dump());
            out.Hash = sha2::sha256((const uint8_t*) out.Resource->data(), out.Resource->size());
        } else if (type == AssetType::Model) {
            const std::string ext = info.Path.extension().string();
            if (ext == ".json") {
                std::unordered_set<std::string> visiting;
                std::optional<js::object> objOpt = loadModelProfile(domain, key, visiting);
                if(!objOpt) {
                    LOG.warn() << "Не удалось загрузить модель: " << info.Path.string();
                    throw std::runtime_error("Model profile load failed");
                }
                js::object obj = std::move(*objOpt);

                HeadlessModel hm;
                out.Header = hm.parse(obj, modelResolver, textureResolver);
                std::u8string compiled = hm.dump();
                if(hm.Cuboids.empty()) {
                    static std::atomic<uint32_t> debugEmptyModelLogCount = 0;
                    uint32_t idx = debugEmptyModelLogCount.fetch_add(1);
                    if(idx < 128) {
                        LOG.warn() << "Model compiled with empty cuboids: "
                            << domain << ':' << key
                            << " file=" << info.Path.string()
                            << " size=" << compiled.size();
                    }
                }
                out.Resource = std::make_shared<std::u8string>(std::move(compiled));
                out.Hash = sha2::sha256((const uint8_t*) out.Resource->data(), out.Resource->size());
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
            out.Resource = std::make_shared<std::u8string>(std::move(file.Data));
            out.Hash = file.Hash;
            out.Header = readOptionalMeta(info.Path);
        } else {
            ResourceFile file = readFileBytes(info.Path);
            out.Resource = std::make_shared<std::u8string>(std::move(file.Data));
            out.Hash = file.Hash;
        }

        out.Id = getId(type, domain, key);

        return out;
    };

    // 2) Обрабатываться будут только изменённые (новый timestamp) или новые ресурсы
    // Определяем каких ресурсов не стало
    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        auto& tableResourcesFirstStage = resourcesFirstStage[type];
        for(const auto& [id, resource] : MediaResources[type]) {
            if(tableResourcesFirstStage.empty()) {
                result.Lost[type][resource.Domain].push_back(resource.Key);
                continue;
            }

            auto iterDomain = tableResourcesFirstStage.find(resource.Domain);
            if(iterDomain == tableResourcesFirstStage.end()) {
                result.Lost[type][resource.Domain].push_back(resource.Key);
                continue;
            }

            if(!iterDomain->second.contains(resource.Key)) {
                result.Lost[type][resource.Domain].push_back(resource.Key);
            }
        }
    }

    // Определение новых или изменённых ресурсов
    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        for(const auto& [domain, table] : resourcesFirstStage[type]) {
            auto iterTableDomain = DKToId[type].find(domain);
            if(iterTableDomain == DKToId[type].end()) {
                // Домен неизвестен движку, все ресурсы в нём новые
                for(const auto& [key, info] : table) {
                    PendingResource resource = buildResource(static_cast<AssetType>(type), domain, key, info);
                    result.NewOrChange[type][domain].push_back(std::move(resource));
                }
            } else {
                for(const auto& [key, info] : table) {
                    bool needsUpdate = true;
                    if(auto iterKey = iterTableDomain->second.find(key); iterKey != iterTableDomain->second.end()) {
                        // Идентификатор найден
                        auto iterRes = MediaResources[type].find(iterKey->second);
                        // Если нашли ресурс по идентификатору и время изменения не поменялось, то он не новый и не изменился
                        if(iterRes != MediaResources[type].end() && iterRes->second.Timestamp == info.Timestamp)
                            needsUpdate = false;
                    }

                    if(!needsUpdate)
                        continue;

                    PendingResource resource = buildResource(static_cast<AssetType>(type), domain, key, info);
                    result.NewOrChange[(int) type][domain].push_back(std::move(resource));
                }
            }
        }
    }

    return result;
}

AssetsPreloader::Out_applyResourceChange AssetsPreloader::applyResourceChange(const Out_reloadResources& orr) {
    Out_applyResourceChange result;

    // Удаляем ресурсы
    /*
        Удаляются только ресурсы, при этом за ними остаётся бронь на идентификатор
        Уже скомпилированные зависимости к ресурсам не будут
        перекомпилироваться для смены идентификатора.
        Если нужный ресурс появится, то привязка останется.
        Новые клиенты не получат ресурс которого нет,
        но он может использоваться
    */
    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); type++) {
        for(const auto& [domain, keys] : orr.Lost[type]) {
            auto iterDomain = DKToId[type].find(domain);

            // Если уже было решено, что ресурсы были, и стали потерянными, то так и должно быть
            assert(iterDomain != DKToId[type].end());

            for(const auto& key : keys) {
                auto iterKey = iterDomain->second.find(key);

                // Ресурс был и должен быть
                assert(iterKey != iterDomain->second.end());

                uint32_t id = iterKey->second;
                auto& resType = MediaResources[type];
                auto iterRes = resType.find(id);
                if(iterRes == resType.end())
                    continue;

                // Ресурс был потерян
                result.Lost[type].push_back(id);
                // Hash более нам неизвестен
                HashToId.erase(iterRes->second.Hash);
                // Затираем ресурс
                resType.erase(iterRes);
            }
        }
    }

    // Добавляем
    for(int type = 0; type < (int) AssetType::MAX_ENUM; type++) {
        auto& typeTable = DKToId[type];
        for(const auto& [domain, resources] : orr.NewOrChange[type]) {
            auto& domainTable = typeTable[domain];
            for(const PendingResource& pending : resources) {
                MediaResource resource {
                    .Domain = domain,
                    .Key = std::move(pending.Key),
                    .Timestamp = pending.Timestamp,
                    .Resource = std::move(pending.Resource),
                    .Hash = pending.Hash,
                    .Header = std::move(pending.Header)
                };

                auto& table = MediaResources[type];
                // Нужно затереть старую ссылку хеша на данный ресурс
                if(auto iter = table.find(pending.Id); iter != table.end())
                    HashToId.erase(iter->second.Hash);

                // Добавили ресурс
                table[pending.Id] = resource;
                // Связали с хешем
                HashToId[resource.Hash] = {static_cast<AssetType>(type), pending.Id};
                // Осведомили о новом/изменённом ресурсе
                result.NewOrChange[type].emplace_back(pending.Id, resource.Hash, std::move(resource.Header));
            }
        }

        // Не должно быть ресурсов, которые были помечены как потерянные
        #ifndef NDEBUG
        std::unordered_set<uint32_t> changed;
        for(const auto& [id, _, _2] : result.NewOrChange[type])
            changed.insert(id);

        auto& lost = result.Lost[type];
        for(auto iter : lost)
            assert(!changed.contains(iter));
        #endif
    }

    return result;
}

AssetsPreloader::Out_bakeId AssetsPreloader::bakeIdTables() {
    #ifndef NDEBUG

    assert(!DKToIdInBakingMode);
    DKToIdInBakingMode = true;
    struct _tempStruct {
        AssetsPreloader* handler;
        ~_tempStruct() { handler->DKToIdInBakingMode = false; }
    } _lock{this};

    #endif

    Out_bakeId result;

    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        // домен+ключ -> id
        {
            auto lock = NewDKToId[type].lock();
            auto& dkToId = DKToId[type];
            for(auto& [domain, keys] : *lock) {
                // Если домен не существует, просто воткнёт новые ключи
                auto [iterDomain, inserted] = dkToId.try_emplace(domain, std::move(keys));
                if(!inserted) {
                    // Домен уже существует, сливаем новые ключи
                    iterDomain->second.merge(keys);
                }
            }

            lock->clear();
        }

        // id -> домен+ключ
        {
            auto lock = NewIdToDK[type].lock();

            auto& idToDK = IdToDK[type];
            result.IdToDK[type] = std::move(*lock);
            lock->clear();
            idToDK.append_range(result.IdToDK[type]);

            // result.LastSendId[type] = LastSendId[type];
            LastSendId[type] = NextId[type];
        }
    }

    return result;
}

AssetsPreloader::Out_fullSync AssetsPreloader::collectFullSync() const {
    Out_fullSync out;

    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        out.IdToDK[type] = IdToDK[type];
    }

    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        for(const auto& [id, resource] : MediaResources[type]) {
            out.HashHeaders[type].push_back(BindHashHeaderInfo{
                .Id = id,
                .Hash = resource.Hash,
                .Header = resource.Header
            });
            out.Resources.emplace_back(
                static_cast<AssetType>(type),
                id,
                &resource
            );
        }
    }

    return out;
}

std::tuple<AssetsNodestate, std::vector<AssetsModel>, std::vector<AssetsTexture>>
AssetsPreloader::getNodeDependency(const std::string& domain, const std::string& key) {
    (void)domain;
    (void)key;
    return {0, {}, {}};
}

}
