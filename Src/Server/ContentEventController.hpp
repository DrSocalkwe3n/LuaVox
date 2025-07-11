#pragma once

#include <Common/Abstract.hpp>
#include "Abstract.hpp"
#include "TOSLib.hpp"
#include <algorithm>
#include <bitset>
#include <map>
#include <memory>
#include <queue>
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
    Разница между информацией о наблюдаемых регионах
*/
struct ContentViewInfo_Diff {
    // Изменения на уровне миров (увиден или потерян)
    std::vector<WorldId_t> WorldsNew, WorldsLost;
    // Изменения на уровне регионов
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> RegionsNew, RegionsLost;

    bool empty() const {
        return WorldsNew.empty() && WorldsLost.empty() && RegionsNew.empty() && RegionsLost.empty();
    }
};

/*
    То, какие регионы наблюдает игрок
*/
struct ContentViewInfo {
    // std::vector<Pos::GlobalRegion> - сортированный и с уникальными значениями
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Regions;

    // Что изменилось относительно obj
    // Перерасчёт должен проводится при смещении игрока или ContentBridge за границу региона
    ContentViewInfo_Diff diffWith(const ContentViewInfo& obj) const {
        ContentViewInfo_Diff out;

        // Проверяем новые миры и регионы
        for(const auto& [key, regions] : Regions) {
            auto iterWorld = obj.Regions.find(key);

            if(iterWorld == obj.Regions.end()) {
                out.WorldsNew.push_back(key);
                out.RegionsNew[key] = regions;
            } else {
                auto &vec = out.RegionsNew[key];
                vec.reserve(8*8);
                std::set_difference(
                    regions.begin(), regions.end(),
                    iterWorld->second.begin(), iterWorld->second.end(),
                    std::back_inserter(vec)
                );
            }
        }

        // Проверяем потерянные миры и регионы
        for(const auto& [key, regions] : obj.Regions) {
            auto iterWorld = Regions.find(key);

            if(iterWorld == Regions.end()) {
                out.WorldsLost.push_back(key);
                out.RegionsLost[key] = regions;
            } else {
                auto &vec = out.RegionsLost[key];
                vec.reserve(8*8);
                std::set_difference(
                    regions.begin(), regions.end(),
                    iterWorld->second.begin(), iterWorld->second.end(),
                    std::back_inserter(vec)
                );
            }
        }

        // shrink_to_feet
        for(auto& [_, regions] : out.RegionsNew)
            regions.shrink_to_fit();
        for(auto& [_, regions] : out.RegionsLost)
            regions.shrink_to_fit();

        return out;
    }
};

/*
    Мост контента, для отслеживания событий из удалённых точек
    По типу портала, через который можно видеть контент на расстоянии
*/
struct ContentBridge {
    /* 
        false -> Из точки Left видно контент в точки Right 
        true -> Контент виден в обе стороны
    */
    bool IsTwoWay = false;
    WorldId_t LeftWorld;
    Pos::GlobalRegion LeftPos;
    WorldId_t RightWorld;
    Pos::GlobalRegion RightPos;
};

struct ContentViewCircle {
    WorldId_t WorldId;
    Pos::GlobalRegion Pos;
    // Радиус в регионах в квадрате
    int16_t Range;
};


/* Игрок */
class ContentEventController {
private:
    struct SubscribedObj {
        // Используется регионами
        std::vector<PortalId_t> Portals;
    } Subscribed;

public:
    // Управляется сервером
    std::unique_ptr<RemoteClient> Remote;
    // Что сейчас наблюдает игрок
    ContentViewInfo ContentViewState;
    // Если игрок пересекал границы чанка (для перерасчёта ContentViewState)
    bool CrossedBorder = true;

    ServerObjectPos Pos, LastPos;
    std::queue<Pos::GlobalNode> Build, Break;

public:
    ContentEventController(std::unique_ptr<RemoteClient>&& remote);

    // Измеряется в чанках в регионах (активная зона)
    static constexpr uint16_t getViewRangeActive() { return 2; }
    // Измеряется в чанках в радиусе (Декоративная зона) + getViewRangeActive()
    uint16_t getViewRangeBackground() const;
    ServerObjectPos getLastPos() const;
    ServerObjectPos getPos() const;

    // Очищает более не наблюдаемые чанки и миры
    void removeUnobservable(const ContentViewInfo_Diff& diff);
    // Здесь приходят частично фильтрованные события
    // Фильтровать не отслеживаемые миры
    void onWorldUpdate(WorldId_t worldId, World *worldObj);

    void onEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_set<RegionEntityId_t> &enter, const std::unordered_set<RegionEntityId_t> &lost);
    void onEntitySwap(ServerEntityId_t prevId, ServerEntityId_t newId);
    void onEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, const std::unordered_map<RegionEntityId_t, Entity*> &entities);

    void onPortalEnterLost(const std::vector<void*> &enter, const std::vector<void*> &lost);
    void onPortalUpdates(const std::vector<void*> &portals);

    inline const SubscribedObj& getSubscribed() { return Subscribed; };

    void onUpdate();

};

}


namespace std {

template <>
struct hash<LV::Server::ServerObjectPos> {
    std::size_t operator()(const LV::Server::ServerObjectPos& obj) const {
        return std::hash<uint32_t>()(obj.WorldId) ^ std::hash<LV::Pos::Object>()(obj.ObjectPos); 
    } 
};
}
