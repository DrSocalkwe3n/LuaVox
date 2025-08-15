#include "ContentEventController.hpp"
#include "Common/Abstract.hpp"
#include "RemoteClient.hpp"
#include "Server/Abstract.hpp"
#include "World.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include <algorithm>


namespace LV::Server {

ContentEventController::ContentEventController(std::unique_ptr<RemoteClient> &&remote)
    : Remote(std::move(remote))
{
    LastPos = Pos = {0, Remote->CameraPos};
}

uint16_t ContentEventController::getViewRangeBackground() const {
    return 0;
}

ServerObjectPos ContentEventController::getLastPos() const {
    return LastPos;
}

ServerObjectPos ContentEventController::getPos() const {
    return Pos;
}

void ContentEventController::removeUnobservable(const ContentViewInfo_Diff& diff) {
    for(const auto& [worldId, regions] : diff.RegionsLost) {
        for(const Pos::GlobalRegion region : regions)
            Remote->prepareRegionRemove(worldId, region);
    }

    for(const WorldId_t worldId : diff.WorldsLost)
        Remote->prepareWorldRemove(worldId);
}

void ContentEventController::onWorldUpdate(WorldId_t worldId, World *worldObj)
{
    auto pWorld = ContentViewState.Regions.find(worldId);
    if(pWorld == ContentViewState.Regions.end())
        return;

    Remote->prepareWorldUpdate(worldId, worldObj);
}

void ContentEventController::onEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_set<RegionEntityId_t> &enter, const std::unordered_set<RegionEntityId_t> &lost)
{
    // Сообщить Remote
    for(RegionEntityId_t eId : lost) {
        Remote->prepareEntityRemove({worldId, regionPos, eId});
    }
}

void ContentEventController::onEntitySwap(ServerEntityId_t prevId, ServerEntityId_t newId)
{
    {
        auto pWorld = ContentViewState.Regions.find(std::get<0>(prevId));
        assert(pWorld != ContentViewState.Regions.end());
        assert(std::binary_search(pWorld->second.begin(), pWorld->second.end(), std::get<1>(prevId)));
    }

    {
        auto npWorld = ContentViewState.Regions.find(std::get<0>(newId));
        assert(npWorld != ContentViewState.Regions.end());
        assert(std::binary_search(npWorld->second.begin(), npWorld->second.end(), std::get<1>(prevId)));
    }

    Remote->prepareEntitySwap(prevId, newId);
}

void ContentEventController::onEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<RegionEntityId_t, Entity*> &entities)
{
    auto pWorld = ContentViewState.Regions.find(worldId);
    if(pWorld == ContentViewState.Regions.end())
        // Исходный мир нами не отслеживается
        return;

    if(!std::binary_search(pWorld->second.begin(), pWorld->second.end(), regionPos))
        // Исходный регион нами не отслеживается
        return;

    for(const auto& [id, entity] : entities) {
        Remote->prepareEntityUpdate({worldId, regionPos, id}, entity);
    }
}

void ContentEventController::onPortalEnterLost(const std::vector<void*> &enter, const std::vector<void*> &lost)
{

}

void ContentEventController::onPortalUpdates(const std::vector<void*> &portals)
{

}

void ContentEventController::onUpdate() {
    LastPos = Pos;
    Pos.ObjectPos = Remote->CameraPos;

    Pos::GlobalRegion r1 = LastPos.ObjectPos >> 12 >> 4 >> 2;
    Pos::GlobalRegion r2 = Pos.ObjectPos >> 12 >> 4 >> 2;
    if(r1 != r2) {
        CrossedBorder = true;
    }

    if(!Remote->Actions.get_read().empty()) {
        auto lock = Remote->Actions.lock();
        while(!lock->empty()) {
            uint8_t action = lock->front();
            lock->pop();

            glm::quat q = Remote->CameraQuat.toQuat();
            glm::vec4 v = glm::mat4(q)*glm::vec4(0, 0, -6, 1);
            Pos::GlobalNode pos = (Pos::GlobalNode) (glm::vec3) v;
            pos += Pos.ObjectPos >> Pos::Object_t::BS_Bit;

            if(action == 0) {
                // Break
                Break.push(pos);

            } else if(action == 1) {
                // Build
                Build.push(pos);
            }
        }
    }
}

}