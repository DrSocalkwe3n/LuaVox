#include "World.hpp"
#include "TOSLib.hpp"
#include <memory>


namespace LV::Server {


World::World(DefWorldId defId)
    : DefId(defId)
{

}

World::~World() {

}

std::vector<Pos::GlobalRegion> World::onRemoteClient_RegionsEnter(std::shared_ptr<RemoteClient> cec, const std::vector<Pos::GlobalRegion>& enter) {
    std::vector<Pos::GlobalRegion> out;

    for(const Pos::GlobalRegion &pos : enter) {
        auto iterRegion = Regions.find(pos);
        if(iterRegion == Regions.end()) {
            out.push_back(pos);
            continue;
        }

        auto &region = *iterRegion->second;
        region.RMs.push_back(cec);
        region.NewRMs.push_back(cec);
        // Отправить клиенту информацию о чанках и сущностях
        std::unordered_map<Pos::bvec4u, const std::vector<VoxelCube>*> voxels;
        std::unordered_map<Pos::bvec4u, const Node*> nodes;

        for(auto& [key, value] : region.Voxels) {
            voxels[key] = &value;
        }

        for(int z = 0; z < 4; z++)
            for(int y = 0; y < 4; y++)
                for(int x = 0; x < 4; x++) {
                    nodes[Pos::bvec4u(x, y, z)] = region.Nodes[Pos::bvec4u(x, y, z).pack()].data();
                }

        
    }

    return out;
}

void World::onRemoteClient_RegionsLost(std::shared_ptr<RemoteClient> cec, const std::vector<Pos::GlobalRegion> &lost) {
    for(const Pos::GlobalRegion &pos : lost) {
        auto region = Regions.find(pos);
        if(region == Regions.end())
            continue;

        std::vector<std::shared_ptr<RemoteClient>> &CECs = region->second->RMs;
        for(size_t iter = 0; iter < CECs.size(); iter++) {
            if(CECs[iter] == cec) {
                CECs.erase(CECs.begin()+iter);
                break;
            }
        }
    }
}

World::SaveUnloadInfo World::onStepDatabaseSync() {
    return {};
}

void World::pushRegions(std::vector<std::pair<Pos::GlobalRegion, RegionIn>> regions) {
    for(auto& [key, value] : regions) {
        Region &region = *(Regions[key] = std::make_unique<Region>());
        region.Voxels = std::move(value.Voxels);
        region.Nodes = value.Nodes;
    }
}

void World::onUpdate(GameServer *server, float dtime) {
    
}

}