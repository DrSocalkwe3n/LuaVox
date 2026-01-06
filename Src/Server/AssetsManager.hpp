#pragma once

#include "Common/Abstract.hpp"
#include "Common/IdProvider.hpp"
#include "Common/AssetsPreloader.hpp"
#include <unordered_map>

namespace LV::Server {

class AssetsManager : public IdProvider<EnumAssets>, protected AssetsPreloader {
public:
    using BindHashHeaderInfo = AssetsManager::BindHashHeaderInfo;

    struct Out_checkAndPrepareResourcesUpdate : public AssetsPreloader::Out_checkAndPrepareResourcesUpdate {
        Out_checkAndPrepareResourcesUpdate(AssetsPreloader::Out_checkAndPrepareResourcesUpdate&& obj)
        : AssetsPreloader::Out_checkAndPrepareResourcesUpdate(std::move(obj))
        {}

        std::unordered_map<ResourceFile::Hash_t, std::u8string> NewHeadless;
    };

    Out_checkAndPrepareResourcesUpdate checkAndPrepareResourcesUpdate(
        const AssetsRegister& instances,
        ReloadStatus* status = nullptr
    ) {
        std::unordered_map<ResourceFile::Hash_t, std::u8string> newHeadless;

        Out_checkAndPrepareResourcesUpdate result = AssetsPreloader::checkAndPrepareResourcesUpdate(
            instances,
            [&](EnumAssets type, std::string_view domain, std::string_view key) { return getId(type, domain, key); },
            [&](std::u8string&& resource, ResourceFile::Hash_t hash, fs::path resPath) { newHeadless.emplace(hash, std::move(resource)); },
            status
        );

        result.NewHeadless = std::move(newHeadless);

        return result;
    }

    struct Out_applyResourcesUpdate : public AssetsPreloader::Out_applyResourcesUpdate {
        Out_applyResourcesUpdate(AssetsPreloader::Out_applyResourcesUpdate&& obj)
        : AssetsPreloader::Out_applyResourcesUpdate(std::move(obj))
        {}
    };

    Out_applyResourcesUpdate applyResourcesUpdate(Out_checkAndPrepareResourcesUpdate& orr) {
        Out_applyResourcesUpdate result = AssetsPreloader::applyResourcesUpdate(orr);

        for(auto& [hash, data] : orr.NewHeadless) {
            Resources.emplace(hash, ResourceHashData{0, std::make_shared<std::u8string>(std::move(data))});
        }

        for(auto& [hash, pathes] : orr.HashToPathNew) {
            auto iter = Resources.find(hash);
            assert(iter != Resources.end());
            iter->second.RefCount += pathes.size();
        }

        for(auto& [hash, pathes] : orr.HashToPathLost) {
            auto iter = Resources.find(hash);
            assert(iter != Resources.end());
            iter->second.RefCount -= pathes.size();

            if(iter->second.RefCount == 0)
                Resources.erase(iter);
        }

        return result;
    }

    std::vector<std::tuple<ResourceFile::Hash_t, std::shared_ptr<const std::u8string>>>
        getResources(const std::vector<ResourceFile::Hash_t>& hashes) const 
    {
        std::vector<std::tuple<ResourceFile::Hash_t, std::shared_ptr<const std::u8string>>> result;
        result.reserve(hashes.size());

        for(const auto& hash : hashes) {
            auto iter = Resources.find(hash);
            if(iter == Resources.end())
                continue;

            result.emplace_back(hash, iter->second.Data);
        }

        return result;
    }

    std::array< 
        std::vector<BindHashHeaderInfo>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    > collectHashBindings() const {
        return AssetsPreloader::collectHashBindings();
    }

private:
    struct ResourceHashData {
        size_t RefCount;
        std::shared_ptr<std::u8string> Data;
    };

    std::unordered_map<
        ResourceFile::Hash_t,
        ResourceHashData
    > Resources;
};

}