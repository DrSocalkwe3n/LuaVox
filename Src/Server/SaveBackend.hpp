#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <memory>
#include <unordered_map>


namespace LV::Server {

struct SB_Region {
    std::vector<VoxelCube_Region> Voxels;
    std::unordered_map<DefVoxelId_t, std::string> VoxelsMap;
    std::unordered_map<Pos::Local16_u, Node> Nodes;
    std::unordered_map<DefNodeId_t, std::string> NodeMap;
    std::vector<Entity> Entityes;
};

class IWorldSaveBackend {
public:
    virtual ~IWorldSaveBackend();

    // Может ли использоваться параллельно
    virtual bool isAsync() { return false; };
    // Существует ли регион
    virtual bool isExist(std::string worldId, Pos::GlobalRegion regionPos) = 0;
    // Загрузить регион
    virtual void load(std::string worldId, Pos::GlobalRegion regionPos, SB_Region *data) = 0;
    // Сохранить регион
    virtual void save(std::string worldId, Pos::GlobalRegion regionPos, const SB_Region *data) = 0;
    // Удалить регион
    virtual void remove(std::string worldId, Pos::GlobalRegion regionPos) = 0;
    // Удалить мир
    virtual void remove(std::string worldId) = 0;
};

struct SB_Player {

};

class IPlayerSaveBackend {
public:
    virtual ~IPlayerSaveBackend();

    // Может ли использоваться параллельно
    virtual bool isAsync() { return false; };
    // Существует ли игрок
    virtual bool isExist(PlayerId_t playerId) = 0;
    // Загрузить игрока
    virtual void load(PlayerId_t playerId, SB_Player *data) = 0;
    // Сохранить игрока
    virtual void save(PlayerId_t playerId, const SB_Player *data) = 0;
    // Удалить игрока
    virtual void remove(PlayerId_t playerId) = 0;
};

struct SB_Auth {
    uint32_t Id;
    std::string PasswordHash;
};

class IAuthSaveBackend {
public:
    virtual ~IAuthSaveBackend();

    // Может ли использоваться параллельно
    virtual bool isAsync() { return false; };
    // Существует ли игрок
    virtual bool isExist(std::string playerId) = 0;
    // Переименовать игрока
    virtual void rename(std::string fromPlayerId, std::string toPlayerId) = 0;
    // Загрузить игрока
    virtual void load(std::string playerId, SB_Auth *data) = 0;
    // Сохранить игрока
    virtual void save(std::string playerId, const SB_Auth *data) = 0;
    // Удалить игрока
    virtual void remove(std::string playerId) = 0;
};

class IModStorageSaveBackend {
public:
    virtual ~IModStorageSaveBackend();

    // Может ли использоваться параллельно
    virtual bool isAsync() { return false; };
    // Загрузить запись
    virtual void load(std::string domain, std::string key, std::string *data) = 0;
    // Сохранить запись
    virtual void save(std::string domain, std::string key, const std::string *data) = 0;
    // Удалить запись
    virtual void remove(std::string domain, std::string key) = 0;
    // Удалить домен
    virtual void remove(std::string domain) = 0;
};

class ISaveBackendProvider {
public:
    virtual ~ISaveBackendProvider();

    virtual bool isAvailable() = 0;
    virtual std::string getName() = 0;
    virtual std::unique_ptr<IWorldSaveBackend> createWorld(boost::json::object data) = 0;
    virtual std::unique_ptr<IPlayerSaveBackend> createPlayer(boost::json::object data) = 0;
    virtual std::unique_ptr<IAuthSaveBackend> createAuth(boost::json::object data) = 0;
    virtual std::unique_ptr<IModStorageSaveBackend> createModStorage(boost::json::object data) = 0;
};


}