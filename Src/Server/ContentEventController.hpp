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


/* Игрок */
class ContentEventController {
public:
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


