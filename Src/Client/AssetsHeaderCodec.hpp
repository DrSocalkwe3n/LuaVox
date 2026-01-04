#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Abstract.hpp"

namespace LV::Client::AssetsHeaderCodec {

struct ParsedHeader {
    EnumAssets Type{};
    std::vector<ResourceId> ModelDeps;
    std::vector<ResourceId> TextureDeps;
    std::vector<std::vector<uint8_t>> TexturePipelines;
};

using MapIdFn = std::function<ResourceId(ResourceId)>;
using WarnFn = std::function<void(const std::string&)>;

std::optional<ParsedHeader> parseHeader(EnumAssets type, const std::vector<uint8_t>& header);

std::vector<uint8_t> rebindHeader(EnumAssets type, const std::vector<uint8_t>& header,
    const MapIdFn& mapModelId, const MapIdFn& mapTextureId, const WarnFn& warn);

} // namespace LV::Client::AssetsHeaderCodec
