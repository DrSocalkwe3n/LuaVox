#pragma once

#include "TextureAtlas.hpp"
#include "TexturePipelineProgram.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include "boost/container/small_vector.hpp"

using TextureId = uint32_t;

namespace detail {

using Word = TexturePipelineProgram::Word;

enum class Op16 : Word {
    End        = 0,
    Base_Tex   = 1,
    Base_Fill  = 2,
    Resize     = 10,
    Transform  = 11,
    Opacity    = 12,
    NoAlpha    = 13,
    MakeAlpha  = 14,
    Invert     = 15,
    Brighten   = 16,
    Contrast   = 17,
    Multiply   = 18,
    Screen     = 19,
    Colorize   = 20,
    Overlay    = 30,
    Mask       = 31,
    LowPart    = 32,
    Combine    = 40
};

enum class SrcKind16 : Word { TexId = 0, Sub = 1 };

struct SrcRef16 {
    SrcKind16 kind{SrcKind16::TexId};
    Word a = 0;
    Word b = 0;
};

inline uint32_t makeU32(Word lo, Word hi) {
    return uint32_t(lo) | (uint32_t(hi) << 16);
}

inline void addUniqueDep(boost::container::small_vector<uint32_t, 8>& deps, uint32_t id) {
    if (id == TextureAtlas::kOverflowId) {
        return;
    }
    if (std::find(deps.begin(), deps.end(), id) == deps.end()) {
        deps.push_back(id);
    }
}

inline bool readSrc(const std::vector<Word>& words, size_t end, size_t& ip, SrcRef16& out) {
    if (ip + 2 >= end) {
        return false;
    }
    out.kind = static_cast<SrcKind16>(words[ip++]);
    out.a = words[ip++];
    out.b = words[ip++];
    return true;
}

inline void extractPipelineDependencies(const std::vector<Word>& words,
                                        size_t start,
                                        size_t end,
                                        boost::container::small_vector<uint32_t, 8>& deps,
                                        std::vector<std::pair<size_t, size_t>>& visited) {
    if (start >= end || end > words.size()) {
        return;
    }
    const std::pair<size_t, size_t> key{start, end};
    if (std::find(visited.begin(), visited.end(), key) != visited.end()) {
        return;
    }
    visited.push_back(key);

    size_t ip = start;
    auto need = [&](size_t n) { return ip + n <= end; };
    auto handleSrc = [&](const SrcRef16& src) {
        if (src.kind == SrcKind16::TexId) {
            addUniqueDep(deps, makeU32(src.a, src.b));
            return;
        }
        if (src.kind == SrcKind16::Sub) {
            size_t subStart = static_cast<size_t>(src.a);
            size_t subEnd = subStart + static_cast<size_t>(src.b);
            if (subStart < subEnd && subEnd <= words.size()) {
                extractPipelineDependencies(words, subStart, subEnd, deps, visited);
            }
        }
    };

    while (ip < end) {
        if (!need(1)) break;
        Op16 op = static_cast<Op16>(words[ip++]);
        switch (op) {
            case Op16::End:
                return;

            case Op16::Base_Tex: {
                if (!need(3)) return;
                SrcRef16 src{};
                if (!readSrc(words, end, ip, src)) return;
                handleSrc(src);
            } break;

            case Op16::Base_Fill:
                if (!need(4)) return;
                ip += 4;
                break;

            case Op16::Overlay:
            case Op16::Mask: {
                if (!need(3)) return;
                SrcRef16 src{};
                if (!readSrc(words, end, ip, src)) return;
                handleSrc(src);
            } break;

            case Op16::LowPart: {
                if (!need(1 + 3)) return;
                ip += 1; // percent
                SrcRef16 src{};
                if (!readSrc(words, end, ip, src)) return;
                handleSrc(src);
            } break;

            case Op16::Resize:
                if (!need(2)) return;
                ip += 2;
                break;

            case Op16::Transform:
            case Op16::Opacity:
                if (!need(1)) return;
                ip += 1;
                break;

            case Op16::NoAlpha:
            case Op16::Brighten:
                break;

            case Op16::MakeAlpha:
                if (!need(2)) return;
                ip += 2;
                break;

            case Op16::Invert:
                if (!need(1)) return;
                ip += 1;
                break;

            case Op16::Contrast:
                if (!need(2)) return;
                ip += 2;
                break;

            case Op16::Multiply:
            case Op16::Screen:
                if (!need(2)) return;
                ip += 2;
                break;

            case Op16::Colorize:
                if (!need(3)) return;
                ip += 3;
                break;

            case Op16::Combine: {
                if (!need(3)) return;
                ip += 2; // skip w,h
                uint32_t n = words[ip++];
                for (uint32_t i = 0; i < n; ++i) {
                    if (!need(2 + 3)) return;
                    ip += 2; // x, y
                    SrcRef16 src{};
                    if (!readSrc(words, end, ip, src)) return;
                    handleSrc(src);
                }
            } break;

            default:
                return;
        }
    }
}

inline boost::container::small_vector<uint32_t, 8> extractPipelineDependencies(const std::vector<Word>& words) {
    boost::container::small_vector<uint32_t, 8> deps;
    std::vector<std::pair<size_t, size_t>> visited;
    extractPipelineDependencies(words, 0, words.size(), deps, visited);
    return deps;
}

inline boost::container::small_vector<uint32_t, 8> extractPipelineDependencies(const boost::container::small_vector<Word, 32>& words) {
    boost::container::small_vector<uint32_t, 8> deps;
    std::vector<std::pair<size_t, size_t>> visited;
    std::vector<Word> copy(words.begin(), words.end());
    extractPipelineDependencies(copy, 0, copy.size(), deps, visited);
    return deps;
}

} // namespace detail

// Структура нехешированного пайплайна
struct Pipeline {
    std::vector<detail::Word> _Pipeline;

    Pipeline() = default;

    explicit Pipeline(const TexturePipelineProgram& program)
        : _Pipeline(program.words().begin(), program.words().end())
    {
    }

    Pipeline(TextureId texId) {
        _Pipeline = {
            static_cast<detail::Word>(detail::Op16::Base_Tex),
            static_cast<detail::Word>(detail::SrcKind16::TexId),
            static_cast<detail::Word>(texId & 0xFFFFu),
            static_cast<detail::Word>((texId >> 16) & 0xFFFFu),
            static_cast<detail::Word>(detail::Op16::End)
        };
    }
};

// Структура хешированного текстурного пайплайна
struct HashedPipeline {
    // Предвычисленный хеш
    std::size_t _Hash;
    boost::container::small_vector<detail::Word, 32> _Pipeline;

    HashedPipeline() = default;
    HashedPipeline(const Pipeline& pipeline) noexcept
        : _Pipeline(pipeline._Pipeline.begin(), pipeline._Pipeline.end())
    {
        reComputeHash();
    }

    // Перевычисляет хеш
    void reComputeHash() noexcept {
        std::size_t hash = 14695981039346656037ull;
        constexpr std::size_t prime = 1099511628211ull;

        for(detail::Word w : _Pipeline) {
            hash ^= static_cast<uint8_t>(w & 0xFF);
            hash *= prime;
            hash ^= static_cast<uint8_t>((w >> 8) & 0xFF);
            hash *= prime;
        }

        _Hash = hash;
    }

    // Выдаёт список зависимых текстур, на основе которых строится эта
    boost::container::small_vector<uint32_t, 8> getDependencedTextures() const {
        return detail::extractPipelineDependencies(_Pipeline);
    }

    bool operator==(const HashedPipeline& obj) const noexcept {
        return _Hash == obj._Hash && _Pipeline == obj._Pipeline;
    }

    bool operator<(const HashedPipeline& obj) const noexcept {
        return _Hash < obj._Hash || (_Hash == obj._Hash && _Pipeline < obj._Pipeline);
    }
};

struct StoredTexture {
    uint16_t _Widht = 0;
    uint16_t _Height = 0;
    std::vector<uint32_t> _Pixels;

    StoredTexture() = default;
    StoredTexture(uint16_t w, uint16_t h, std::vector<uint32_t> pixels)
        : _Widht(w), _Height(h), _Pixels(std::move(pixels))
    {
    }
};


// Пайплайновый текстурный атлас
class PipelinedTextureAtlas {
public:
    using AtlasTextureId = uint32_t;
    struct HostTextureView {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t rowPitchBytes = 0;
        const uint8_t* pixelsRGBA8 = nullptr;
    };

private:
    // Функтор хеша
    struct HashedPipelineKeyHash {
        std::size_t operator()(const HashedPipeline& k) const noexcept {
            return k._Hash;
        }
    };

    // Функтор равенства
    struct HashedPipelineKeyEqual {
        bool operator()(const HashedPipeline& a, const HashedPipeline& b) const noexcept {
            return a._Pipeline == b._Pipeline;
        }
    };

    // Текстурный атлас
    TextureAtlas Super;
    // Пустой пайплайн (указывающий на одну текстуру) ссылается на простой идентификатор (ResToAtlas)
    std::unordered_map<HashedPipeline, AtlasTextureId, HashedPipelineKeyHash, HashedPipelineKeyEqual> _PipeToTexId;
    // Загруженные текстуры
    std::unordered_map<TextureId, StoredTexture> _ResToTexture;
    std::unordered_map<AtlasTextureId, StoredTexture> _AtlasCpuTextures;
    // Список зависимых пайплайнов от текстур (при изменении текстуры, нужно перерисовать пайплайны)
    std::unordered_map<TextureId, boost::container::small_vector<HashedPipeline, 8>> _AddictedTextures;
    // Изменённые простые текстуры (для последующего массового обновление пайплайнов)
    std::vector<uint32_t> _ChangedTextures;
    // Необходимые к созданию/обновлению пайплайны
    std::vector<HashedPipeline> _ChangedPipelines;

public:
    PipelinedTextureAtlas(TextureAtlas&& tk);

    uint32_t atlasSide() const {
        return Super.atlasSide();
    }

    uint32_t atlasLayers() const {
        return Super.atlasLayers();
    }

    uint32_t AtlasSide() const {
        return atlasSide();
    }

    uint32_t AtlasLayers() const {
        return atlasLayers();
    }

    // Должны всегда бронировать идентификатор, либо отдавать kOverflowId. При этом запись tex+pipeline остаётся
    // Выдаёт стабильный идентификатор, привязанный к пайплайну
    AtlasTextureId getByPipeline(const HashedPipeline& pipeline);

    // Уведомить что текстура+pipeline более не используются (идентификатор будет освобождён)
    // Освобождать можно при потере ресурсов
    void freeByPipeline(const HashedPipeline& pipeline);

    void updateTexture(uint32_t texId, const StoredTexture& texture);
    void updateTexture(uint32_t texId, StoredTexture&& texture);

    void freeTexture(uint32_t texId);

    bool getHostTexture(TextureId texId, HostTextureView& out) const;

    // Генерация текстуры пайплайна
    StoredTexture _generatePipelineTexture(const HashedPipeline& pipeline);

    // Обновляет пайплайны по необходимости
    void flushNewPipelines();

    TextureAtlas::DescriptorOut flushUploadsAndBarriers(VkCommandBuffer cmdBuffer);

    void notifyGpuFinished();

private:
    std::optional<StoredTexture> tryCopyFirstDependencyTexture(const HashedPipeline& pipeline) const;

    static StoredTexture makeSolidColorTexture(uint32_t rgba);
};
