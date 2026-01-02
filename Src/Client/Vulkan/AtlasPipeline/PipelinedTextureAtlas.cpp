#include "PipelinedTextureAtlas.hpp"

PipelinedTextureAtlas::PipelinedTextureAtlas(TextureAtlas&& tk)
    : Super(std::move(tk)) {}

PipelinedTextureAtlas::AtlasTextureId PipelinedTextureAtlas::getByPipeline(const HashedPipeline& pipeline) {
    auto iter = _PipeToTexId.find(pipeline);
    if (iter == _PipeToTexId.end()) {
        AtlasTextureId atlasTexId = Super.registerTexture();
        _PipeToTexId.insert({pipeline, atlasTexId});
        _ChangedPipelines.push_back(pipeline);

        for (uint32_t texId : pipeline.getDependencedTextures()) {
            _AddictedTextures[texId].push_back(pipeline);
        }

        {
            std::vector<TexturePipelineProgram::AnimSpec> animMeta =
                TexturePipelineProgram::extractAnimationSpecs(pipeline._Pipeline.data(), pipeline._Pipeline.size());
            if (!animMeta.empty()) {
                AnimatedPipelineState entry;
                entry.Specs.reserve(animMeta.size());
                for (const auto& spec : animMeta) {
                    detail::AnimSpec16 outSpec{};
                    outSpec.TexId = spec.HasTexId ? spec.TexId : TextureAtlas::kOverflowId;
                    outSpec.FrameW = spec.FrameW;
                    outSpec.FrameH = spec.FrameH;
                    outSpec.FrameCount = spec.FrameCount;
                    outSpec.FpsQ = spec.FpsQ;
                    outSpec.Flags = spec.Flags;
                    entry.Specs.push_back(outSpec);
                }
                entry.LastFrames.resize(entry.Specs.size(), std::numeric_limits<uint32_t>::max());
                entry.Smooth = false;
                for (const auto& spec : entry.Specs) {
                    if (spec.Flags & detail::AnimSmooth) {
                        entry.Smooth = true;
                        break;
                    }
                }
                _AnimatedPipelines.emplace(pipeline, std::move(entry));
            }
        }

        return atlasTexId;
    }

    return iter->second;
}

void PipelinedTextureAtlas::freeByPipeline(const HashedPipeline& pipeline) {
    auto iter = _PipeToTexId.find(pipeline);
    if (iter == _PipeToTexId.end()) {
        return;
    }

    for (uint32_t texId : pipeline.getDependencedTextures()) {
        auto iterAT = _AddictedTextures.find(texId);
        assert(iterAT != _AddictedTextures.end());
        auto iterATSub = std::find(iterAT->second.begin(), iterAT->second.end(), pipeline);
        assert(iterATSub != iterAT->second.end());
        iterAT->second.erase(iterATSub);
    }

    Super.removeTexture(iter->second);
    _AtlasCpuTextures.erase(iter->second);
    _PipeToTexId.erase(iter);
    _AnimatedPipelines.erase(pipeline);
}

void PipelinedTextureAtlas::updateTexture(uint32_t texId, const StoredTexture& texture) {
    _ResToTexture[texId] = texture;
    _ChangedTextures.push_back(texId);
}

void PipelinedTextureAtlas::updateTexture(uint32_t texId, StoredTexture&& texture) {
    _ResToTexture[texId] = std::move(texture);
    _ChangedTextures.push_back(texId);
}

void PipelinedTextureAtlas::freeTexture(uint32_t texId) {
    auto iter = _ResToTexture.find(texId);
    if (iter != _ResToTexture.end()) {
        _ResToTexture.erase(iter);
    }
}

bool PipelinedTextureAtlas::getHostTexture(TextureId texId, HostTextureView& out) const {
    auto fill = [&](const StoredTexture& tex) -> bool {
        if (tex._Pixels.empty() || tex._Widht == 0 || tex._Height == 0) {
            return false;
        }
        out.width = tex._Widht;
        out.height = tex._Height;
        out.rowPitchBytes = static_cast<uint32_t>(tex._Widht) * 4u;
        out.pixelsRGBA8 = reinterpret_cast<const uint8_t*>(tex._Pixels.data());
        return true;
    };

    auto it = _ResToTexture.find(texId);
    if (it != _ResToTexture.end() && fill(it->second)) {
        return true;
    }

    auto itAtlas = _AtlasCpuTextures.find(texId);
    if (itAtlas != _AtlasCpuTextures.end() && fill(itAtlas->second)) {
        return true;
    }
    return false;
}

StoredTexture PipelinedTextureAtlas::_generatePipelineTexture(const HashedPipeline& pipeline) {
    std::vector<detail::Word> words(pipeline._Pipeline.begin(), pipeline._Pipeline.end());
    if (words.empty()) {
        if (auto tex = tryCopyFirstDependencyTexture(pipeline)) {
            return *tex;
        }
        return makeSolidColorTexture(0xFFFF00FFu);
    }

    TexturePipelineProgram program;
    program.fromBytes(std::move(words));

    TexturePipelineProgram::OwnedTexture baked;
    auto provider = [this](uint32_t texId) -> std::optional<Texture> {
        auto iter = _ResToTexture.find(texId);
        if (iter == _ResToTexture.end()) {
            return std::nullopt;
        }
        const StoredTexture& stored = iter->second;
        if (stored._Pixels.empty() || stored._Widht == 0 || stored._Height == 0) {
            return std::nullopt;
        }
        Texture tex{};
        tex.Width = stored._Widht;
        tex.Height = stored._Height;
        tex.Pixels = stored._Pixels.data();
        return tex;
    };

    if (!program.bake(provider, baked, _AnimTimeSeconds, nullptr)) {
        if (auto tex = tryCopyFirstDependencyTexture(pipeline)) {
            return *tex;
        }
        return makeSolidColorTexture(0xFFFF00FFu);
    }

    const uint32_t width = baked.Width;
    const uint32_t height = baked.Height;
    if (width == 0 || height == 0 ||
        width > std::numeric_limits<uint16_t>::max() ||
        height > std::numeric_limits<uint16_t>::max() ||
        baked.Pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        if (auto tex = tryCopyFirstDependencyTexture(pipeline)) {
            return *tex;
        }
        return makeSolidColorTexture(0xFFFF00FFu);
    }

    return StoredTexture(static_cast<uint16_t>(width),
                         static_cast<uint16_t>(height),
                         std::move(baked.Pixels));
}

void PipelinedTextureAtlas::flushNewPipelines() {
    std::vector<uint32_t> changedTextures = std::move(_ChangedTextures);
    _ChangedTextures.clear();

    std::sort(changedTextures.begin(), changedTextures.end());
    changedTextures.erase(std::unique(changedTextures.begin(), changedTextures.end()), changedTextures.end());

    std::vector<HashedPipeline> changedPipelineTextures;
    for (uint32_t texId : changedTextures) {
        auto iter = _AddictedTextures.find(texId);
        if (iter == _AddictedTextures.end()) {
            continue;
        }

        changedPipelineTextures.append_range(iter->second);
    }

    changedPipelineTextures.append_range(std::move(_ChangedPipelines));
    _ChangedPipelines.clear();
    changedTextures.clear();

    std::sort(changedPipelineTextures.begin(), changedPipelineTextures.end());
    changedPipelineTextures.erase(std::unique(changedPipelineTextures.begin(), changedPipelineTextures.end()),
                                  changedPipelineTextures.end());

    for (const HashedPipeline& pipeline : changedPipelineTextures) {
        auto iterPTTI = _PipeToTexId.find(pipeline);
        assert(iterPTTI != _PipeToTexId.end());

        StoredTexture texture = _generatePipelineTexture(pipeline);
        AtlasTextureId atlasTexId = iterPTTI->second;
        auto& stored = _AtlasCpuTextures[atlasTexId];
        stored = std::move(texture);
        if (!stored._Pixels.empty()) {
            // Смена порядка пикселей
            for (uint32_t& pixel : stored._Pixels) {
                union {
                    struct { uint8_t r, g, b, a; } color;
                    uint32_t data;
                };

                data = pixel;
                std::swap(color.r, color.b);
                pixel = data;
            }

            Super.setTextureData(atlasTexId,
                                 stored._Widht,
                                 stored._Height,
                                 stored._Pixels.data(),
                                 stored._Widht * 4u);
        }
    }
}

TextureAtlas::DescriptorOut PipelinedTextureAtlas::flushUploadsAndBarriers(VkCommandBuffer cmdBuffer) {
    return Super.flushUploadsAndBarriers(cmdBuffer);
}

void PipelinedTextureAtlas::notifyGpuFinished() {
    Super.notifyGpuFinished();
}

bool PipelinedTextureAtlas::updateAnimatedPipelines(double timeSeconds) {
    _AnimTimeSeconds = timeSeconds;
    if (_AnimatedPipelines.empty()) {
        return false;
    }

    bool changed = false;
    for (auto& [pipeline, entry] : _AnimatedPipelines) {
        if (entry.Specs.empty()) {
            continue;
        }

        if (entry.Smooth) {
            _ChangedPipelines.push_back(pipeline);
            changed = true;
            continue;
        }

        if (entry.LastFrames.size() != entry.Specs.size())
            entry.LastFrames.assign(entry.Specs.size(), std::numeric_limits<uint32_t>::max());

        bool pipelineChanged = false;
        for (size_t i = 0; i < entry.Specs.size(); ++i) {
            const auto& spec = entry.Specs[i];

            uint32_t fpsQ = spec.FpsQ ? spec.FpsQ : TexturePipelineProgram::DefaultAnimFpsQ;
            double fps = double(fpsQ) / 256.0;
            double frameTime = timeSeconds * fps;
            if (frameTime < 0.0)
                frameTime = 0.0;

            uint32_t frameCount = spec.FrameCount;
            // Авторасчёт количества кадров
            if (frameCount == 0) {
                auto iterTex = _ResToTexture.find(spec.TexId);
                if (iterTex != _ResToTexture.end()) {
                    uint32_t fw = spec.FrameW ? spec.FrameW : iterTex->second._Widht;
                    uint32_t fh = spec.FrameH ? spec.FrameH : iterTex->second._Widht;
                    if (fw > 0 && fh > 0) {
                        if (spec.Flags & detail::AnimHorizontal)
                            frameCount = iterTex->second._Widht / fw;
                        else
                            frameCount = iterTex->second._Height / fh;
                    }
                }
            }

            if (frameCount == 0)
                frameCount = 1;

            uint32_t frameIndex = frameCount ? (uint32_t(frameTime) % frameCount) : 0u;
            if (entry.LastFrames[i] != frameIndex) {
                entry.LastFrames[i] = frameIndex;
                pipelineChanged = true;
            }
        }

        if (pipelineChanged) {
            _ChangedPipelines.push_back(pipeline);
            changed = true;
        }
    }

    return changed;
}

std::optional<StoredTexture> PipelinedTextureAtlas::tryCopyFirstDependencyTexture(const HashedPipeline& pipeline) const {
    auto deps = pipeline.getDependencedTextures();
    if (!deps.empty()) {
        auto iter = _ResToTexture.find(deps.front());
        if (iter != _ResToTexture.end()) {
            return iter->second;
        }
    }
    return std::nullopt;
}

StoredTexture PipelinedTextureAtlas::makeSolidColorTexture(uint32_t rgba) {
    return StoredTexture(1, 1, std::vector<uint32_t>{rgba});
}
