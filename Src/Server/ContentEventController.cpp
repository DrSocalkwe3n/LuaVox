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

uint16_t ContentEventController::getViewRangeActive() const {
    return 16;
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

void ContentEventController::checkContentViewChanges() {
    // Очистка уже не наблюдаемых чанков
    for(const auto &[worldId, regions] : ContentView_LostView.View) {
        for(const auto &[regionPos, chunks] : regions) {
            size_t bitPos = chunks._Find_first();
            while(bitPos != chunks.size()) {
                Pos::Local16_u chunkPosLocal;
                chunkPosLocal = bitPos;
                Pos::GlobalChunk chunkPos = regionPos.toChunk(chunkPosLocal);
                Remote->prepareChunkRemove(worldId, chunkPos);
                bitPos = chunks._Find_next(bitPos);
            }
        }
    }

    // Очистка миров
    for(WorldId_t worldId : ContentView_LostView.Worlds) {
        Remote->prepareWorldRemove(worldId);
    }
}

void ContentEventController::onWorldUpdate(WorldId_t worldId, World *worldObj)
{
    auto pWorld = ContentViewState.find(worldId);
    if(pWorld == ContentViewState.end())
        return;

    Remote->prepareWorldUpdate(worldId, worldObj);
}

void ContentEventController::onChunksUpdate_Voxels(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const std::vector<VoxelCube>*> &chunks)
{
    auto pWorld = ContentViewState.find(worldId);
    if(pWorld == ContentViewState.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    const std::bitset<4096> &chunkBitset = pRegion->second;

    for(auto pChunk : chunks) {
        if(!chunkBitset.test(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos = regionPos.toChunk(pChunk.first);
        Remote->prepareChunkUpdate_Voxels(worldId, chunkPos, *pChunk.second);
    }
}

void ContentEventController::onChunksUpdate_Nodes(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const std::unordered_map<Pos::Local16_u, Node>*> &chunks)
{
    auto pWorld = ContentViewState.find(worldId);
    if(pWorld == ContentViewState.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    const std::bitset<4096> &chunkBitset = pRegion->second;

    for(auto pChunk : chunks) {
        if(!chunkBitset.test(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos = regionPos.toChunk(pChunk.first);
        Remote->prepareChunkUpdate_Nodes(worldId, chunkPos, *pChunk.second);
    }
}

void ContentEventController::onChunksUpdate_LightPrism(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    const std::unordered_map<Pos::Local16_u, const LightPrism*> &chunks)
{
    auto pWorld = ContentViewState.find(worldId);
    if(pWorld == ContentViewState.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    const std::bitset<4096> &chunkBitset = pRegion->second;

    for(auto pChunk : chunks) {
        if(!chunkBitset.test(pChunk.first))
            continue;

        Pos::GlobalChunk chunkPos = regionPos.toChunk(pChunk.first);
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