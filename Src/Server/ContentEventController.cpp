#include "ContentEventController.hpp"
#include "Common/Abstract.hpp"
#include "RemoteClient.hpp"
#include "Server/Abstract.hpp"
#include "World.hpp"


namespace LV::Server {

ContentEventController::ContentEventController(std::unique_ptr<RemoteClient> &&remote)
    : Remote(std::move(remote))
{
}

uint8_t ContentEventController::getViewRange() const {
    return 3;
}

ServerObjectPos ContentEventController::getLastPos() const {
    return {0, {0, 0, 0}};
}

ServerObjectPos ContentEventController::getPos() const {
    return {0, {0, 0, 0}};
}

void ContentEventController::onRegionsLost(WorldId_t worldId, const std::vector<Pos::GlobalRegion> &lost) {
    auto pWorld = Subscribed.Chunks.find(worldId);
    if(pWorld == Subscribed.Chunks.end())
        return;

    for(Pos::GlobalRegion rPos : lost) {
        auto pRegion = pWorld->second.find(rPos);
        if(pRegion != pWorld->second.end()) {
            for(Pos::Local16_u lChunkPos : pRegion->second) {
                Pos::GlobalChunk gChunkPos(
                    (pRegion->first.X << 4) | lChunkPos.X,
                    (pRegion->first.Y << 4) | lChunkPos.Y,
                    (pRegion->first.Z << 4) | lChunkPos.Z
                );

                Remote->prepareChunkRemove(worldId, gChunkPos);
            }

            pWorld->second.erase(pRegion);
        }
    }

    if(pWorld->second.empty()) {
        Subscribed.Chunks.erase(pWorld);
        Remote->prepareWorldRemove(worldId);
    }
}

void ContentEventController::onChunksEnterLost(WorldId_t worldId, World *worldObj, Pos::GlobalRegion regionPos, const std::unordered_set<Pos::Local16_u> &enter, const std::unordered_set<Pos::Local16_u> &lost) {    
    if(!Subscribed.Chunks.contains(worldId)) {
        Remote->prepareWorldNew(worldId, worldObj);
    }
    
    std::unordered_set<Pos::Local16_u> &chunks = Subscribed.Chunks[worldId][regionPos];
    
    chunks.insert(enter.begin(), enter.end());

    for(Pos::Local16_u cPos : lost) {
        chunks.erase(cPos);

        Pos::GlobalChunk chunkPos(
            (regionPos.X << 4) | cPos.X,
            (regionPos.Y << 4) | cPos.Y,
            (regionPos.Z << 4) | cPos.Z
        );

        Remote->prepareChunkRemove(worldId, chunkPos);
    }

    if(Subscribed.Chunks[worldId].empty()) {
        Subscribed.Chunks.erase(Subscribed.Chunks.find(worldId));
        Remote->prepareWorldRemove(worldId);
    }
}

void ContentEventController::onChunksUpdate_Voxels(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> &chunks)
{
    auto pWorld = Subscribed.Chunks.find(worldId);
    if(pWorld == Subscribed.Chunks.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    for(auto pChunk : chunks) {
        if(!pRegion->second.contains(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos(
            (regionPos.X << 4) | pChunk.first.X,
            (regionPos.Y << 4) | pChunk.first.Y,
            (regionPos.Z << 4) | pChunk.first.Z
        );

        Remote->prepareChunkUpdate_Voxels(worldId, chunkPos, *pChunk.second);
    }
}

void ContentEventController::onChunksUpdate_Nodes(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> &chunks)
{
    auto pWorld = Subscribed.Chunks.find(worldId);
    if(pWorld == Subscribed.Chunks.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    for(auto pChunk : chunks) {
        if(!pRegion->second.contains(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos(
            (regionPos.X << 4) | pChunk.first.X,
            (regionPos.Y << 4) | pChunk.first.Y,
            (regionPos.Z << 4) | pChunk.first.Z
        );

        Remote->prepareChunkUpdate_Nodes(worldId, chunkPos, *pChunk.second);
    }
}

void ContentEventController::onChunksUpdate_LightPrism(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const LightPrism*> &chunks)
{
    auto pWorld = Subscribed.Chunks.find(worldId);
    if(pWorld == Subscribed.Chunks.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    for(auto pChunk : chunks) {
        if(!pRegion->second.contains(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos(
            (regionPos.X << 4) | pChunk.first.X,
            (regionPos.Y << 4) | pChunk.first.Y,
            (regionPos.Z << 4) | pChunk.first.Z
        );

        Remote->prepareChunkUpdate_LightPrism(worldId, chunkPos, pChunk.second);
    }
}

void ContentEventController::onEntityEnterLost(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_set<LocalEntityId_t> &enter, const std::unordered_set<LocalEntityId_t> &lost)
{
    auto pWorld = Subscribed.Entities.find(worldId);
    if(pWorld == Subscribed.Entities.end()) {
        // pWorld = std::get<0>(Subscribed.Entities.emplace(std::make_pair(worldId, decltype(Subscribed.Entities)::value_type())));
        Subscribed.Entities[worldId];
        pWorld = Subscribed.Entities.find(worldId);
    }

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end()) {
        // pRegion = std::get<0>(pWorld->second.emplace(std::make_pair(worldId, decltype(pWorld->second)::value_type())));
        pWorld->second[regionPos];
        pRegion = pWorld->second.find(regionPos);
    }

    std::unordered_set<LocalEntityId_t> &entityesId = pRegion->second;

    for(LocalEntityId_t eId : lost) {
        entityesId.erase(eId);
    }
    
    entityesId.insert(enter.begin(), enter.end());

    if(entityesId.empty()) {
        pWorld->second.erase(pRegion);

        if(pWorld->second.empty())
            Subscribed.Entities.erase(pWorld);
    }

    // Сообщить Remote
    for(LocalEntityId_t eId : lost) {
        Remote->prepareEntityRemove({worldId, regionPos, eId});
    }
}

void ContentEventController::onEntitySwap(WorldId_t lastWorldId, Pos::GlobalRegion lastRegionPos, 
    LocalEntityId_t lastId, WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, LocalEntityId_t newId)
{
    // Проверим отслеживается ли эта сущность нами
    auto lpWorld = Subscribed.Entities.find(lastWorldId);
    if(lpWorld == Subscribed.Entities.end())
        // Исходный мир нами не отслеживается
        return;

    auto lpRegion = lpWorld->second.find(lastRegionPos);
    if(lpRegion == lpWorld->second.end())
        // Исходный регион нами не отслеживается
        return;

    auto lpceId = lpRegion->second.find(lastId);
    if(lpceId == lpRegion->second.end())
        // Сущность нами не отслеживается
        return;

    // Проверим отслеживается ли регион, в который будет перемещена сущность
    auto npWorld = Subscribed.Entities.find(newWorldId);
    if(npWorld != Subscribed.Entities.end()) {
        auto npRegion = npWorld->second.find(newRegionPos);
        if(npRegion != npWorld->second.end()) {
            // Следующий регион отслеживается, перекинем сущность
            lpRegion->second.erase(lpceId);
            npRegion->second.insert(newId);

            Remote->prepareEntitySwap({lastWorldId, lastRegionPos, lastId}, {newWorldId, newRegionPos, newId});

            goto entitySwaped;
        }
    }

    Remote->prepareEntityRemove({lastWorldId, lastRegionPos, lastId});

    entitySwaped:
    return;
}

void ContentEventController::onEntityUpdates(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::vector<Entity> &entities)
{
    auto lpWorld = Subscribed.Entities.find(worldId);
    if(lpWorld == Subscribed.Entities.end())
        // Исходный мир нами не отслеживается
        return;

    auto lpRegion = lpWorld->second.find(regionPos);
    if(lpRegion == lpWorld->second.end())
        // Исходный регион нами не отслеживается
        return;

    for(size_t eId = 0; eId < entities.size(); eId++) {
        if(!lpRegion->second.contains(eId))
            continue;

        Remote->prepareEntityUpdate({worldId, regionPos, eId}, &entities[eId]);
    }
}

void ContentEventController::onPortalEnterLost(const std::vector<void*> &enter, const std::vector<void*> &lost)
{

}

void ContentEventController::onPortalUpdates(const std::vector<void*> &portals)
{

}

}