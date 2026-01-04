#include "Client/AssetsHeaderCodec.hpp"
#include <cstring>
#include <unordered_set>
#include "TOSLib.hpp"

namespace LV::Client::AssetsHeaderCodec {

namespace {

struct ParsedModelHeader {
    std::vector<ResourceId> ModelDeps;
    std::vector<std::vector<uint8_t>> TexturePipelines;
    std::vector<ResourceId> TextureDeps;
};

std::optional<std::vector<ResourceId>> parseNodestateHeaderBytes(const std::vector<uint8_t>& header) {
    if(header.empty() || header.size() % sizeof(ResourceId) != 0)
        return std::nullopt;

    const size_t count = header.size() / sizeof(ResourceId);
    std::vector<ResourceId> deps;
    deps.resize(count);
    for(size_t i = 0; i < count; ++i) {
        ResourceId raw = 0;
        std::memcpy(&raw, header.data() + i * sizeof(ResourceId), sizeof(ResourceId));
        deps[i] = raw;
    }
    return deps;
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

    auto read8 = [&](size_t& ip, uint8_t& out)->bool{
        if(ip >= size)
            return false;
        out = code[ip++];
        return true;
    };
    auto read16 = [&](size_t& ip, uint16_t& out)->bool{
        if(ip + 1 >= size)
            return false;
        out = uint16_t(code[ip]) | (uint16_t(code[ip + 1]) << 8);
        ip += 2;
        return true;
    };
    auto read24 = [&](size_t& ip, uint32_t& out)->bool{
        if(ip + 2 >= size)
            return false;
        out = uint32_t(code[ip])
            | (uint32_t(code[ip + 1]) << 8)
            | (uint32_t(code[ip + 2]) << 16);
        ip += 3;
        return true;
    };
    auto read32 = [&](size_t& ip, uint32_t& out)->bool{
        if(ip + 3 >= size)
            return false;
        out = uint32_t(code[ip])
            | (uint32_t(code[ip + 1]) << 8)
            | (uint32_t(code[ip + 2]) << 16)
            | (uint32_t(code[ip + 3]) << 24);
        ip += 4;
        return true;
    };

    auto readSrc = [&](size_t& ip, SrcMeta& out)->bool{
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

    auto patchTexId = [&](const SrcMeta& src)->PipelineRemapResult{
        if(src.Kind != SrcKind::TexId)
            return {};
        uint32_t newId = mapId(src.TexId);
        if(newId >= (1u << 24))
            return {false, "TexId exceeds u24 range"};
        if(src.TexIdOffset + 2 >= code.size())
            return {false, "TexId patch outside pipeline"};
        code[src.TexIdOffset + 0] = uint8_t(newId & 0xFFu);
        code[src.TexIdOffset + 1] = uint8_t((newId >> 8) & 0xFFu);
        code[src.TexIdOffset + 2] = uint8_t((newId >> 16) & 0xFFu);
        return {};
    };

    std::function<bool(size_t, size_t)> scan;
    scan = [&](size_t start, size_t end) -> bool {
        if(start >= end || end > size)
            return true;
        for(const auto& range : visited) {
            if(range.Start == start && range.End == end)
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
                PipelineRemapResult r = patchTexId(src);
                if(!r.Ok)
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
                if(!read16(ip, tmp16)) return false;
                if(!read16(ip, tmp16)) return false;
                if(!read32(ip, tmp32)) return false;
            } break;

            case Op::Base_Anim: {
                SrcMeta src{};
                if(!readSrc(ip, src)) return false;
                PipelineRemapResult r = patchTexId(src);
                if(!r.Ok) return false;
                uint16_t frameW = 0;
                uint16_t frameH = 0;
                uint16_t frameCount = 0;
                uint16_t fpsQ = 0;
                uint8_t flags = 0;
                if(!read16(ip, frameW)) return false;
                if(!read16(ip, frameH)) return false;
                if(!read16(ip, frameCount)) return false;
                if(!read16(ip, fpsQ)) return false;
                if(!read8(ip, flags)) return false;
                (void)frameW; (void)frameH; (void)frameCount; (void)fpsQ; (void)flags;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd)) return false;
                }
            } break;

            case Op::Resize: {
                uint16_t tmp16 = 0;
                if(!read16(ip, tmp16)) return false;
                if(!read16(ip, tmp16)) return false;
            } break;

            case Op::Transform:
            case Op::Opacity:
            case Op::Invert:
                if(!read8(ip, opByte)) return false;
                break;

            case Op::NoAlpha:
            case Op::Brighten:
                break;

            case Op::MakeAlpha:
                if(ip + 2 >= size) return false;
                ip += 3;
                break;

            case Op::Contrast:
                if(ip + 1 >= size) return false;
                ip += 2;
                break;

            case Op::Multiply:
            case Op::Screen: {
                uint32_t tmp32 = 0;
                if(!read32(ip, tmp32)) return false;
            } break;

            case Op::Colorize: {
                uint32_t tmp32 = 0;
                if(!read32(ip, tmp32)) return false;
                if(!read8(ip, opByte)) return false;
            } break;

            case Op::Anim: {
                uint16_t frameW = 0;
                uint16_t frameH = 0;
                uint16_t frameCount = 0;
                uint16_t fpsQ = 0;
                uint8_t flags = 0;
                if(!read16(ip, frameW)) return false;
                if(!read16(ip, frameH)) return false;
                if(!read16(ip, frameCount)) return false;
                if(!read16(ip, fpsQ)) return false;
                if(!read8(ip, flags)) return false;
                (void)frameW; (void)frameH; (void)frameCount; (void)fpsQ; (void)flags;
            } break;

            case Op::Overlay:
            case Op::Mask: {
                SrcMeta src{};
                if(!readSrc(ip, src)) return false;
                PipelineRemapResult r = patchTexId(src);
                if(!r.Ok) return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd)) return false;
                }
            } break;

            case Op::LowPart: {
                if(!read8(ip, opByte)) return false;
                SrcMeta src{};
                if(!readSrc(ip, src)) return false;
                PipelineRemapResult r = patchTexId(src);
                if(!r.Ok) return false;
                if(src.Kind == SrcKind::Sub) {
                    size_t subStart = src.Off;
                    size_t subEnd = subStart + src.Len;
                    if(!scan(subStart, subEnd)) return false;
                }
            } break;

            case Op::Combine: {
                uint16_t w = 0, h = 0, n = 0;
                if(!read16(ip, w)) return false;
                if(!read16(ip, h)) return false;
                if(!read16(ip, n)) return false;
                for(uint16_t i = 0; i < n; ++i) {
                    uint16_t tmp16 = 0;
                    if(!read16(ip, tmp16)) return false;
                    if(!read16(ip, tmp16)) return false;
                    SrcMeta src{};
                    if(!readSrc(ip, src)) return false;
                    PipelineRemapResult r = patchTexId(src);
                    if(!r.Ok) return false;
                    if(src.Kind == SrcKind::Sub) {
                        size_t subStart = src.Off;
                        size_t subEnd = subStart + src.Len;
                        if(!scan(subStart, subEnd)) return false;
                    }
                }
                (void)w; (void)h;
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

std::vector<uint32_t> collectTexturePipelineIds(const std::vector<uint8_t>& code) {
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

std::optional<ParsedModelHeader> parseModelHeaderBytes(const std::vector<uint8_t>& header) {
    if(header.empty())
        return std::nullopt;

    ParsedModelHeader result;
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
            if(pipe.size() != size32)
                return std::nullopt;
            result.TexturePipelines.emplace_back(pipe.begin(), pipe.end());
        }

        std::unordered_set<ResourceId> seen;
        for(const auto& pipe : result.TexturePipelines) {
            for(uint32_t id : collectTexturePipelineIds(pipe)) {
                if(seen.insert(id).second)
                    result.TextureDeps.push_back(id);
            }
        }
    } catch(const std::exception&) {
        return std::nullopt;
    }

    return result;
}

} // namespace

std::optional<ParsedHeader> parseHeader(EnumAssets type, const std::vector<uint8_t>& header) {
    if(header.empty())
        return std::nullopt;

    ParsedHeader result;
    result.Type = type;

    if(type == EnumAssets::Nodestate) {
        auto deps = parseNodestateHeaderBytes(header);
        if(!deps)
            return std::nullopt;
        result.ModelDeps = std::move(*deps);
        return result;
    }

    if(type == EnumAssets::Model) {
        auto parsed = parseModelHeaderBytes(header);
        if(!parsed)
            return std::nullopt;
        result.ModelDeps = std::move(parsed->ModelDeps);
        result.TexturePipelines = std::move(parsed->TexturePipelines);
        result.TextureDeps = std::move(parsed->TextureDeps);
        return result;
    }

    return std::nullopt;
}

std::vector<uint8_t> rebindHeader(EnumAssets type, const std::vector<uint8_t>& header,
    const MapIdFn& mapModelId, const MapIdFn& mapTextureId, const WarnFn& warn)
{
    if(header.empty())
        return {};

    if(type == EnumAssets::Nodestate) {
        if(header.size() % sizeof(ResourceId) != 0)
            return header;
        std::vector<uint8_t> out(header.size());
        const size_t count = header.size() / sizeof(ResourceId);
        for(size_t i = 0; i < count; ++i) {
            ResourceId raw = 0;
            std::memcpy(&raw, header.data() + i * sizeof(ResourceId), sizeof(ResourceId));
            ResourceId mapped = mapModelId(raw);
            std::memcpy(out.data() + i * sizeof(ResourceId), &mapped, sizeof(ResourceId));
        }
        return out;
    }

    if(type == EnumAssets::Model) {
        try {
            TOS::ByteBuffer buffer(header.size(), header.data());
            auto reader = buffer.reader();

            uint16_t modelCount = reader.readUInt16();
            std::vector<ResourceId> models;
            models.reserve(modelCount);
            for(uint16_t i = 0; i < modelCount; ++i) {
                ResourceId id = reader.readUInt32();
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
                    warn("Pipeline size mismatch");
                }
                std::vector<uint8_t> code(pipe.begin(), pipe.end());
                auto result = remapTexturePipelineIds(code, [&](uint32_t id) {
                    return mapTextureId(static_cast<ResourceId>(id));
                });
                if(!result.Ok) {
                    warn(result.Error);
                }
                pipelines.emplace_back(std::move(code));
            }

            TOS::ByteBuffer::Writer wr;
            wr << uint16_t(models.size());
            for(ResourceId id : models)
                wr << id;
            wr << uint16_t(pipelines.size());
            for(const auto& pipe : pipelines) {
                wr << uint32_t(pipe.size());
                TOS::ByteBuffer pipeBuff(pipe.begin(), pipe.end());
                wr << pipeBuff;
            }

            TOS::ByteBuffer out = wr.complite();
            return std::vector<uint8_t>(out.begin(), out.end());
        } catch(const std::exception&) {
            warn("Failed to rebind model header");
            return header;
        }
    }

    return header;
}

} // namespace LV::Client::AssetsHeaderCodec
