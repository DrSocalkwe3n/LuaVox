#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Client/AssetsCacheManager.hpp"
#include "Common/Abstract.hpp"
#include "TOSLib.hpp"

namespace LV::Client {

namespace fs = std::filesystem;

class AssetsManager {
public:
    using Ptr = std::shared_ptr<AssetsManager>;
    using AssetType = EnumAssets;
    using AssetId = ResourceId;

    struct ResourceKey {
        Hash_t Hash{};
        AssetType Type{};
        std::string Domain;
        std::string Key;
        AssetId Id = 0;
    };

    struct BindInfo {
        AssetType Type{};
        AssetId LocalId = 0;
        std::string Domain;
        std::string Key;
        Hash_t Hash{};
        std::vector<uint8_t> Header;
    };

    struct BindResult {
        AssetId LocalId = 0;
        bool Changed = false;
        bool NewBinding = false;
        std::optional<AssetId> ReboundFrom;
    };

    struct PackRegister {
        std::vector<fs::path> Packs;
    };

    struct PackResource {
        AssetType Type{};
        AssetId LocalId = 0;
        std::string Domain;
        std::string Key;
        Resource Res;
        Hash_t Hash{};
        std::u8string Header;
    };

    struct PackReloadResult {
        std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> ChangeOrAdd;
        std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> Lost;
    };

    struct ParsedHeader {
        AssetType Type{};
        std::vector<AssetId> ModelDeps;
        std::vector<AssetId> TextureDeps;
        std::vector<std::vector<uint8_t>> TexturePipelines;
    };

    static Ptr Create(asio::io_context& ioc, const fs::path& cachePath,
        size_t maxCacheDirectorySize = 8 * 1024 * 1024 * 1024ULL,
        size_t maxLifeTime = 7 * 24 * 60 * 60) {
        return Ptr(new AssetsManager(ioc, cachePath, maxCacheDirectorySize, maxLifeTime));
    }

    PackReloadResult reloadPacks(const PackRegister& reg);

    BindResult bindServerResource(AssetType type, AssetId serverId, std::string domain, std::string key,
        const Hash_t& hash, std::vector<uint8_t> header);
    std::optional<AssetId> unbindServerResource(AssetType type, AssetId serverId);
    void clearServerBindings();

    const BindInfo* getBind(AssetType type, AssetId localId) const;

    std::vector<uint8_t> rebindHeader(AssetType type, const std::vector<uint8_t>& header, bool serverIds = true);
    static std::optional<ParsedHeader> parseHeader(AssetType type, const std::vector<uint8_t>& header);

    void pushResources(std::vector<Resource> resources) {
        Cache->pushResources(std::move(resources));
    }

    void pushReads(std::vector<ResourceKey> reads);
    std::vector<std::pair<ResourceKey, std::optional<Resource>>> pullReads();

    AssetId getOrCreateLocalId(AssetType type, std::string_view domain, std::string_view key);
    std::optional<AssetId> getLocalIdFromServer(AssetType type, AssetId serverId) const;

private:
    struct DomainKey {
        std::string Domain;
        std::string Key;
        bool Known = false;
    };

    using IdTable = std::unordered_map<
        std::string,
        std::unordered_map<std::string, AssetId, detail::TSVHash, detail::TSVEq>,
        detail::TSVHash,
        detail::TSVEq>;

    using PackTable = std::unordered_map<
        std::string,
        std::unordered_map<std::string, PackResource, detail::TSVHash, detail::TSVEq>,
        detail::TSVHash,
        detail::TSVEq>;

    AssetsManager(asio::io_context& ioc, const fs::path& cachePath,
        size_t maxCacheDirectorySize, size_t maxLifeTime);

    AssetId allocateLocalId(AssetType type);
    AssetId resolveLocalIdMutable(AssetType type, AssetId localId);
    AssetId resolveLocalId(AssetType type, AssetId localId) const;
    void unionLocalIds(AssetType type, AssetId fromId, AssetId toId, std::optional<AssetId>* reboundFrom);

    std::optional<PackResource> findPackResource(AssetType type, std::string_view domain, std::string_view key) const;

    Logger LOG = "Client>AssetsManager";
    AssetsCacheManager::Ptr Cache;

    std::array<IdTable, static_cast<size_t>(AssetType::MAX_ENUM)> DKToLocal;
    std::array<std::vector<DomainKey>, static_cast<size_t>(AssetType::MAX_ENUM)> LocalToDK;
    std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> LocalParent;
    std::array<std::vector<AssetId>, static_cast<size_t>(AssetType::MAX_ENUM)> ServerToLocal;
    std::array<std::vector<std::optional<BindInfo>>, static_cast<size_t>(AssetType::MAX_ENUM)> BindInfos;
    std::array<PackTable, static_cast<size_t>(AssetType::MAX_ENUM)> PackResources;
    std::array<AssetId, static_cast<size_t>(AssetType::MAX_ENUM)> NextLocalId{};

    std::unordered_map<Hash_t, std::vector<ResourceKey>> PendingReadsByHash;
    std::vector<std::pair<ResourceKey, std::optional<Resource>>> ReadyReads;
};

} // namespace LV::Client
