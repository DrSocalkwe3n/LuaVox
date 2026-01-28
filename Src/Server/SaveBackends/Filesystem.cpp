#include "Filesystem.hpp"
#include "Server/Abstract.hpp"
#include "Server/SaveBackend.hpp"
#include "TOSLib.hpp"
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parser.hpp>
#include <boost/json/serializer.hpp>
#include <boost/json/value.hpp>
#include <memory>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

namespace LV::Server::SaveBackends {

namespace fs = std::filesystem;
namespace js = boost::json;

namespace {

constexpr uint32_t kRegionVersion = 1;
constexpr size_t kRegionNodeCount = 4 * 4 * 4 * 16 * 16 * 16;

template<typename T>
js::object packIdMap(const std::vector<std::pair<T, std::string>>& map) {
    js::object out;
    for(const auto& [id, key] : map) {
        out[std::to_string(id)] = key;
    }
    return out;
}

void unpackIdMap(const js::object& obj, std::vector<std::string>& out) {
    size_t maxId = 0;
    for(const auto& kvp : obj) {
        try {
            maxId = std::max(maxId, static_cast<size_t>(std::stoul(kvp.key())));
        } catch(...) {
            continue;
        }
    }

    out.assign(maxId + 1, {});

    for(const auto& kvp : obj) {
        try {
            size_t id = std::stoul(kvp.key());
            out[id] = std::string(kvp.value().as_string());
        } catch(...) {
            continue;
        }
    }
}

std::string encodeCompressed(const uint8_t* data, size_t size) {
    std::u8string compressed = compressLinear(std::u8string_view(reinterpret_cast<const char8_t*>(data), size));
    return TOS::Enc::toBase64(reinterpret_cast<const uint8_t*>(compressed.data()), compressed.size());
}

std::u8string decodeCompressed(const std::string& base64) {
    if(base64.empty())
        return {};

    TOS::ByteBuffer buffer = TOS::Enc::fromBase64(base64);
    return unCompressLinear(std::u8string_view(reinterpret_cast<const char8_t*>(buffer.data()), buffer.size()));
}

bool writeRegionFile(const fs::path& path, const SB_Region_In& data) {
    js::object jobj;
    jobj["version"] = kRegionVersion;

    {
        std::vector<VoxelCube_Region> voxels;
        convertChunkVoxelsToRegion(data.Voxels, voxels);

        js::object jvoxels;
        jvoxels["count"] = static_cast<uint64_t>(voxels.size());
        if(!voxels.empty()) {
            const uint8_t* raw = reinterpret_cast<const uint8_t*>(voxels.data());
            size_t rawSize = sizeof(VoxelCube_Region) * voxels.size();
            jvoxels["data"] = encodeCompressed(raw, rawSize);
        } else {
            jvoxels["data"] = "";
        }

        jobj["voxels"] = std::move(jvoxels);
        jobj["voxels_map"] = packIdMap(data.VoxelsMap);
    }

    {
        js::object jnodes;
        const Node* nodePtr = data.Nodes[0].data();
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(nodePtr);
        size_t rawSize = sizeof(Node) * kRegionNodeCount;
        jnodes["data"] = encodeCompressed(raw, rawSize);
        jobj["nodes"] = std::move(jnodes);
        jobj["nodes_map"] = packIdMap(data.NodeMap);
    }

    {
        js::array ents;
        for(const Entity& entity : data.Entityes) {
            js::object je;
            je["def"] = static_cast<uint64_t>(entity.getDefId());
            je["world"] = static_cast<uint64_t>(entity.WorldId);
            je["pos"] = js::array{entity.Pos.x, entity.Pos.y, entity.Pos.z};
            je["speed"] = js::array{entity.Speed.x, entity.Speed.y, entity.Speed.z};
            je["accel"] = js::array{entity.Acceleration.x, entity.Acceleration.y, entity.Acceleration.z};
            je["quat"] = js::array{entity.Quat.x, entity.Quat.y, entity.Quat.z, entity.Quat.w};
            je["hp"] = static_cast<uint64_t>(entity.HP);
            je["abbox"] = js::array{entity.ABBOX.x, entity.ABBOX.y, entity.ABBOX.z};
            je["in_region"] = js::array{entity.InRegionPos.x, entity.InRegionPos.y, entity.InRegionPos.z};

            js::object tags;
            for(const auto& [key, value] : entity.Tags) {
                tags[key] = value;
            }
            je["tags"] = std::move(tags);

            ents.push_back(std::move(je));
        }

        jobj["entities"] = std::move(ents);
        jobj["entities_map"] = packIdMap(data.EntityMap);
    }

    fs::create_directories(path.parent_path());
    std::ofstream fd(path, std::ios::binary);
    if(!fd)
        return false;

    fd << js::serialize(jobj);
    return true;
}

bool readRegionFile(const fs::path& path, DB_Region_Out& out) {
    try {
        std::ifstream fd(path, std::ios::binary);
        if(!fd)
            return false;

        out = {};

        js::object jobj = js::parse(fd).as_object();

    if(auto it = jobj.find("voxels"); it != jobj.end()) {
        const js::object& jvoxels = it->value().as_object();
        size_t count = 0;
        if(auto itCount = jvoxels.find("count"); itCount != jvoxels.end())
            count = static_cast<size_t>(itCount->value().to_number<uint64_t>());

        std::string base64;
        if(auto itData = jvoxels.find("data"); itData != jvoxels.end())
            base64 = std::string(itData->value().as_string());

        if(count > 0 && !base64.empty()) {
            std::u8string raw = decodeCompressed(base64);
            if(raw.size() != sizeof(VoxelCube_Region) * count)
                return false;

            out.Voxels.resize(count);
            std::memcpy(out.Voxels.data(), raw.data(), raw.size());
        }
    }

    if(auto it = jobj.find("voxels_map"); it != jobj.end()) {
        unpackIdMap(it->value().as_object(), out.VoxelIdToKey);
    }

    if(auto it = jobj.find("nodes"); it != jobj.end()) {
        const js::object& jnodes = it->value().as_object();
        std::string base64;
        if(auto itData = jnodes.find("data"); itData != jnodes.end())
            base64 = std::string(itData->value().as_string());

        if(!base64.empty()) {
            std::u8string raw = decodeCompressed(base64);
            if(raw.size() != sizeof(Node) * kRegionNodeCount)
                return false;

            std::memcpy(out.Nodes[0].data(), raw.data(), raw.size());
        }
    }

    if(auto it = jobj.find("nodes_map"); it != jobj.end()) {
        unpackIdMap(it->value().as_object(), out.NodeIdToKey);
    }

    if(auto it = jobj.find("entities"); it != jobj.end()) {
        const js::array& ents = it->value().as_array();
        out.Entityes.reserve(ents.size());

        for(const js::value& val : ents) {
            const js::object& je = val.as_object();
            DefEntityId defId = static_cast<DefEntityId>(je.at("def").to_number<uint64_t>());
            Entity entity(defId);

            if(auto itWorld = je.find("world"); itWorld != je.end())
                entity.WorldId = static_cast<DefWorldId>(itWorld->value().to_number<uint64_t>());

            if(auto itPos = je.find("pos"); itPos != je.end()) {
                const js::array& arr = itPos->value().as_array();
                entity.Pos = Pos::Object(
                    static_cast<int32_t>(arr.at(0).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(1).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(2).to_number<int64_t>())
                );
            }

            if(auto itSpeed = je.find("speed"); itSpeed != je.end()) {
                const js::array& arr = itSpeed->value().as_array();
                entity.Speed = Pos::Object(
                    static_cast<int32_t>(arr.at(0).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(1).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(2).to_number<int64_t>())
                );
            }

            if(auto itAccel = je.find("accel"); itAccel != je.end()) {
                const js::array& arr = itAccel->value().as_array();
                entity.Acceleration = Pos::Object(
                    static_cast<int32_t>(arr.at(0).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(1).to_number<int64_t>()),
                    static_cast<int32_t>(arr.at(2).to_number<int64_t>())
                );
            }

            if(auto itQuat = je.find("quat"); itQuat != je.end()) {
                const js::array& arr = itQuat->value().as_array();
                entity.Quat = glm::quat(
                    static_cast<float>(arr.at(3).to_number<double>()),
                    static_cast<float>(arr.at(0).to_number<double>()),
                    static_cast<float>(arr.at(1).to_number<double>()),
                    static_cast<float>(arr.at(2).to_number<double>())
                );
            }

            if(auto itHp = je.find("hp"); itHp != je.end())
                entity.HP = static_cast<uint32_t>(itHp->value().to_number<uint64_t>());

            if(auto itAabb = je.find("abbox"); itAabb != je.end()) {
                const js::array& arr = itAabb->value().as_array();
                entity.ABBOX.x = static_cast<uint64_t>(arr.at(0).to_number<uint64_t>());
                entity.ABBOX.y = static_cast<uint64_t>(arr.at(1).to_number<uint64_t>());
                entity.ABBOX.z = static_cast<uint64_t>(arr.at(2).to_number<uint64_t>());
            }

            if(auto itRegion = je.find("in_region"); itRegion != je.end()) {
                const js::array& arr = itRegion->value().as_array();
                entity.InRegionPos = Pos::GlobalRegion(
                    static_cast<int16_t>(arr.at(0).to_number<int64_t>()),
                    static_cast<int16_t>(arr.at(1).to_number<int64_t>()),
                    static_cast<int16_t>(arr.at(2).to_number<int64_t>())
                );
            }

            if(auto itTags = je.find("tags"); itTags != je.end()) {
                const js::object& tags = itTags->value().as_object();
                for(const auto& kvp : tags) {
                    entity.Tags[std::string(kvp.key())] = static_cast<float>(kvp.value().to_number<double>());
                }
            }

            out.Entityes.push_back(std::move(entity));
        }
    }

    if(auto it = jobj.find("entities_map"); it != jobj.end()) {
        unpackIdMap(it->value().as_object(), out.EntityToKey);
    }

        return true;
    } catch(const std::exception& exc) {
        TOS::Logger("RegionLoader::Filesystem").warn() << "Не удалось загрузить регион " << path << "\n\t" << exc.what();
        return false;
    }
}

}

class WSB_Filesystem : public IWorldSaveBackend {
    fs::path Dir;

public:
    WSB_Filesystem(const boost::json::object &data) {
        Dir = (std::string) data.at("path").as_string();
    }

    virtual ~WSB_Filesystem() {

    }

    fs::path getPath(std::string worldId, Pos::GlobalRegion regionPos) {
        return Dir / worldId / std::to_string(regionPos.x) / std::to_string(regionPos.y) / std::to_string(regionPos.z);
    }

    virtual TickSyncInfo_Out tickSync(TickSyncInfo_In &&data) override {
        TickSyncInfo_Out out;
        // Сохранение регионов
        for(auto& [worldId, regions] : data.ToSave) {
            for(auto& [regionPos, region] : regions) {
                writeRegionFile(getPath(std::to_string(worldId), regionPos), region);
            }
        }

        // Загрузка регионов
        for(auto& [worldId, regions] : data.Load) {
            for(const Pos::GlobalRegion& regionPos : regions) {
                const fs::path path = getPath(std::to_string(worldId), regionPos);
                if(!fs::exists(path)) {
                    out.NotExisten[worldId].push_back(regionPos);
                    continue;
                }

                DB_Region_Out regionOut;
                if(!readRegionFile(path, regionOut)) {
                    out.NotExisten[worldId].push_back(regionPos);
                    continue;
                }

                out.LoadedRegions[worldId].push_back({regionPos, std::move(regionOut)});
            }
        }

        return out;
    }

    virtual void changePreloadDistance(uint8_t value) override {

    }

    // virtual void load(std::string worldId, Pos::GlobalRegion regionPos, SB_Region *data) {
    //     std::ifstream fd(getPath(worldId, regionPos));
    //     js::object jobj = js::parse(fd).as_object();

    //     {
    //         js::array &jaVoxels = jobj.at("Voxels").as_array();
    //         for(js::value &jvVoxel : jaVoxels) {
    //             js::object &joVoxel = jvVoxel.as_object();
    //             VoxelCube_Region cube;
    //             cube.Data = joVoxel.at("Data").as_uint64();
    //             cube.Left.x = joVoxel.at("LeftX").as_uint64();
    //             cube.Left.y = joVoxel.at("LeftY").as_uint64();
    //             cube.Left.z = joVoxel.at("LeftZ").as_uint64();
    //             cube.Right.x = joVoxel.at("RightX").as_uint64();
    //             cube.Right.y = joVoxel.at("RightY").as_uint64();
    //             cube.Right.z = joVoxel.at("RightZ").as_uint64();
    //             data->Voxels.push_back(cube);
    //         }
    //     }

    //     {
    //         js::object &joVoxelMap = jobj.at("VoxelsMap").as_object();
    //         for(js::key_value_pair &jkvp : joVoxelMap) {
    //             data->VoxelsMap[std::stoul(jkvp.key())] = jkvp.value().as_string();
    //         }
    //     }
    // }

    // virtual void save(std::string worldId, Pos::GlobalRegion regionPos, const SB_Region *data) {
    //     js::object jobj;

    //     {
    //         js::array jaVoxels;
    //         for(const VoxelCube_Region &cube : data->Voxels) {
    //             js::object joVoxel;
    //             joVoxel["Data"] = cube.Data;
    //             joVoxel["LeftX"] = cube.Left.x;
    //             joVoxel["LeftY"] = cube.Left.y;
    //             joVoxel["LeftZ"] = cube.Left.z;
    //             joVoxel["RightX"] = cube.Right.x;
    //             joVoxel["RightY"] = cube.Right.y;
    //             joVoxel["RightZ"] = cube.Right.z;
    //             jaVoxels.push_back(std::move(joVoxel));
    //         }

    //         jobj["Voxels"] = std::move(jaVoxels);
    //     }

    //     {
    //         js::object joVoxelMap;
    //         for(const auto &pair : data->VoxelsMap) {
    //             joVoxelMap[std::to_string(pair.first)] = pair.second;
    //         }

    //         jobj["VoxelsMap"] = std::move(joVoxelMap);
    //     }

    //     fs::create_directories(getPath(worldId, regionPos).parent_path());
    //     std::ofstream fd(getPath(worldId, regionPos));
    //     fd << js::serialize(jobj);
    // }
};

class PSB_Filesystem : public IPlayerSaveBackend {
    fs::path Dir;

public:
    PSB_Filesystem(const boost::json::object &data) {
        Dir = (std::string) data.at("path").as_string();
    }

    virtual ~PSB_Filesystem() {

    }

    fs::path getPath(PlayerId_t playerId) {
        return Dir / std::to_string(playerId);
    }

    virtual bool isAsync() { return false; };

    virtual bool isExist(PlayerId_t playerId) {
        return fs::exists(getPath(playerId));
    }

    virtual void load(PlayerId_t playerId, SB_Player *data) {

    }

    virtual void save(PlayerId_t playerId, const SB_Player *data) {

    }

    virtual void remove(PlayerId_t playerId) {

    }
};

class ASB_Filesystem : public IAuthSaveBackend {
    fs::path Dir;

public:
    ASB_Filesystem(const boost::json::object &data) {
        Dir = (std::string) data.at("path").as_string();
    }

    virtual ~ASB_Filesystem() {

    }

    fs::path getPath(std::string playerId) {
        return Dir / playerId;
    }

    virtual bool isAsync() { return false; };

    virtual coro<bool> isExist(std::string useranme) override {
        co_return fs::exists(getPath(useranme));
    }

    virtual coro<> rename(std::string prevUsername, std::string newUsername) override {
        fs::rename(getPath(prevUsername), getPath(newUsername));
        co_return;
    }

    virtual coro<bool> load(std::string useranme, SB_Auth& data) override {
        std::ifstream fd(getPath(useranme));
        js::object jobj = js::parse(fd).as_object();

        data.Id = jobj.at("Id").as_uint64();
        data.PasswordHash = jobj.at("PasswordHash").as_string();
        co_return true;
    }

    virtual coro<> save(std::string playerId, const SB_Auth& data) override {
        js::object jobj;

        jobj["Id"] = data.Id;
        jobj["PasswordHash"] = data.PasswordHash;

        fs::create_directories(getPath(playerId).parent_path());
        std::ofstream fd(getPath(playerId));
        fd << js::serialize(jobj);
        co_return;
    }

    virtual coro<> remove(std::string username) override {
        fs::remove(getPath(username));
        co_return;
    }
};

class MSSB_Filesystem : public IModStorageSaveBackend {
    fs::path Dir;

public:
    MSSB_Filesystem(const boost::json::object &data) {
        Dir = (std::string) data.at("path").as_string();
    }

    virtual ~MSSB_Filesystem() {

    }

    virtual bool isAsync() { return false; };

    virtual void load(std::string domain, std::string key, std::string *data) {

    }

    virtual void save(std::string domain, std::string key, const std::string *data) {

    }
    virtual void remove(std::string domain, std::string key) {

    }

    virtual void remove(std::string domain) {

    }
};

Filesystem::~Filesystem() {

}

bool Filesystem::isAvailable() {
    return true;
}

std::string Filesystem::getName() {
    return "Filesystem"; 
}

std::unique_ptr<IWorldSaveBackend> Filesystem::createWorld(boost::json::object data) {
    return std::make_unique<WSB_Filesystem>(data);
}

std::unique_ptr<IPlayerSaveBackend> Filesystem::createPlayer(boost::json::object data) {
    return std::make_unique<PSB_Filesystem>(data);
}

std::unique_ptr<IAuthSaveBackend> Filesystem::createAuth(boost::json::object data) {
    return std::make_unique<ASB_Filesystem>(data);
}

std::unique_ptr<IModStorageSaveBackend> Filesystem::createModStorage(boost::json::object data) {
    return std::make_unique<MSSB_Filesystem>(data);
}

}
