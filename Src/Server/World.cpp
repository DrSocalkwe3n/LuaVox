#include "World.hpp"


namespace LV::Server {


World::World(DefWorldId_t defId)
    : DefId(defId)
{

}

World::~World() {

}

void World::onUpdate(GameServer *server, float dtime) {
    
}

std::vector<Pos::GlobalRegion> World::onCEC_RegionsEnter(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &enter) {
    std::vector<Pos::GlobalRegion> out;
    
    for(const Pos::GlobalRegion &pos : enter) {
        auto iterRegion = Regions.find(pos);
        if(iterRegion == Regions.end()) {
            out.push_back(pos);
        }

        iterRegion->second->CECs.push_back(cec);
        // Отправить клиенту информацию о чанках и сущностях
    }

    return out;
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

}