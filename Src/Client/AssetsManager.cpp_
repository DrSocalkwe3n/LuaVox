#include "AssetsManager.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include "Common/TexturePipelineProgram.hpp"

namespace LV::Client {

namespace {

static const char* assetTypeName(EnumAssets type) {
    switch(type) {
    case EnumAssets::Nodestate: return "nodestate";
    case EnumAssets::Model: return "model";
    case EnumAssets::Texture: return "texture";
    case EnumAssets::Particle: return "particle";
    case EnumAssets::Animation: return "animation";
    case EnumAssets::Sound: return "sound";
    case EnumAssets::Font: return "font";
    default: return "unknown";
    }
}

static const char* enumAssetsToDirectory(LV::EnumAssets value) {
    switch(value) {
    case LV::EnumAssets::Nodestate: return "nodestate";
    case LV::EnumAssets::Particle: return "particle";
    case LV::EnumAssets::Animation: return "animation";
    case LV::EnumAssets::Model: return "model";
    case LV::EnumAssets::Texture: return "texture";
    case LV::EnumAssets::Sound: return "sound";
    case LV::EnumAssets::Font: return "font";
    default:
        break;
    }

    assert(!"Unknown asset type");
    return "";
}

static std::u8string readFileBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file)
        throw std::runtime_error("Не удалось открыть файл: " + path.string());

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if(size < 0)
        size = 0;
    file.seekg(0, std::ios::beg);

    std::u8string data;
    data.resize(static_cast<size_t>(size));
    if(size > 0) {
        file.read(reinterpret_cast<char*>(data.data()), size);
        if(!file)
            throw std::runtime_error("Не удалось прочитать файл: " + path.string());
    }

    return data;
}

static std::u8string readOptionalMeta(const fs::path& path) {
    fs::path metaPath = path;
    metaPath += ".meta";
    if(!fs::exists(metaPath) || !fs::is_regular_file(metaPath))
        return {};

    return readFileBytes(metaPath);
}

} // namespace
AssetsManager::AssetsManager(asio::io_context& ioc, const fs::path& cachePath,
    size_t maxCacheDirectorySize, size_t maxLifeTime)
    : Cache(AssetsCacheManager::Create(ioc, cachePath, maxCacheDirectorySize, maxLifeTime))
{
    for(size_t i = 0; i < static_cast<size_t>(AssetType::MAX_ENUM); ++i)
        Types[i].NextLocalId = 1;
    initSources();
}

void AssetsManager::initSources() {
    using SourceResult = AssetsManager::SourceResult;
    using SourceStatus = AssetsManager::SourceStatus;
    using SourceReady = AssetsManager::SourceReady;
    using ResourceKey = AssetsManager::ResourceKey;
    using PackResource = AssetsManager::PackResource;

    class PackSource final : public IResourceSource {
    public:
        explicit PackSource(AssetsManager* manager) : Manager(manager) {}

        SourceResult tryGet(const ResourceKey& key) override {
            std::optional<PackResource> pack = Manager->findPackResource(key.Type, key.Domain, key.Key);
            if(pack && pack->Hash == key.Hash)
                return {SourceStatus::Hit, pack->Res, 0};
            return {SourceStatus::Miss, std::nullopt, 0};
        }

        void collectReady(std::vector<SourceReady>&) override {}

        bool isAsync() const override {
            return false;
        }

        void startPending(std::vector<Hash_t>) override {}

    private:
        AssetsManager* Manager = nullptr;
    };

    class MemorySource final : public IResourceSource {
    public:
        explicit MemorySource(AssetsManager* manager) : Manager(manager) {}

        SourceResult tryGet(const ResourceKey& key) override {
            auto iter = Manager->MemoryResourcesByHash.find(key.Hash);
            if(iter == Manager->MemoryResourcesByHash.end())
                return {SourceStatus::Miss, std::nullopt, 0};
            return {SourceStatus::Hit, iter->second, 0};
        }

        void collectReady(std::vector<SourceReady>&) override {}

        bool isAsync() const override {
            return false;
        }

        void startPending(std::vector<Hash_t>) override {}

    private:
        AssetsManager* Manager = nullptr;
    };

    class CacheSource final : public IResourceSource {
    public:
        CacheSource(AssetsManager* manager, size_t sourceIndex)
            : Manager(manager), SourceIndex(sourceIndex) {}

        SourceResult tryGet(const ResourceKey&) override {
            return {SourceStatus::Pending, std::nullopt, SourceIndex};
        }

        void collectReady(std::vector<SourceReady>& out) override {
            std::vector<std::pair<Hash_t, std::optional<Resource>>> cached = Manager->Cache->pullReads();
            out.reserve(out.size() + cached.size());
            for(auto& [hash, res] : cached)
                out.push_back(SourceReady{hash, res, SourceIndex});
        }

        bool isAsync() const override {
            return true;
        }

        void startPending(std::vector<Hash_t> hashes) override {
            if(!hashes.empty())
                Manager->Cache->pushReads(std::move(hashes));
        }

    private:
        AssetsManager* Manager = nullptr;
        size_t SourceIndex = 0;
    };

    Sources.clear();
    PackSourceIndex = Sources.size();
    Sources.push_back(SourceEntry{std::make_unique<PackSource>(this), 0});
    MemorySourceIndex = Sources.size();
    Sources.push_back(SourceEntry{std::make_unique<MemorySource>(this), 0});
    CacheSourceIndex = Sources.size();
    Sources.push_back(SourceEntry{std::make_unique<CacheSource>(this, CacheSourceIndex), 0});
}

void AssetsManager::collectReadyFromSources() {
    std::vector<SourceReady> ready;
    for(auto& entry : Sources)
        entry.Source->collectReady(ready);

    for(SourceReady& item : ready) {
        auto iter = PendingReadsByHash.find(item.Hash);
        if(iter == PendingReadsByHash.end())
            continue;
        if(item.Value)
            registerSourceHit(item.Hash, item.SourceIndex);
        for(ResourceKey& key : iter->second) {
            if(item.SourceIndex == CacheSourceIndex) {
                if(item.Value) {
                    LOG.debug() << "Cache hit type=" << assetTypeName(key.Type)
                        << " id=" << key.Id
                        << " key=" << key.Domain << ':' << key.Key
                        << " hash=" << int(item.Hash[0]) << '.'
                        << int(item.Hash[1]) << '.'
                        << int(item.Hash[2]) << '.'
                        << int(item.Hash[3])
                        << " size=" << item.Value->size();
                } else {
                    LOG.debug() << "Cache miss type=" << assetTypeName(key.Type)
                        << " id=" << key.Id
                        << " key=" << key.Domain << ':' << key.Key
                        << " hash=" << int(item.Hash[0]) << '.'
                        << int(item.Hash[1]) << '.'
                        << int(item.Hash[2]) << '.'
                        << int(item.Hash[3]);
                }
            }
            ReadyReads.emplace_back(std::move(key), item.Value);
        }
        PendingReadsByHash.erase(iter);
    }
}

AssetsManager::SourceResult AssetsManager::querySources(const ResourceKey& key) {
    auto cacheIter = SourceCacheByHash.find(key.Hash);
    if(cacheIter != SourceCacheByHash.end()) {
        const size_t cachedIndex = cacheIter->second.SourceIndex;
        if(cachedIndex < Sources.size()
            && cacheIter->second.Generation == Sources[cachedIndex].Generation)
        {
            SourceResult cached = Sources[cachedIndex].Source->tryGet(key);
            cached.SourceIndex = cachedIndex;
            if(cached.Status != SourceStatus::Miss)
                return cached;
        }
        SourceCacheByHash.erase(cacheIter);
    }

    SourceResult pending;
    pending.Status = SourceStatus::Miss;
    for(size_t i = 0; i < Sources.size(); ++i) {
        SourceResult res = Sources[i].Source->tryGet(key);
        res.SourceIndex = i;
        if(res.Status == SourceStatus::Hit) {
            registerSourceHit(key.Hash, i);
            return res;
        }
        if(res.Status == SourceStatus::Pending && pending.Status == SourceStatus::Miss)
            pending = res;
    }

    return pending;
}

void AssetsManager::registerSourceHit(const Hash_t& hash, size_t sourceIndex) {
    if(sourceIndex >= Sources.size())
        return;
    if(Sources[sourceIndex].Source->isAsync())
        return;
    SourceCacheByHash[hash] = SourceCacheEntry{
        .SourceIndex = sourceIndex,
        .Generation = Sources[sourceIndex].Generation
    };
}

void AssetsManager::invalidateSourceCache(size_t sourceIndex) {
    if(sourceIndex >= Sources.size())
        return;
    Sources[sourceIndex].Generation++;
    for(auto iter = SourceCacheByHash.begin(); iter != SourceCacheByHash.end(); ) {
        if(iter->second.SourceIndex == sourceIndex)
            iter = SourceCacheByHash.erase(iter);
        else
            ++iter;
    }
}

void AssetsManager::invalidateAllSourceCache() {
    for(auto& entry : Sources)
        entry.Generation++;
    SourceCacheByHash.clear();
}

void AssetsManager::tickSources() {
    collectReadyFromSources();
}

AssetsManager::PackReloadResult AssetsManager::reloadPacks(const PackRegister& reg) {
    PackReloadResult result;
    std::array<PackTable, static_cast<size_t>(AssetType::MAX_ENUM)> oldPacks;
    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        oldPacks[type] = Types[type].PackResources;
        Types[type].PackResources.clear();
    }

    for(const fs::path& instance : reg.Packs) {
        try {
            if(fs::is_regular_file(instance)) {
                LOG.warn() << "Архивы ресурспаков пока не поддерживаются: " << instance.string();
                continue;
            }
            if(!fs::is_directory(instance)) {
                LOG.warn() << "Неизвестный тип ресурспака: " << instance.string();
                continue;
            }

            fs::path assetsRoot = instance;
            fs::path assetsCandidate = instance / "assets";
            if(fs::exists(assetsCandidate) && fs::is_directory(assetsCandidate))
                assetsRoot = assetsCandidate;

            for(auto begin = fs::directory_iterator(assetsRoot), end = fs::directory_iterator(); begin != end; ++begin) {
                if(!begin->is_directory())
                    continue;

                fs::path domainPath = begin->path();
                std::string domain = domainPath.filename().string();

                for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
                    AssetType assetType = static_cast<AssetType>(type);
                    fs::path assetPath = domainPath / enumAssetsToDirectory(assetType);
                    if(!fs::exists(assetPath) || !fs::is_directory(assetPath))
                        continue;

                    auto& typeTable = Types[type].PackResources[domain];
                    for(auto fbegin = fs::recursive_directory_iterator(assetPath),
                             fend = fs::recursive_directory_iterator();
                        fbegin != fend; ++fbegin) {
                        if(fbegin->is_directory())
                            continue;
                        fs::path file = fbegin->path();
                        if(assetType == AssetType::Texture && file.extension() == ".meta")
                            continue;

                        std::string key = fs::relative(file, assetPath).generic_string();
                        if(typeTable.contains(key))
                            continue;

                        PackResource entry;
                        entry.Type = assetType;
                        entry.Domain = domain;
                        entry.Key = key;
                        entry.LocalId = getOrCreateLocalId(assetType, entry.Domain, entry.Key);

                        try {
                            if(assetType == AssetType::Nodestate) {
                                std::u8string data = readFileBytes(file);
                                std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
                                js::object obj = js::parse(view).as_object();

                                HeadlessNodeState hns;
                                auto modelResolver = [&](std::string_view model) -> AssetsModel {
                                    auto [mDomain, mKey] = parseDomainKey(model, entry.Domain);
                                    return getOrCreateLocalId(AssetType::Model, mDomain, mKey);
                                };

                                entry.Header = hns.parse(obj, modelResolver);
                                std::u8string compiled = hns.dump();
                                entry.Res = Resource(std::move(compiled));
                                entry.Hash = entry.Res.hash();
                            } else if(assetType == AssetType::Model) {
                                const std::string ext = file.extension().string();
                                if(ext == ".json") {
                                    std::u8string data = readFileBytes(file);
                                    std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
                                    js::object obj = js::parse(view).as_object();

                                    HeadlessModel hm;
                                    auto modelResolver = [&](std::string_view model) -> AssetsModel {
                                        auto [mDomain, mKey] = parseDomainKey(model, entry.Domain);
                                        return getOrCreateLocalId(AssetType::Model, mDomain, mKey);
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

                                    auto textureResolver = [&](std::string_view textureSrc) -> std::vector<uint8_t> {
                                        TexturePipelineProgram tpp;
                                        if(!tpp.compile(normalizeTexturePipelineSrc(textureSrc)))
                                            return {};
                                        auto textureIdResolver = [&](std::string_view name) -> std::optional<uint32_t> {
                                            auto [tDomain, tKey] = parseDomainKey(name, entry.Domain);
                                            return getOrCreateLocalId(AssetType::Texture, tDomain, tKey);
                                        };
                                        if(!tpp.link(textureIdResolver))
                                            return {};
                                        return tpp.toBytes();
                                    };

                                    entry.Header = hm.parse(obj, modelResolver, textureResolver);
                                    std::u8string compiled = hm.dump();
                                    entry.Res = Resource(std::move(compiled));
                                    entry.Hash = entry.Res.hash();
                                } else {
                                    LOG.warn() << "Не поддерживаемый формат модели: " << file.string();
                                    continue;
                                }
                            } else if(assetType == AssetType::Texture) {
                                std::u8string data = readFileBytes(file);
                                entry.Res = Resource(std::move(data));
                                entry.Hash = entry.Res.hash();
                                entry.Header = readOptionalMeta(file);
                            } else {
                                std::u8string data = readFileBytes(file);
                                entry.Res = Resource(std::move(data));
                                entry.Hash = entry.Res.hash();
                            }
                        } catch(const std::exception& exc) {
                            LOG.warn() << "Ошибка загрузки ресурса " << file.string() << ": " << exc.what();
                            continue;
                        }

                        typeTable.emplace(entry.Key, entry);
                    }
                }
            }
        } catch(const std::exception& exc) {
            LOG.warn() << "Ошибка загрузки ресурспака " << instance.string() << ": " << exc.what();
        }
    }

    for(size_t type = 0; type < static_cast<size_t>(AssetType::MAX_ENUM); ++type) {
        for(const auto& [domain, keyTable] : Types[type].PackResources) {
            for(const auto& [key, res] : keyTable) {
                bool changed = true;
                auto oldDomain = oldPacks[type].find(domain);
                if(oldDomain != oldPacks[type].end()) {
                    auto oldKey = oldDomain->second.find(key);
                    if(oldKey != oldDomain->second.end()) {
                        changed = oldKey->second.Hash != res.Hash;
                    }
                }
                if(changed)
                    result.ChangeOrAdd[type].push_back(res.LocalId);
            }
        }

        for(const auto& [domain, keyTable] : oldPacks[type]) {
            for(const auto& [key, res] : keyTable) {
                auto newDomain = Types[type].PackResources.find(domain);
                bool lost = true;
                if(newDomain != Types[type].PackResources.end()) {
                    if(newDomain->second.contains(key))
                        lost = false;
                }
                if(lost)
                    result.Lost[type].push_back(res.LocalId);
            }
        }
    }

    invalidateAllSourceCache();
    return result;
}

AssetsManager::BindResult AssetsManager::bindServerResource(AssetType type, AssetId serverId,
    std::string domain, std::string key, const Hash_t& hash, std::vector<uint8_t> header)
{
    BindResult result;
    AssetId localFromDK = getOrCreateLocalId(type, domain, key);
    auto& map = Types[static_cast<size_t>(type)].ServerToLocal;
    AssetId localFromServer = 0;
    if(serverId < map.size())
        localFromServer = map[serverId];

    if(localFromServer != 0)
        unionLocalIds(type, localFromServer, localFromDK, &result.ReboundFrom);
    AssetId localId = resolveLocalIdMutable(type, localFromDK);

    if(serverId >= map.size())
        map.resize(serverId + 1, 0);
    map[serverId] = localId;

    auto& infoList = Types[static_cast<size_t>(type)].BindInfos;
    if(localId >= infoList.size())
        infoList.resize(localId + 1);

    bool hadBinding = infoList[localId].has_value();
    bool changed = !hadBinding || infoList[localId]->Hash != hash || infoList[localId]->Header != header;

    infoList[localId] = BindInfo{
        .Type = type,
        .LocalId = localId,
        .Domain = std::move(domain),
        .Key = std::move(key),
        .Hash = hash,
        .Header = std::move(header)
    };

    result.LocalId = localId;
    result.Changed = changed;
    result.NewBinding = !hadBinding;
    return result;
}

std::optional<AssetsManager::AssetId> AssetsManager::unbindServerResource(AssetType type, AssetId serverId) {
    auto& map = Types[static_cast<size_t>(type)].ServerToLocal;
    if(serverId >= map.size())
        return std::nullopt;
    AssetId localId = map[serverId];
    map[serverId] = 0;
    if(localId == 0)
        return std::nullopt;
    return resolveLocalIdMutable(type, localId);
}

void AssetsManager::clearServerBindings() {
    for(auto& typeData : Types) {
        typeData.ServerToLocal.clear();
        typeData.BindInfos.clear();
    }
}

const AssetsManager::BindInfo* AssetsManager::getBind(AssetType type, AssetId localId) const {
    localId = resolveLocalId(type, localId);
    const auto& table = Types[static_cast<size_t>(type)].BindInfos;
    if(localId >= table.size())
        return nullptr;
    if(!table[localId])
        return nullptr;
    return &*table[localId];
}

std::vector<uint8_t> AssetsManager::rebindHeader(AssetType type, const std::vector<uint8_t>& header, bool serverIds) {
    auto mapModelId = [&](AssetId id) -> AssetId {
        if(serverIds) {
            auto localId = getLocalIdFromServer(AssetType::Model, id);
            if(!localId) {
                assert(!"Missing server bind for model id");
                MAKE_ERROR("Нет бинда сервера для модели id=" << id);
            }
            return *localId;
        }
        return resolveLocalIdMutable(AssetType::Model, id);
    };

    auto mapTextureId = [&](AssetId id) -> AssetId {
        if(serverIds) {
            auto localId = getLocalIdFromServer(AssetType::Texture, id);
            if(!localId) {
                assert(!"Missing server bind for texture id");
                MAKE_ERROR("Нет бинда сервера для текстуры id=" << id);
            }
            return *localId;
        }
        return resolveLocalIdMutable(AssetType::Texture, id);
    };

    auto warn = [&](const std::string& msg) {
        LOG.warn() << msg;
    };

    return AssetsHeaderCodec::rebindHeader(type, header, mapModelId, mapTextureId, warn);
}

std::optional<AssetsManager::ParsedHeader> AssetsManager::parseHeader(AssetType type, const std::vector<uint8_t>& header) {
    return AssetsHeaderCodec::parseHeader(type, header);
}

void AssetsManager::pushResources(std::vector<Resource> resources) {
    for(const Resource& res : resources) {
        Hash_t hash = res.hash();
        MemoryResourcesByHash[hash] = res;
        SourceCacheByHash.erase(hash);
        registerSourceHit(hash, MemorySourceIndex);

        auto iter = PendingReadsByHash.find(hash);
        if(iter != PendingReadsByHash.end()) {
            for(ResourceKey& key : iter->second)
                ReadyReads.emplace_back(std::move(key), res);
            PendingReadsByHash.erase(iter);
        }
    }

    Cache->pushResources(std::move(resources));
}

void AssetsManager::pushReads(std::vector<ResourceKey> reads) {
    std::unordered_map<size_t, std::vector<Hash_t>> pendingBySource;

    for(ResourceKey& key : reads) {
        SourceResult res = querySources(key);
        if(res.Status == SourceStatus::Hit) {
            if(res.SourceIndex == PackSourceIndex && res.Value) {
                LOG.debug() << "Pack hit type=" << assetTypeName(key.Type)
                    << " id=" << key.Id
                    << " key=" << key.Domain << ':' << key.Key
                    << " hash=" << int(key.Hash[0]) << '.'
                    << int(key.Hash[1]) << '.'
                    << int(key.Hash[2]) << '.'
                    << int(key.Hash[3])
                    << " size=" << res.Value->size();
            }
            ReadyReads.emplace_back(std::move(key), res.Value);
            continue;
        }

        if(res.Status == SourceStatus::Pending) {
            auto& list = PendingReadsByHash[key.Hash];
            bool isFirst = list.empty();
            list.push_back(std::move(key));
            if(isFirst)
                pendingBySource[res.SourceIndex].push_back(list.front().Hash);
            continue;
        }

        ReadyReads.emplace_back(std::move(key), std::nullopt);
    }

    for(auto& [sourceIndex, hashes] : pendingBySource) {
        if(sourceIndex < Sources.size())
            Sources[sourceIndex].Source->startPending(std::move(hashes));
    }
}

std::vector<std::pair<AssetsManager::ResourceKey, std::optional<Resource>>> AssetsManager::pullReads() {
    tickSources();

    std::vector<std::pair<ResourceKey, std::optional<Resource>>> out;
    out.reserve(ReadyReads.size());

    for(auto& entry : ReadyReads)
        out.emplace_back(std::move(entry));
    ReadyReads.clear();

    return out;
}

AssetsManager::AssetId AssetsManager::getOrCreateLocalId(AssetType type, std::string_view domain, std::string_view key) {
    auto& table = Types[static_cast<size_t>(type)].DKToLocal;
    auto iterDomain = table.find(domain);
    if(iterDomain == table.end()) {
        iterDomain = table.emplace(
            std::string(domain),
            std::unordered_map<std::string, AssetId, detail::TSVHash, detail::TSVEq>{}
        ).first;
    }

    auto& keyTable = iterDomain->second;
    auto iterKey = keyTable.find(key);
    if(iterKey != keyTable.end()) {
        iterKey->second = resolveLocalIdMutable(type, iterKey->second);
        return iterKey->second;
    }

    AssetId id = allocateLocalId(type);
    keyTable.emplace(std::string(key), id);

    auto& dk = Types[static_cast<size_t>(type)].LocalToDK;
    if(id >= dk.size())
        dk.resize(id + 1);
    dk[id] = DomainKey{std::string(domain), std::string(key), true};

    return id;
}

std::optional<AssetsManager::AssetId> AssetsManager::getLocalIdFromServer(AssetType type, AssetId serverId) const {
    const auto& map = Types[static_cast<size_t>(type)].ServerToLocal;
    if(serverId >= map.size())
        return std::nullopt;
    AssetId local = map[serverId];
    if(local == 0)
        return std::nullopt;
    return resolveLocalId(type, local);
}

AssetsManager::AssetId AssetsManager::resolveLocalId(AssetType type, AssetId localId) const {
    if(localId == 0)
        return 0;
    const auto& parents = Types[static_cast<size_t>(type)].LocalParent;
    if(localId >= parents.size())
        return localId;
    AssetId cur = localId;
    while(cur < parents.size() && parents[cur] != cur && parents[cur] != 0)
        cur = parents[cur];
    return cur;
}

AssetsManager::AssetId AssetsManager::allocateLocalId(AssetType type) {
    auto& next = Types[static_cast<size_t>(type)].NextLocalId;
    AssetId id = next++;

    auto& parents = Types[static_cast<size_t>(type)].LocalParent;
    if(id >= parents.size())
        parents.resize(id + 1, 0);
    parents[id] = id;

    auto& dk = Types[static_cast<size_t>(type)].LocalToDK;
    if(id >= dk.size())
        dk.resize(id + 1);

    return id;
}

AssetsManager::AssetId AssetsManager::resolveLocalIdMutable(AssetType type, AssetId localId) {
    if(localId == 0)
        return 0;
    auto& parents = Types[static_cast<size_t>(type)].LocalParent;
    if(localId >= parents.size())
        return localId;
    AssetId root = localId;
    while(root < parents.size() && parents[root] != root && parents[root] != 0)
        root = parents[root];
    if(root == localId)
        return root;
    AssetId cur = localId;
    while(cur < parents.size() && parents[cur] != root && parents[cur] != 0) {
        AssetId next = parents[cur];
        parents[cur] = root;
        cur = next;
    }
    return root;
}

void AssetsManager::unionLocalIds(AssetType type, AssetId fromId, AssetId toId, std::optional<AssetId>* reboundFrom) {
    AssetId fromRoot = resolveLocalIdMutable(type, fromId);
    AssetId toRoot = resolveLocalIdMutable(type, toId);
    if(fromRoot == 0 || toRoot == 0 || fromRoot == toRoot)
        return;

    auto& parents = Types[static_cast<size_t>(type)].LocalParent;
    if(fromRoot >= parents.size() || toRoot >= parents.size())
        return;

    parents[fromRoot] = toRoot;
    if(reboundFrom)
        *reboundFrom = fromRoot;

    auto& dk = Types[static_cast<size_t>(type)].LocalToDK;
    if(fromRoot < dk.size()) {
        const DomainKey& fromDK = dk[fromRoot];
        if(fromDK.Known) {
            if(toRoot >= dk.size())
                dk.resize(toRoot + 1);
            DomainKey& toDK = dk[toRoot];
            if(!toDK.Known) {
                toDK = fromDK;
                Types[static_cast<size_t>(type)].DKToLocal[toDK.Domain][toDK.Key] = toRoot;
            } else if(toDK.Domain != fromDK.Domain || toDK.Key != fromDK.Key) {
                LOG.warn() << "Конфликт домен/ключ при ребинде: "
                    << fromDK.Domain << ':' << fromDK.Key << " vs "
                    << toDK.Domain << ':' << toDK.Key;
            }
        }
    }

    auto& binds = Types[static_cast<size_t>(type)].BindInfos;
    if(fromRoot < binds.size()) {
        if(toRoot >= binds.size())
            binds.resize(toRoot + 1);
        if(!binds[toRoot] && binds[fromRoot])
            binds[toRoot] = std::move(binds[fromRoot]);
    }
}

std::optional<AssetsManager::PackResource> AssetsManager::findPackResource(AssetType type,
    std::string_view domain, std::string_view key) const
{
    const auto& typeTable = Types[static_cast<size_t>(type)].PackResources;
    auto iterDomain = typeTable.find(domain);
    if(iterDomain == typeTable.end())
        return std::nullopt;
    auto iterKey = iterDomain->second.find(key);
    if(iterKey == iterDomain->second.end())
        return std::nullopt;
    return iterKey->second;
}

} // namespace LV::Client
