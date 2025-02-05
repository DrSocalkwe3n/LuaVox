#pragma once

#include "Common/Abstract.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include <memory>
#include <unordered_map>


namespace LV::Server {

class GameServer;

class Region {
public:
    uint64_t IsChunkChanged_Voxels[64] = {0};
    uint64_t IsChunkChanged_Nodes[64] = {0};
    bool IsChanged = false;
    // cx cy cz
    std::vector<VoxelCube> Voxels[16][16][16];
    // x y cx cy cz
    LightPrism Lights[16][16][16][16][16];
    std::unordered_map<Pos::Local16_u, Node> Nodes[16][16][16];

    std::vector<Entity> Entityes;
    std::vector<ContentEventController*> CECs;

    bool IsLoaded = false;
    float LastSaveTime = 0;

    void getCollideBoxes(Pos::GlobalRegion rPos, AABB aabb, std::vector<CollisionAABB> &boxes) {
        // Абсолютная позиция начала региона
        Pos::Object raPos(rPos.X, rPos.Y, rPos.Z);
        raPos <<= Pos::Object_t::BS_Bit;

        // Бокс региона
        AABB regionAABB(raPos, raPos+Pos::Object(Pos::Object_t::BS*256));

        // Если регион не загружен, то он весь непроходим
        if(!IsLoaded) {
            boxes.emplace_back(regionAABB);
            return;
        }

        // Собираем коробки сущностей
        for(size_t iter = 0; iter < Entityes.size(); iter++) {
            Entity &entity = Entityes[iter];

            if(entity.IsRemoved)
                continue;

            CollisionAABB aabbInfo = CollisionAABB(entity.aabbAtPos());

            if(aabbInfo.isCollideWith(aabb)) {
                aabbInfo.Type = CollisionAABB::EnumType::Entity;
                aabbInfo.Entity.Index = iter;
                boxes.push_back(aabbInfo);
            }
        }

        // Собираем коробки вокселей
        if(aabb.isCollideWith(regionAABB)) {
            glm::ivec3 beg, end;
            for(int axis = 0; axis < 3; axis++)
                beg[axis] = std::max(aabb.VecMin[axis], regionAABB.VecMin[axis]) >> 16;
            for(int axis = 0; axis < 3; axis++)
                end[axis] = (std::min(aabb.VecMax[axis], regionAABB.VecMax[axis])+0xffff) >> 16;

            for(; beg.z <= end.z; beg.z++)
            for(; beg.y <= end.y; beg.y++)
            for(; beg.x <= end.x; beg.x++) {
                std::vector<VoxelCube> &voxels = Voxels[beg.x][beg.y][beg.z];

                if(voxels.empty())
                    continue;

                CollisionAABB aabbInfo = CollisionAABB(regionAABB);
                for(int axis = 0; axis < 3; axis++)
                    aabbInfo.VecMin[axis] |= beg[axis] << 16;

                for(size_t iter = 0; iter < voxels.size(); iter++) {
                    VoxelCube &cube = voxels[iter];

                    for(int axis = 0; axis < 3; axis++)
                        aabbInfo.VecMin[axis] &= ~0xff00;
                    aabbInfo.VecMax = aabbInfo.VecMin;

                    aabbInfo.VecMin.x |= int(cube.Left.X) << 8;
                    aabbInfo.VecMin.y |= int(cube.Left.Y) << 8;
                    aabbInfo.VecMin.z |= int(cube.Left.Z) << 8;

                    aabbInfo.VecMax.x |= int(cube.Right.X) << 8;
                    aabbInfo.VecMax.y |= int(cube.Right.Y) << 8;
                    aabbInfo.VecMax.z |= int(cube.Right.Z) << 8;

                    if(aabb.isCollideWith(aabbInfo)) {
                        aabbInfo = {
                            .Type = CollisionAABB::EnumType::Voxel,
                            .Voxel = {
                                .Chunk = Pos::Local16_u(beg.x, beg.y, beg.z),
                                .Index = static_cast<uint32_t>(iter),
                                .Id = cube.Material
                            }
                        };

                        boxes.push_back(aabbInfo);
                    }
                }
            }
        }

        // Собираем коробки нод

    }

    EntityId_t pushEntity(Entity &entity) {
        for(size_t iter = 0; iter < Entityes.size(); iter++) {
            Entity &obj = Entityes[iter];

            if(!obj.IsRemoved)
                continue;

            obj = std::move(entity);

            return iter;
        }

        if(Entityes.size() < 0xfffe) {
            Entityes.emplace_back(std::move(entity));
            return Entityes.size()-1;
        }

        return EntityId_t(-1);
    }
};

class World {
    WorldId_t Id;

public:
    std::vector<Pos::GlobalRegion> NeedToLoad;
    std::unordered_map<Pos::GlobalRegion, std::unique_ptr<Region>> Regions;

public:
    World(WorldId_t id);
    ~World();

    /*
        Обновить регионы
    */
    void onUpdate(GameServer *server, float dtime);

    // Игрок начал отслеживать регионы
    void onCEC_RegionsEnter(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &enter);
    void onCEC_RegionsLost(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &lost);

    Region* forceLoadOrGetRegion(Pos::GlobalRegion pos);
};



}