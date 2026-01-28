#pragma once

#include "Abstract.hpp"
#include "Common/Abstract.hpp"
#include "Common/Async.hpp"
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <memory>
#include <unordered_map>


namespace LV::Server {

/*
    Обменная единица мира
*/
struct SB_Region_In {
    // Список вокселей всех чанков
    std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> Voxels;
    // Привязка вокселей к ключу профиля
    std::vector<std::pair<DefVoxelId, std::string>> VoxelsMap;
    // Ноды всех чанков
    std::array<std::array<Node, 16*16*16>, 4*4*4> Nodes;
    // Привязка нод к ключу профиля
    std::vector<std::pair<DefNodeId, std::string>> NodeMap;
    // Сущности
    std::vector<Entity> Entityes;
    // Привязка идентификатора к ключу профиля
    std::vector<std::pair<DefEntityId, std::string>> EntityMap;
};

struct DB_Region_Out {
    std::vector<VoxelCube_Region> Voxels;
    std::array<std::array<Node, 16*16*16>, 4*4*4> Nodes;
    std::vector<Entity> Entityes;

    std::vector<std::string> VoxelIdToKey, NodeIdToKey, EntityToKey;
};

class IWorldSaveBackend {
public:
    virtual ~IWorldSaveBackend();

    struct TickSyncInfo_In {
        // Для загрузки и более не используемые (регионы автоматически подгружаются по списку загруженных)
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Load, Unload;
        // Регионы для сохранения
        std::unordered_map<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, SB_Region_In>>> ToSave;
    };

    struct TickSyncInfo_Out {
        std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> NotExisten;
        std::unordered_map<WorldId_t, std::vector<std::pair<Pos::GlobalRegion, DB_Region_Out>>> LoadedRegions;
    };

    /*
        Обмен данными раз в такт
        Хотим списки на загрузку регионов
        Отдаём уже загруженные регионы и список отсутствующих в базе регионов
    */
    virtual TickSyncInfo_Out tickSync(TickSyncInfo_In &&data) = 0;

    /*
        Устанавливает радиус вокруг прогруженного региона для предзагрузки регионов
    */
    virtual void changePreloadDistance(uint8_t value) = 0;
};

struct SB_Player {

};

class IPlayerSaveBackend {
public:
    virtual ~IPlayerSaveBackend();

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

    // Существует ли игрок
    virtual coro<bool> isExist(std::string username) = 0;
    // Переименовать игрока
    virtual coro<> rename(std::string prevUsername, std::string newUsername) = 0;
    // Загрузить игрока (если есть, вернёт true)
    virtual coro<bool> load(std::string username, SB_Auth &data) = 0;
    // Сохранить игрока
    virtual coro<> save(std::string username, const SB_Auth &data) = 0;
    // Удалить игрока
    virtual coro<> remove(std::string username) = 0;
};

class IModStorageSaveBackend {
public:
    virtual ~IModStorageSaveBackend();

    // // Загрузить запись
    // virtual void load(std::string domain, std::string key, std::string *data) = 0;
    // // Сохранить запись
    // virtual void save(std::string domain, std::string key, const std::string *data) = 0;
    // // Удалить запись
    // virtual void remove(std::string domain, std::string key) = 0;
    // // Удалить домен
    // virtual void remove(std::string domain) = 0;
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
