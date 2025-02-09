#pragma once

#include <Common/Abstract.hpp>
#include "Abstract.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace LV::Server {

class RemoteClient;
class GameServer;
class World;


struct ServerObjectPos {
    WorldId_t WorldId;
    Pos::Object ObjectPos;
};


/*
    Сфера в которой отслеживаются события игроком
*/
struct ContentViewCircle {
    WorldId_t WorldId;
    // Позиция в чанках
    glm::i16vec3 Pos;
    // (Единица равна размеру чанку) в квадрате
    int32_t Range;

    inline int32_t sqrDistance(Pos::GlobalRegion regionPos) const {
        glm::i32vec3 vec = {Pos.x-((regionPos.X << 4) | 0b1000), Pos.y-((regionPos.Y << 4) | 0b1000), Pos.z-((regionPos.Z << 4) | 0b1000)};
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    inline int32_t sqrDistance(Pos::GlobalChunk chunkPos) const {
        glm::i32vec3 vec = {Pos.x-chunkPos.X, Pos.y-chunkPos.Y, Pos.z-chunkPos.Z};
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    inline int64_t sqrDistance(Pos::Object objectPos) const {
        glm::i32vec3 vec = {Pos.x-(objectPos.x >> 20), Pos.y-(objectPos.y >> 20), Pos.z-(objectPos.z >> 20)};
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    bool isIn(Pos::GlobalRegion regionPos) const {
        return sqrDistance(regionPos) < Range+192; // (8×sqrt(3))^2
    }

    bool isIn(Pos::GlobalChunk chunkPos) const {
        return sqrDistance(chunkPos) < Range+3; // (1×sqrt(3))^2
    }

    bool isIn(Pos::Object objectPos, int32_t size = 0) const {
        return sqrDistance(objectPos) < Range+3+size;
    }
};


/*
    Мост контента, для отслеживания событий из удалённх точек
    По типу портала, через который можно видеть контент на расстоянии
*/
struct ContentBridge {
    /* 
        false -> Из точки From видно контент из точки To 
        true -> Контент виден в обе стороны
    */
    bool IsTwoWay = false;
    WorldId_t LeftWorld;
    // Позиция в чанках
    glm::i16vec3 LeftPos;
    WorldId_t RightWorld;
    // Позиция в чанках
    glm::i16vec3 RightPos;
};


/* Игрок */
class ContentEventController {
private:

    struct SubscribedObj {
        // Используется регионами
        std::vector<PortalId_t> Portals;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_set<Pos::Local16_u>>> Chunks;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_set<LocalEntityId_t>>> Entities;
    } Subscribed;

public:
    // Управляется сервером
    std::unique_ptr<RemoteClient> Remote;
    // Регионы сюда заглядывают
    std::unordered_map<WorldId_t, std::vector<ContentViewCircle>> ContentViewCircles;
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> SubscribedRegions;

public:
    ContentEventController(std::unique_ptr<RemoteClient> &&remote);

    // Измеряется в чанках в длину
    uint8_t getViewRange() const;
    ServerObjectPos getLastPos() const;
    ServerObjectPos getPos() const;

    // Навешивается слушателем событий на регионы
    // Здесь приходят частично фильтрованные события
    
    // Регионы следят за чанками, которые видят игроки
    void onRegionsLost(WorldId_t worldId, const std::vector<Pos::GlobalRegion> &lost);
    void onChunksEnterLost(WorldId_t worldId, World *worldObj, Pos::GlobalRegion regionId, const std::unordered_set<Pos::Local16_u> &enter, const std::unordered_set<Pos::Local16_u> &lost);
    // Нужно фильтровать неотслеживаемые чанки
    void onChunksUpdate_Voxels(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> &chunks);
    void onChunksUpdate_Nodes(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> &chunks);
    void onChunksUpdate_LightPrism(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::Local16_u, const LightPrism*> &chunks);

    void onEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_set<LocalEntityId_t> &enter, const std::unordered_set<LocalEntityId_t> &lost);
    void onEntitySwap(WorldId_t lastWorldId, Pos::GlobalRegion lastRegionPos, LocalEntityId_t lastId, WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, LocalEntityId_t newId);
    void onEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::vector<Entity> &entities);

    void onPortalEnterLost(const std::vector<void*> &enter, const std::vector<void*> &lost);
    void onPortalUpdates(const std::vector<void*> &portals);

    inline const SubscribedObj& getSubscribed() { return Subscribed; };

};


}

namespace std {

template <>
struct hash<LV::Server::ServerObjectPos> {
    std::size_t operator()(const LV::Server::ServerObjectPos& obj) const {
        return std::hash<uint32_t>()(obj.WorldId) ^ std::hash<int32_t>()(obj.ObjectPos.x) ^ std::hash<int32_t>()(obj.ObjectPos.y) ^ std::hash<int32_t>()(obj.ObjectPos.z); 
    } 
};


}
