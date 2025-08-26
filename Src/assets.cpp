#include "assets.hpp"
#include <TOSLib.hpp>
#include <boost/smart_ptr/scoped_array.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

extern std::unordered_map<std::string, std::tuple<const char*, const char*>> _binary_assets_symbols;
namespace fs = std::filesystem;

namespace LV {

iResource::iResource() = default;
iResource::~iResource() = default;

static std::mutex iResourceCacheMtx;
static std::unordered_map<std::string, std::weak_ptr<iResource>> iResourceCache;

class FS_iResource : public iResource {
    boost::scoped_array<uint8_t> Array;

public:
    FS_iResource(const std::filesystem::path &path)
    {
        std::ifstream fd(path);

        if(!fd)
            MAKE_ERROR("Ошибка чтения ресурса: " << path);

        fd.seekg(0, std::ios::end);
        Size = fd.tellg();
        fd.seekg(0, std::ios::beg);
        Array.reset(new uint8_t[Size]);
        fd.read((char*) Array.get(), Size);
        Data = Array.get();
    }

    virtual ~FS_iResource() = default;
};

std::shared_ptr<iResource> getResource(const std::string &path) {
    std::unique_lock<std::mutex> lock(iResourceCacheMtx);

    if(auto iter = iResourceCache.find(path); iter != iResourceCache.end()) {
        std::shared_ptr<iResource> iResource = iter->second.lock();
        if(!iResource) {
            iResourceCache.erase(iter);
        } else {
            return iResource;
        }
    }

    fs::path fs_path("assets");
    fs_path /= path;

    if(fs::exists(fs_path)) {
        std::shared_ptr<iResource> iResource = std::make_shared<FS_iResource>(fs_path);
        iResourceCache.emplace(path, iResource);
        TOS::Logger("iResources").debug() << "Ресурс " << fs_path << " найден в фс";
        return iResource;
    }

    if(auto iter = _binary_assets_symbols.find(path); iter != _binary_assets_symbols.end()) {
        TOS::Logger("iResources").debug() << "Ресурс " << fs_path << " is inlined";
        return std::make_shared<iResource>((const uint8_t*) std::get<0>(iter->second), std::get<1>(iter->second)-std::get<0>(iter->second));
    }
    
    MAKE_ERROR("Ресурс " << path << " не найден");
}

}