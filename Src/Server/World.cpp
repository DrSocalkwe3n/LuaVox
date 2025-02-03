#include "World.hpp"


namespace AL::Server {


World::World(WorldId_t id) {

}

World::~World() {

}

void World::onUpdate(GameServer *server, float dtime) {
    
}

void World::onCEC_RegionsEnter(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &enter) {
    for(const Pos::GlobalRegion &pos : enter) {
        std::unique_ptr<Region> &region = Regions[pos];
        if(!region) {
            region = std::make_unique<Region>();
            NeedToLoad.push_back(pos);
        }

        region->CECs.push_back(cec);
    }
}

void World::onCEC_RegionsLost(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &lost) {
    for(const Pos::GlobalRegion &pos : lost) {
        auto region = Regions.find(pos);
        if(region == Regions.end())
            continue;

        std::vector<ContentEventController*> &CECs = region->second->CECs;
        for(size_t iter = 0; iter < CECs.size(); iter++) {
            if(CECs[iter] == cec) {
                CECs.erase(CECs.begin()+iter);
                break;
            }
        }
    }
}

Region* World::forceLoadOrGetRegion(Pos::GlobalRegion pos) {
    std::unique_ptr<Region> &region = Regions[pos];
    if(!region)
        region = std::make_unique<Region>();

    if(!region->IsLoaded) {
        region->IsLoaded = true;
    }

    return region.get();
}


}