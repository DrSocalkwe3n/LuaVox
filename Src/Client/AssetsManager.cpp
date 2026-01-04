#include "AssetsManager.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <fstream>
#include <unordered_set>
#include "Common/Net.hpp"
#include "Common/TexturePipelineProgram.hpp"

namespace LV::Client {

namespace {

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

struct PipelineRemapResult {
    bool Ok = true;
    std::string Error;
};

PipelineRemapResult remapTexturePipelineIds(std::vector<uint8_t>& code,
    const std::function<uint32_t(uint32_t)>& mapId)
{
    struct Range {
        size_t Start = 0;
        size_t End = 0;
    };

    enum class SrcKind : uint8_t { TexId = 0, Sub = 1 };
    enum class Op : uint8_t {
        End = 0,
        Base_Tex  = 1,
        Base_Fill = 2,
        Base_Anim = 3,
        Resize    = 10,
        Transform = 11,
        Opacity   = 12,
        NoAlpha   = 13,
        MakeAlpha = 14,
        Invert    = 15,
        Brighten  = 16,
        Contrast  = 17,
        Multiply  = 18,
        Screen    = 19,
        Colorize  = 20,
        Anim      = 21,
        Overlay   = 30,
        Mask      = 31,
        LowPart   = 32,
        Combine   = 40
    };

    struct SrcMeta {
        SrcKind Kind = SrcKind::TexId;
        uint32_t TexId = 0;
        uint32_t Off = 0;
        uint32_t Len = 0;
        size_t TexIdOffset = 0;
    };

    const size_t size = code.size();
    std::vector<Range> visited;

    auto read8 = [&](size_t& ip, uint8_t& out) -> bool {
        if(ip >= size)
            return false;
        out = code[ip++];
        return true;
    };

    auto read16 = [&](size_t& ip, uint16_t& out) -> bool {
        if(ip + 1 >= size)
            return false;
        out = uint16_t(code[ip]) | (uint16_t(code[ip + 1]) << 8);
        ip += 2;
        return true;
    };

    auto read24 = [&](size_t& ip, uint32_t& out) -> bool {
        if(ip + 2 >= size)
            return false;
        out = uint32_t(code[ip]) |
            (uint32_t(code[ip + 1]) << 8) |
            (uint32_t(code[ip + 2]) << 16);
        ip += 3;
        return true;
    };

    auto read32 = [&](size_t& ip, uint32_t& out) -> bool {
        if(ip + 3 >= size)
            return false;
        out = uint32_t(code[ip]) |
            (uint32_t(code[ip + 1]) << 8) |
            (uint32_t(code[ip + 2]) << 16) |
            (uint32_t(code[ip + 3]) << 24);
        ip += 4;
        return true;
    };

    auto readSrc = [&](size_t& ip, SrcMeta& out) -> bool {
        uint8_t kind = 0;
        if(!read8(ip, kind))
            return false;
        out.Kind = static_cast<SrcKind>(kind);
        if(out.Kind == SrcKind::TexId) {
            out.TexIdOffset = ip;
            return read24(ip, out.TexId);
        }
        if(out.Kind == SrcKind::Sub) {
            return read24(ip, out.Off) && read24(ip, out.Len);
        }
        return false;
    };

    auto patchTexId = [&](const SrcMeta& src) -> bool {
        if(src.Kind != SrcKind::TexId)
            return true;
        uint32_t newId = mapId(src.TexId);
        if(newId >= (1u << 24))
            return false;
        if(src.TexIdOffset + 2 >= code.size())
            return false;
        code[src.TexIdOffset + 0] = uint8_t(newId & 0xFFu);
        code[src.TexIdOffset + 1] = uint8_t((newId >> 8) & 0xFFu);
        code[src.TexIdOffset + 2] = uint8_t((newId >> 16) & 0xFFu);
        return true;
    };

    std::move_only_function<bool(size_t, size_t)> scan;
    scan = [&](size_t start, size_t end) -> bool {
        if(start >= end || end > size)
            return true;
        for(const auto& r : visited) {
            if(r.Start == start && r.End == end)
                return true;
        }
        visited.push_back(Range{start, end});

        size_t ip = start;
        while(ip < end) {
            uint8_t opByte = 0;
            if(!read8(ip, opByte))
                return false;
            Op op = static_cast<Op>(opByte);
            switch(op) {
            case Op::End:
                return true;

            case Op::Base_Tex: {
                SrcMeta src{};
                if(!readSrc(ip, src))
                    return false;
                if(!patchTexId(src))
                    return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd))
                        return false;
                }
            } break;

            case Op::Base_Fill: {
                uint16_t tmp16 = 0;
                uint32_t tmp32 = 0;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read32(ip, tmp32))
                    return false;
            } break;

            case Op::Base_Anim: {
                SrcMeta src{};
                if(!readSrc(ip, src))
                    return false;
                if(!patchTexId(src))
                    return false;
                uint16_t tmp16 = 0;
                uint8_t tmp8 = 0;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read8(ip, tmp8))
                    return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd))
                        return false;
                }
            } break;

            case Op::Resize: {
                uint16_t tmp16 = 0;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
            } break;

            case Op::Transform:
            case Op::Opacity:
            case Op::Invert: {
                uint8_t tmp8 = 0;
                if(!read8(ip, tmp8))
                    return false;
            } break;

            case Op::NoAlpha:
            case Op::Brighten:
                break;

            case Op::MakeAlpha:
                if(ip + 2 >= size)
                    return false;
                ip += 3;
                break;

            case Op::Contrast:
                if(ip + 1 >= size)
                    return false;
                ip += 2;
                break;

            case Op::Multiply:
            case Op::Screen: {
                uint32_t tmp32 = 0;
                if(!read32(ip, tmp32))
                    return false;
            } break;

            case Op::Colorize: {
                uint32_t tmp32 = 0;
                uint8_t tmp8 = 0;
                if(!read32(ip, tmp32))
                    return false;
                if(!read8(ip, tmp8))
                    return false;
            } break;

            case Op::Anim: {
                uint16_t tmp16 = 0;
                uint8_t tmp8 = 0;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                if(!read8(ip, tmp8))
                    return false;
            } break;

            case Op::Overlay:
            case Op::Mask: {
                SrcMeta src{};
                if(!readSrc(ip, src))
                    return false;
                if(!patchTexId(src))
                    return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd))
                        return false;
                }
            } break;

            case Op::LowPart: {
                uint8_t tmp8 = 0;
                if(!read8(ip, tmp8))
                    return false;
                SrcMeta src{};
                if(!readSrc(ip, src))
                    return false;
                if(!patchTexId(src))
                    return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd))
                        return false;
                }
            } break;

            case Op::Combine: {
                uint16_t tmp16 = 0;
                if(!read16(ip, tmp16))
                    return false;
                if(!read16(ip, tmp16))
                    return false;
                uint16_t count = 0;
                if(!read16(ip, count))
                    return false;
                for(uint16_t i = 0; i < count; ++i) {
                    if(!read16(ip, tmp16))
                        return false;
                    if(!read16(ip, tmp16))
                        return false;
                    SrcMeta src{};
                    if(!readSrc(ip, src))
                        return false;
                    if(!patchTexId(src))
                        return false;
                    if(src.Kind == SrcKind::Sub) {
                        size_t subStart = src.Off;
                        size_t subEnd = subStart + src.Len;
                        if(!scan(subStart, subEnd))
                            return false;
                    }
                }
            } break;

            default:
                return false;
            }
        }

        return true;
    };

    if(!scan(0, size))
        return {false, "Invalid texture pipeline bytecode"};

    return {};
}

static std::vector<uint32_t> collectTexturePipelineIds(const std::vector<uint8_t>& code) {
    std::vector<uint32_t> out;
    std::unordered_set<uint32_t> seen;

    auto addId = [&](uint32_t id) {
        if(seen.insert(id).second)
            out.push_back(id);
    };

    std::vector<uint8_t> copy = code;
    auto result = remapTexturePipelineIds(copy, [&](uint32_t id) {
        addId(id);
        return id;
    });

    if(!result.Ok)
        return {};

    return out;
}

} // namespace

AssetsManager::AssetsManager(asio::io_context& ioc, const fs::path& cachePath,
    size_t maxCacheDirectorySize, size_t maxLifeTime)
    : Cache(AssetsCacheManager::Create(ioc, cachePath, maxCacheDirectorySize, maxLifeTime))
{
    for(size_t i = 0; i < static_cast<size_t>(AssetType::MAX_ENUM); ++i)
        NextLocalId[i] = 1;
}

AssetsManager::PackReloadResult AssetsManager::reloadPacks(const PackRegister& reg) {
    PackReloadResult result;
    auto oldPacks = PackResources;
    for(auto& table : PackResources)
        table.clear();

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

                    auto& typeTable = PackResources[type][domain];
                    for(auto fbegin = fs::recursive_directory_iterator(assetPath),
                             fend = fs::recursive_directory_iterator();
                        fbegin != fend; ++fbegin) {
                        if(fbegin->is_directory())
                            continue;
                        fs::path file = fbegin->path();
                        if(assetType == AssetType::Texture && file.extension() == ".meta")
                            continue;

                        std::string key = fs::relative(file, assetPath).string();
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
                                    auto textureResolver = [&](std::string_view textureSrc) -> std::vector<uint8_t> {
                                        TexturePipelineProgram tpp;
                                        if(!tpp.compile(std::string(textureSrc)))
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
        for(const auto& [domain, keyTable] : PackResources[type]) {
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
                auto newDomain = PackResources[type].find(domain);
                bool lost = true;
                if(newDomain != PackResources[type].end()) {
                    if(newDomain->second.contains(key))
                        lost = false;
                }
                if(lost)
                    result.Lost[type].push_back(res.LocalId);
            }
        }
    }

    return result;
}

AssetsManager::BindResult AssetsManager::bindServerResource(AssetType type, AssetId serverId,
    std::string domain, std::string key, const Hash_t& hash, std::vector<uint8_t> header)
{
    BindResult result;
    AssetId localFromDK = getOrCreateLocalId(type, domain, key);
    AssetId localFromServer = ensureServerLocalId(type, serverId);

    unionLocalIds(type, localFromServer, localFromDK, &result.ReboundFrom);
    AssetId localId = resolveLocalIdMutable(type, localFromDK);

    auto& map = ServerToLocal[static_cast<size_t>(type)];
    if(serverId >= map.size())
        map.resize(serverId + 1, 0);
    map[serverId] = localId;

    auto& infoList = BindInfos[static_cast<size_t>(type)];
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
    auto& map = ServerToLocal[static_cast<size_t>(type)];
    if(serverId >= map.size())
        return std::nullopt;
    AssetId localId = map[serverId];
    map[serverId] = 0;
    if(localId == 0)
        return std::nullopt;
    return resolveLocalIdMutable(type, localId);
}

void AssetsManager::clearServerBindings() {
    for(auto& table : ServerToLocal)
        table.clear();
    for(auto& table : BindInfos)
        table.clear();
}

const AssetsManager::BindInfo* AssetsManager::getBind(AssetType type, AssetId localId) const {
    localId = resolveLocalId(type, localId);
    const auto& table = BindInfos[static_cast<size_t>(type)];
    if(localId >= table.size())
        return nullptr;
    if(!table[localId])
        return nullptr;
    return &*table[localId];
}

std::vector<uint8_t> AssetsManager::rebindHeader(AssetType type, const std::vector<uint8_t>& header, bool serverIds) {
    if(header.empty())
        return {};

    auto mapModelId = [&](AssetId id) -> AssetId {
        if(serverIds)
            return ensureServerLocalId(AssetType::Model, id);
        return resolveLocalIdMutable(AssetType::Model, id);
    };

    auto mapTextureId = [&](AssetId id) -> AssetId {
        if(serverIds)
            return ensureServerLocalId(AssetType::Texture, id);
        return resolveLocalIdMutable(AssetType::Texture, id);
    };

    if(type == AssetType::Nodestate) {
        if(header.size() % sizeof(AssetId) != 0)
            return header;
        std::vector<uint8_t> out(header.size());
        const size_t count = header.size() / sizeof(AssetId);
        for(size_t i = 0; i < count; ++i) {
            AssetId raw = 0;
            std::memcpy(&raw, header.data() + i * sizeof(AssetId), sizeof(AssetId));
            AssetId mapped = mapModelId(raw);
            std::memcpy(out.data() + i * sizeof(AssetId), &mapped, sizeof(AssetId));
        }
        return out;
    }

    if(type == AssetType::Model) {
        try {
            TOS::ByteBuffer buffer(header.size(), header.data());
            auto reader = buffer.reader();

            uint16_t modelCount = reader.readUInt16();
            std::vector<AssetId> models;
            models.reserve(modelCount);
            for(uint16_t i = 0; i < modelCount; ++i) {
                AssetId id = reader.readUInt32();
                models.push_back(mapModelId(id));
            }

            uint16_t texCount = reader.readUInt16();
            std::vector<std::vector<uint8_t>> pipelines;
            pipelines.reserve(texCount);
            for(uint16_t i = 0; i < texCount; ++i) {
                uint32_t size32 = reader.readUInt32();
                TOS::ByteBuffer pipe;
                reader.readBuffer(pipe);
                if(pipe.size() != size32) {
                    LOG.warn() << "Несовпадение длины pipeline: " << size32 << " vs " << pipe.size();
                }
                std::vector<uint8_t> code(pipe.begin(), pipe.end());
                auto result = remapTexturePipelineIds(code, [&](uint32_t id) {
                    return mapTextureId(static_cast<AssetId>(id));
                });
                if(!result.Ok) {
                    LOG.warn() << "Ошибка ребинда pipeline: " << result.Error;
                }
                pipelines.emplace_back(std::move(code));
            }

            TOS::ByteBuffer::Writer wr;
            wr << uint16_t(models.size());
            for(AssetId id : models)
                wr << id;
            wr << uint16_t(pipelines.size());
            for(const auto& pipe : pipelines) {
                wr << uint32_t(pipe.size());
                TOS::ByteBuffer pipeBuff(pipe.begin(), pipe.end());
                wr << pipeBuff;
            }

            TOS::ByteBuffer out = wr.complite();
            return std::vector<uint8_t>(out.begin(), out.end());
        } catch(const std::exception& exc) {
            LOG.warn() << "Ошибка ребинда заголовка модели: " << exc.what();
            return header;
        }
    }

    return header;
}

std::optional<AssetsManager::ParsedHeader> AssetsManager::parseHeader(AssetType type, const std::vector<uint8_t>& header) {
    if(header.empty())
        return std::nullopt;

    ParsedHeader result;
    result.Type = type;

    if(type == AssetType::Nodestate) {
        if(header.size() % sizeof(AssetId) != 0)
            return std::nullopt;
        const size_t count = header.size() / sizeof(AssetId);
        result.ModelDeps.resize(count);
        for(size_t i = 0; i < count; ++i) {
            AssetId raw = 0;
            std::memcpy(&raw, header.data() + i * sizeof(AssetId), sizeof(AssetId));
            result.ModelDeps[i] = raw;
        }
        return result;
    }

    if(type == AssetType::Model) {
        try {
            TOS::ByteBuffer buffer(header.size(), header.data());
            auto reader = buffer.reader();

            uint16_t modelCount = reader.readUInt16();
            result.ModelDeps.reserve(modelCount);
            for(uint16_t i = 0; i < modelCount; ++i)
                result.ModelDeps.push_back(reader.readUInt32());

            uint16_t texCount = reader.readUInt16();
            result.TexturePipelines.reserve(texCount);
            for(uint16_t i = 0; i < texCount; ++i) {
                uint32_t size32 = reader.readUInt32();
                TOS::ByteBuffer pipe;
                reader.readBuffer(pipe);
                if(pipe.size() != size32) {
                    return std::nullopt;
                }
                result.TexturePipelines.emplace_back(pipe.begin(), pipe.end());
            }

            std::unordered_set<AssetId> seen;
            for(const auto& pipe : result.TexturePipelines) {
                for(uint32_t id : collectTexturePipelineIds(pipe)) {
                    if(seen.insert(id).second)
                        result.TextureDeps.push_back(id);
                }
            }

            return result;
        } catch(const std::exception&) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

void AssetsManager::pushReads(std::vector<ResourceKey> reads) {
    std::vector<Hash_t> forCache;
    forCache.reserve(reads.size());

    for(ResourceKey& key : reads) {
        std::optional<PackResource> pack = findPackResource(key.Type, key.Domain, key.Key);
        if(pack && pack->Hash == key.Hash) {
            ReadyReads.emplace_back(std::move(key), pack->Res);
            continue;
        }

        auto& list = PendingReadsByHash[key.Hash];
        bool isFirst = list.empty();
        list.push_back(std::move(key));
        if(isFirst)
            forCache.push_back(list.front().Hash);
    }

    if(!forCache.empty())
        Cache->pushReads(std::move(forCache));
}

std::vector<std::pair<AssetsManager::ResourceKey, std::optional<Resource>>> AssetsManager::pullReads() {
    std::vector<std::pair<ResourceKey, std::optional<Resource>>> out;
    out.reserve(ReadyReads.size());

    for(auto& entry : ReadyReads)
        out.emplace_back(std::move(entry));
    ReadyReads.clear();

    std::vector<std::pair<Hash_t, std::optional<Resource>>> cached = Cache->pullReads();
    for(auto& [hash, res] : cached) {
        auto iter = PendingReadsByHash.find(hash);
        if(iter == PendingReadsByHash.end())
            continue;
        for(ResourceKey& key : iter->second)
            out.emplace_back(std::move(key), res);
        PendingReadsByHash.erase(iter);
    }

    return out;
}

AssetsManager::AssetId AssetsManager::getOrCreateLocalId(AssetType type, std::string_view domain, std::string_view key) {
    auto& table = DKToLocal[static_cast<size_t>(type)];
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

    auto& dk = LocalToDK[static_cast<size_t>(type)];
    if(id >= dk.size())
        dk.resize(id + 1);
    dk[id] = DomainKey{std::string(domain), std::string(key), true};

    return id;
}

AssetsManager::AssetId AssetsManager::getOrCreateLocalFromServer(AssetType type, AssetId serverId) {
    return ensureServerLocalId(type, serverId);
}

std::optional<AssetsManager::AssetId> AssetsManager::getLocalIdFromServer(AssetType type, AssetId serverId) const {
    const auto& map = ServerToLocal[static_cast<size_t>(type)];
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
    const auto& parents = LocalParent[static_cast<size_t>(type)];
    if(localId >= parents.size())
        return localId;
    AssetId cur = localId;
    while(cur < parents.size() && parents[cur] != cur && parents[cur] != 0)
        cur = parents[cur];
    return cur;
}

AssetsManager::AssetId AssetsManager::allocateLocalId(AssetType type) {
    auto& next = NextLocalId[static_cast<size_t>(type)];
    AssetId id = next++;

    auto& parents = LocalParent[static_cast<size_t>(type)];
    if(id >= parents.size())
        parents.resize(id + 1, 0);
    parents[id] = id;

    auto& dk = LocalToDK[static_cast<size_t>(type)];
    if(id >= dk.size())
        dk.resize(id + 1);

    return id;
}

AssetsManager::AssetId AssetsManager::ensureServerLocalId(AssetType type, AssetId serverId) {
    auto& map = ServerToLocal[static_cast<size_t>(type)];
    if(serverId >= map.size())
        map.resize(serverId + 1, 0);
    if(map[serverId] == 0)
        map[serverId] = allocateLocalId(type);
    return resolveLocalIdMutable(type, map[serverId]);
}

AssetsManager::AssetId AssetsManager::resolveLocalIdMutable(AssetType type, AssetId localId) {
    if(localId == 0)
        return 0;
    auto& parents = LocalParent[static_cast<size_t>(type)];
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

    auto& parents = LocalParent[static_cast<size_t>(type)];
    if(fromRoot >= parents.size() || toRoot >= parents.size())
        return;

    parents[fromRoot] = toRoot;
    if(reboundFrom)
        *reboundFrom = fromRoot;

    auto& dk = LocalToDK[static_cast<size_t>(type)];
    if(fromRoot < dk.size()) {
        const DomainKey& fromDK = dk[fromRoot];
        if(fromDK.Known) {
            if(toRoot >= dk.size())
                dk.resize(toRoot + 1);
            DomainKey& toDK = dk[toRoot];
            if(!toDK.Known) {
                toDK = fromDK;
                DKToLocal[static_cast<size_t>(type)][toDK.Domain][toDK.Key] = toRoot;
            } else if(toDK.Domain != fromDK.Domain || toDK.Key != fromDK.Key) {
                LOG.warn() << "Конфликт домен/ключ при ребинде: "
                    << fromDK.Domain << ':' << fromDK.Key << " vs "
                    << toDK.Domain << ':' << toDK.Key;
            }
        }
    }

    auto& binds = BindInfos[static_cast<size_t>(type)];
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
    const auto& typeTable = PackResources[static_cast<size_t>(type)];
    auto iterDomain = typeTable.find(domain);
    if(iterDomain == typeTable.end())
        return std::nullopt;
    auto iterKey = iterDomain->second.find(key);
    if(iterKey == iterDomain->second.end())
        return std::nullopt;
    return iterKey->second;
}

} // namespace LV::Client
