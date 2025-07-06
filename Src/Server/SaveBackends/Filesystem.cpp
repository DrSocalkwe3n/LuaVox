#include "Filesystem.hpp"
#include "Server/Abstract.hpp"
#include "Server/SaveBackend.hpp"
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

namespace LV::Server::SaveBackends {

namespace fs = std::filesystem;
namespace js = boost::json;

class WSB_Filesystem : public IWorldSaveBackend {
    fs::path Dir;

public:
    WSB_Filesystem(const boost::json::object &data) {
        Dir = (std::string) data.at("Path").as_string();
    }

    virtual ~WSB_Filesystem() {

    }

    fs::path getPath(std::string worldId, Pos::GlobalRegion regionPos) {
        return Dir / worldId / std::to_string(regionPos.x) / std::to_string(regionPos.y) / std::to_string(regionPos.z);
    }

    virtual TickSyncInfo_Out tickSync(TickSyncInfo_In &&data) override {
        TickSyncInfo_Out out;
        out.NotExisten = std::move(data.Load);
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
        Dir = (std::string) data.at("Path").as_string();
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
        Dir = (std::string) data.at("Path").as_string();
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
    }

    virtual coro<> save(std::string playerId, const SB_Auth& data) override {
        js::object jobj;

        jobj["Id"] = data.Id;
        jobj["PasswordHash"] = data.PasswordHash;

        fs::create_directories(getPath(playerId).parent_path());
        std::ofstream fd(getPath(playerId));
        fd << js::serialize(jobj);
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
        Dir = (std::string) data.at("Path").as_string();
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
