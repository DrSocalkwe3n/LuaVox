#include "assets.hpp"
#include <TOSLib.hpp>
#include <boost/smart_ptr/scoped_array.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

extern std::unordered_map<std::string, std::tuple<const char*, const char*>> _binary_assets_symbols;
namespace fs = std::filesystem;

namespace AL {

Resource::Resource() = default;
Resource::~Resource() = default;

static std::mutex ResourceCacheMtx;
static std::unordered_map<std::string, std::weak_ptr<Resource>> ResourceCache;

class FS_Resource : public Resource {
    boost::scoped_array<uint8_t> Array;

public:
    FS_Resource(const std::filesystem::path &path)
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

    virtual ~FS_Resource() = default;
};

std::shared_ptr<Resource> getResource(const std::string &path) {
    std::unique_lock<std::mutex> lock(ResourceCacheMtx);

    if(auto iter = ResourceCache.find(path); iter != ResourceCache.end()) {
        std::shared_ptr<Resource> resource = iter->second.lock();
        if(!resource) {
            ResourceCache.erase(iter);
        } else {
            return resource;
        }
    }

    fs::path fs_path("assets");
    fs_path /= path;

    if(fs::exists(fs_path)) {
        std::shared_ptr<Resource> resource = std::make_shared<FS_Resource>(fs_path);
        ResourceCache.emplace(path, resource);
        TOS::Logger("Resources").debug() << "Ресурс " << fs_path << " найден в фс";
        return resource;
    }

    if(auto iter = _binary_assets_symbols.find(path); iter != _binary_assets_symbols.end()) {
        TOS::Logger("Resources").debug() << "Ресурс " << fs_path << " is inlined";
        return std::make_shared<Resource>((const uint8_t*) std::get<0>(iter->second), std::get<1>(iter->second)-std::get<0>(iter->second));
    }
    
    MAKE_ERROR("Ресурс " << path << " не найден");
}

}