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

namespace AL::Server::SaveBackends {

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
        return Dir / worldId / std::to_string(regionPos.X) / std::to_string(regionPos.Y) / std::to_string(regionPos.Z);
    }

    virtual bool isAsync() { return false; };

    virtual bool isExist(std::string worldId, Pos::GlobalRegion regionPos) {
        return fs::exists(getPath(worldId, regionPos));
    }

    virtual void load(std::string worldId, Pos::GlobalRegion regionPos, SB_Region *data) {
        std::ifstream fd(getPath(worldId, regionPos));
        js::object jobj = js::parse(fd).as_object();

        {
            js::array &jaVoxels = jobj.at("Voxels").as_array();
            for(js::value &jvVoxel : jaVoxels) {
                js::object &joVoxel = jvVoxel.as_object();
                VoxelCube_Region cube;
                cube.Material = joVoxel.at("Material").as_uint64();
                cube.Left.X = joVoxel.at("LeftX").as_uint64();
                cube.Left.Y = joVoxel.at("LeftY").as_uint64();
                cube.Left.Z = joVoxel.at("LeftZ").as_uint64();
                cube.Right.X = joVoxel.at("RightX").as_uint64();
                cube.Right.Y = joVoxel.at("RightY").as_uint64();
                cube.Right.Z = joVoxel.at("RightZ").as_uint64();
                data->Voxels.push_back(cube);
            }
        }

        {
            js::object &joVoxelMap = jobj.at("VoxelsMap").as_object();
            for(js::key_value_pair &jkvp : joVoxelMap) {
                data->VoxelsMap[std::stoul(jkvp.key())] = jkvp.value().as_string();
            }
        }
    }

    virtual void save(std::string worldId, Pos::GlobalRegion regionPos, const SB_Region *data) {
        js::object jobj;

        {
            js::array jaVoxels;
            for(const VoxelCube_Region &cube : data->Voxels) {
                js::object joVoxel;
                joVoxel["Material"] = cube.Material;
                joVoxel["LeftX"] = cube.Left.X;
                joVoxel["LeftY"] = cube.Left.Y;
                joVoxel["LeftZ"] = cube.Left.Z;
                joVoxel["RightX"] = cube.Right.X;
                joVoxel["RightY"] = cube.Right.Y;
                joVoxel["RightZ"] = cube.Right.Z;
                jaVoxels.push_back(std::move(joVoxel));
            }

            jobj["Voxels"] = std::move(jaVoxels);
        }

        {
            js::object joVoxelMap;
            for(const auto &pair : data->VoxelsMap) {
                joVoxelMap[std::to_string(pair.first)] = pair.second;
            }

            jobj["VoxelsMap"] = std::move(joVoxelMap);
        }

        fs::create_directories(getPath(worldId, regionPos).parent_path());
        std::ofstream fd(getPath(worldId, regionPos));
        fd << js::serialize(jobj);
    }

    virtual void remove(std::string worldId, Pos::GlobalRegion regionPos) {
        fs::remove(getPath(worldId, regionPos));
    }

    virtual void remove(std::string worldId) {
        fs::remove_all(Dir / worldId);
    }
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

    virtual bool isExist(std::string playerId) {
        return fs::exists(getPath(playerId));
    }

    virtual void rename(std::string fromPlayerId, std::string toPlayerId) {
        fs::rename(getPath(fromPlayerId), getPath(toPlayerId));
    }

    virtual void load(std::string playerId, SB_Auth *data) {
        std::ifstream fd(getPath(playerId));
        js::object jobj = js::parse(fd).as_object();

        data->Id = jobj.at("Id").as_uint64();
        data->PasswordHash = jobj.at("PasswordHash").as_string();
    }

    virtual void save(std::string playerId, const SB_Auth *data) {
        js::object jobj;

        jobj["Id"] = data->Id;
        jobj["PasswordHash"] = data->PasswordHash;

        fs::create_directories(getPath(playerId).parent_path());
        std::ofstream fd(getPath(playerId));
        fd << js::serialize(jobj);
    }

    virtual void remove(std::string playerId) {
        fs::remove(getPath(playerId));
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