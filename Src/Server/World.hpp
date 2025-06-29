#pragma once

#include "Common/Abstract.hpp"
#include "Server/Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include "Server/SaveBackend.hpp"
#include <memory>
#include <unordered_map>
#include <vector>


namespace LV::Server {

class GameServer;

class Region {
public:
    uint64_t IsChunkChanged_Voxels = 0;
    uint64_t IsChunkChanged_Nodes = 0;
    bool IsChanged = false; // Изменён ли был регион, относительно последнего сохранения
    std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> Voxels;
    // x y cx cy cz
    //LightPrism Lights[16][16][4][4][4];

    Node Nodes[16][16][16][4][4][4];

    std::vector<Entity> Entityes;
    std::vector<FuncEntity> FuncEntityes;
    std::vector<ContentEventController*> CECs;
    // Используется для прорежения количества проверок на наблюдаемые чанки и сущности
    // В одно обновление региона - проверка одного наблюдателя
    uint16_t CEC_NextChunkAndEntityesViewCheck = 0;

    bool IsLoaded = false;
    float LastSaveTime = 0;

    void getCollideBoxes(Pos::GlobalRegion rPos, AABB aabb, std::vector<CollisionAABB> &boxes) {
        // Абсолютная позиция начала региона
        Pos::Object raPos = Pos::Object(rPos) << Pos::Object_t::BS_Bit;

        // Бокс региона
        AABB regionAABB(raPos, raPos+Pos::Object(Pos::Object_t::BS*64));

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
            // Определяем с какими чанками есть пересечения
            glm::ivec3 beg, end;
            for(int axis = 0; axis < 3; axis++)
                beg[axis] = std::max(aabb.VecMin[axis], regionAABB.VecMin[axis]) >> 12 >> 4;
            for(int axis = 0; axis < 3; axis++)
                end[axis] = (std::min(aabb.VecMax[axis], regionAABB.VecMax[axis])+0xffff) >> 12 >> 4;

            for(; beg.z <= end.z; beg.z++)
            for(; beg.y <= end.y; beg.y++)
            for(; beg.x <= end.x; beg.x++) {
                auto iterVoxels = Voxels.find(Pos::bvec4u(beg));

                if(iterVoxels == Voxels.end() && iterVoxels->second.empty())
                    continue;

                auto &voxels = iterVoxels->second;

                CollisionAABB aabbInfo = CollisionAABB(regionAABB);
                for(int axis = 0; axis < 3; axis++)
                    aabbInfo.VecMin.set(axis, aabbInfo.VecMin[axis] | beg[axis] << 16);

                for(size_t iter = 0; iter < voxels.size(); iter++) {
                    VoxelCube &cube = voxels[iter];

                    for(int axis = 0; axis < 3; axis++)
                        aabbInfo.VecMin.set(axis, aabbInfo.VecMin[axis] & ~0xff00);
                    aabbInfo.VecMax = aabbInfo.VecMin;

                    aabbInfo.VecMin.x |= int(cube.Left.x) << 8;
                    aabbInfo.VecMin.y |= int(cube.Left.y) << 8;
                    aabbInfo.VecMin.z |= int(cube.Left.z) << 8;

                    aabbInfo.VecMax.x |= int(cube.Right.x) << 8;
                    aabbInfo.VecMax.y |= int(cube.Right.y) << 8;
                    aabbInfo.VecMax.z |= int(cube.Right.z) << 8;

                    if(aabb.isCollideWith(aabbInfo)) {
                        aabbInfo = {
                            .Type = CollisionAABB::EnumType::Voxel,
                            .Voxel = {
                                .Chunk = Pos::bvec4u(beg.x, beg.y, beg.z),
                                .Index = static_cast<uint32_t>(iter),
                            }
                        };

                        boxes.push_back(aabbInfo);
                    }
                }
            }
        }

        // Собираем коробки нод

    }

    RegionEntityId_t pushEntity(Entity &entity) {
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

        // В регионе не осталось места
        return RegionEntityId_t(-1);
    }

    void load(SB_Region *data) {
        convertRegionVoxelsToChunks(data->Voxels, Voxels);
    }

    void save(SB_Region *data) {
        data->Voxels.clear();
        convertChunkVoxelsToRegion(Voxels, data->Voxels);
    }
};

class World {
    DefWorldId_t DefId;

public:
    std::vector<Pos::GlobalRegion> NeedToLoad;
    std::unordered_map<Pos::GlobalRegion, std::unique_ptr<Region>> Regions;

public:
    World(DefWorldId_t defId);
    ~World();

    /*
        Обновить регионы
    */
    void onUpdate(GameServer *server, float dtime);

    // Игрок начал отслеживать регионы
    void onCEC_RegionsEnter(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &enter);
    void onCEC_RegionsLost(ContentEventController *cec, const std::vector<Pos::GlobalRegion> &lost);

    DefWorldId_t getDefId() const { return DefId; }
};



}