#pragma once

#include <Common/Abstract.hpp>
#include "Abstract.hpp"
#include <bitset>
#include <map>
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
    // (Единица равна размеру чанка) в квадрате
    int32_t Range;

    inline int32_t sqrDistance(Pos::GlobalRegion regionPos) const {
        glm::i32vec3 vec = Pos-(glm::i16vec3) ((Pos::GlobalChunk(regionPos) << 2) | 0b10);
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    inline int32_t sqrDistance(Pos::GlobalChunk chunkPos) const {
        glm::i32vec3 vec = Pos-(glm::i16vec3) chunkPos;
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    inline int64_t sqrDistance(Pos::Object objectPos) const {
        glm::i32vec3 vec = Pos-(glm::i16vec3) (objectPos >> 12 >> 4);
        return vec.x*vec.x+vec.y*vec.y+vec.z*vec.z;
    };

    bool isIn(Pos::GlobalRegion regionPos) const {
        return sqrDistance(regionPos) < Range+12; // (2×sqrt(3))^2
    }

    bool isIn(Pos::GlobalChunk chunkPos) const {
        return sqrDistance(chunkPos) < Range+3; // (1×sqrt(3))^2
    }

    bool isIn(Pos::Object objectPos, int32_t size = 0) const {
        return sqrDistance(objectPos) < Range+3+size;
    }
};

// Регион -> чанки попавшие под обозрение Pos::bvec4u
using ContentViewWorld = std::map<Pos::GlobalRegion, std::bitset<64>>; // 1 - чанк виден, 0 - не виден

struct ContentViewGlobal_DiffInfo;

struct ContentViewGlobal : public std::map<WorldId_t, ContentViewWorld>  {
    // Вычисляет половинную разницу между текущей и предыдущей области видимости
    // Возвращает области, которые появились по отношению к old, чтобы получить области потерянные из виду поменять местами *this и old
    ContentViewGlobal_DiffInfo calcDiffWith(const ContentViewGlobal &old) const;
};

struct ContentViewGlobal_DiffInfo {
    // Новые увиденные чанки
    ContentViewGlobal View;
    // Регионы
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Regions;
    // Миры
    std::vector<WorldId_t> Worlds;

    bool empty() const {
        return View.empty() && Regions.empty() && Worlds.empty();
    }
};

inline ContentViewGlobal_DiffInfo ContentViewGlobal::calcDiffWith(const ContentViewGlobal &old) const {
    ContentViewGlobal_DiffInfo newView;

    // Рассматриваем разницу меж мирами
    for(const auto &[newWorldId, newWorldView] : *this) {
        auto oldWorldIter = old.find(newWorldId);
        if(oldWorldIter == old.end()) { // В старом состоянии нет мира
            newView.View[newWorldId] = newWorldView;
            newView.Worlds.push_back(newWorldId);
            auto &newRegions = newView.Regions[newWorldId];
            for(const auto &[regionPos, _] : newWorldView)
                newRegions.push_back(regionPos);
        } else {
            const std::map<Pos::GlobalRegion, std::bitset<64>> &newRegions = newWorldView;
            const std::map<Pos::GlobalRegion, std::bitset<64>> &oldRegions = oldWorldIter->second;
            std::map<Pos::GlobalRegion, std::bitset<64>> *diffRegions = nullptr;

            // Рассматриваем разницу меж регионами
            for(const auto &[newRegionPos, newRegionBitField] : newRegions) {
                auto oldRegionIter = oldRegions.find(newRegionPos);
                if(oldRegionIter == oldRegions.end()) { // В старой описи мира нет региона
                    if(!diffRegions)
                        diffRegions = &newView.View[newWorldId];

                    (*diffRegions)[newRegionPos] = newRegionBitField;
                    newView.Regions[newWorldId].push_back(newRegionPos);
                } else {
                    const std::bitset<64> &oldChunks = oldRegionIter->second;
                    std::bitset<64> chunks = (~oldChunks) & newRegionBitField; // Останется поле с новыми чанками
                    if(chunks._Find_first() != chunks.size()) {
                        // Есть новые чанки
                        if(!diffRegions)
                            diffRegions = &newView.View[newWorldId];

                        (*diffRegions)[newRegionPos] = chunks;
                    }
                }
            }
        }
    }

    return newView;
}


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
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_set<RegionEntityId_t>>> Entities;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_set<RegionEntityId_t>>> FuncEntities;
    } Subscribed;

public:
    // Управляется сервером
    std::unique_ptr<RemoteClient> Remote;
    // Регионы сюда заглядывают
    // Каждый такт значения изменений обновляются GameServer'ом
    // Объявленная в чанках территория точно отслеживается (активная зона)
    ContentViewGlobal ContentViewState;
    ContentViewGlobal_DiffInfo ContentView_NewView, ContentView_LostView;
    // Миры добавленные в наблюдение в текущем такте
    std::vector<WorldId_t> NewWorlds;

    // size_t CVCHash = 0; // Хэш для std::vector<ContentViewCircle>
    // std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> SubscribedRegions;

public:
    ContentEventController(std::unique_ptr<RemoteClient> &&remote);

    // Измеряется в чанках в радиусе (активная зона)
    uint16_t getViewRangeActive() const;
    // Измеряется в чанках в радиусе (Декоративная зона) + getViewRangeActive()
    uint16_t getViewRangeBackground() const;
    ServerObjectPos getLastPos() const;
    ServerObjectPos getPos() const;

    // Проверка на необходимость подгрузки новых определений миров
    // и очистка клиента от не наблюдаемых данных
    void checkContentViewChanges();
    // Здесь приходят частично фильтрованные события
    // Фильтровать не отслеживаемые миры
    void onWorldUpdate(WorldId_t worldId, World *worldObj);

    // Нужно фильтровать неотслеживаемые чанки
    void onChunksUpdate_Voxels(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> &chunks);
    void onChunksUpdate_Nodes(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::bvec4u, const Node*> &chunks);
    //void onChunksUpdate_LightPrism(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<Pos::bvec4u, const LightPrism*> &chunks);

    void onEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_set<RegionEntityId_t> &enter, const std::unordered_set<RegionEntityId_t> &lost);
    void onEntitySwap(WorldId_t lastWorldId, Pos::GlobalRegion lastRegionPos, RegionEntityId_t lastId, WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, RegionEntityId_t newId);
    void onEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::vector<Entity> &entities);

    void onFuncEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_set<RegionEntityId_t> &enter, const std::unordered_set<RegionEntityId_t> &lost);
    void onFuncEntitySwap(WorldId_t lastWorldId, Pos::GlobalRegion lastRegionPos, RegionEntityId_t lastId, WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, RegionEntityId_t newId);
    void onFuncEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::vector<Entity> &entities);

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


template <>
struct hash<LV::Server::ContentViewCircle> {
    size_t operator()(const LV::Server::ContentViewCircle& obj) const noexcept {
        // Используем стандартную функцию хеширования для uint32_t, glm::i16vec3 и int32_t
        auto worldIdHash = std::hash<uint32_t>{}(obj.WorldId) << 32;
        auto posHash = 
            std::hash<int16_t>{}(obj.Pos.x) ^
            (std::hash<int16_t>{}(obj.Pos.y) << 16) ^ 
            (std::hash<int16_t>{}(obj.Pos.z) << 32);
        auto rangeHash = std::hash<int32_t>{}(obj.Range);

        return worldIdHash ^
                posHash ^
                (~rangeHash << 32);
    }
};
}
