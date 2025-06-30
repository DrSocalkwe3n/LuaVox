#include "ContentEventController.hpp"
#include "Common/Abstract.hpp"
#include "RemoteClient.hpp"
#include "Server/Abstract.hpp"
#include "World.hpp"
#include <algorithm>


namespace LV::Server {

ContentEventController::ContentEventController(std::unique_ptr<RemoteClient> &&remote)
    : Remote(std::move(remote))
{
}

uint16_t ContentEventController::getViewRangeBackground() const {
    return 0;
}

ServerObjectPos ContentEventController::getLastPos() const {
    return {0, Remote->CameraPos};
}

ServerObjectPos ContentEventController::getPos() const {
    return {0, Remote->CameraPos};
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

void ContentEventController::onChunksUpdate_Voxels(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*>& chunks)
{
    auto pWorld = ContentViewState.Regions.find(worldId);
    if(pWorld == ContentViewState.Regions.end())
        return;

    if(!std::binary_search(pWorld->second.begin(), pWorld->second.end(), regionPos))
        return;

    for(auto pChunk : chunks) {
        Pos::GlobalChunk chunkPos = (Pos::GlobalChunk(regionPos) << 2) + pChunk.first;
        Remote->prepareChunkUpdate_Voxels(worldId, chunkPos, pChunk.second);
    }
}

void ContentEventController::onChunksUpdate_Nodes(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::bvec4u, const Node*> &chunks)
{
    auto pWorld = ContentViewState.Regions.find(worldId);
    if(pWorld == ContentViewState.Regions.end())
        return;

    if(!std::binary_search(pWorld->second.begin(), pWorld->second.end(), regionPos))
        return;

    for(auto pChunk : chunks) {
        Pos::GlobalChunk chunkPos = (Pos::GlobalChunk(regionPos) << 2) + Pos::GlobalChunk(pChunk.first);
        Remote->prepareChunkUpdate_Nodes(worldId, chunkPos, pChunk.second);
    }
}

// void ContentEventController::onChunksUpdate_LightPrism(WorldId_t worldId, Pos::GlobalRegion regionPos, 
//     const std::unordered_map<Pos::bvec4u, const LightPrism*> &chunks)
// {
//     auto pWorld = ContentViewState.find(worldId);
//     if(pWorld == ContentViewState.end())
//         return;

//     auto pRegion = pWorld->second.find(regionPos);
//     if(pRegion == pWorld->second.end())
//         return;

//     const std::bitset<4096> &chunkBitset = pRegion->second;

//     for(auto pChunk : chunks) {
//         if(!chunkBitset.test(pChunk.first))
//             continue;

//         Pos::GlobalChunk chunkPos = regionPos.toChunk(pChunk.first);
//         Remote->prepareChunkUpdate_LightPrism(worldId, chunkPos, pChunk.second);
//     }
// }

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

}