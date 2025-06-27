#pragma once

#include <cstdint>
#include <Common/Abstract.hpp>
#include <Common/Collide.hpp>
#include <boost/uuid/detail/sha1.hpp>


namespace LV::Server {

// В одном регионе может быть максимум 2^16 сущностей. Клиенту адресуются сущности в формате <мир>+<позиция региона>+<uint16_t>
// И если сущность перешла из одного региона в другой, идентификатор сущности на стороне клиента сохраняется
using EntityId_t = uint16_t;
using FuncEntityId_t = uint16_t;
using ClientEntityId_t = std::tuple<WorldId_t, Pos::GlobalRegion, EntityId_t>;

using MediaStreamId_t = uint16_t;
using ContentBridgeId_t = ResourceId_t;
using PlayerId_t = ResourceId_t;


/*
    Сервер загружает информацию о локальных текстурах
    Пересмотр списка текстур?
    Динамичные текстуры?

*/

struct ResourceFile {
    using Hash_t = boost::uuids::detail::sha1::digest_type;

    Hash_t Hash;
    std::vector<std::byte> Data;

    void calcHash() {
        boost::uuids::detail::sha1 hash;
        hash.process_bytes(Data.data(), Data.size());
        hash.get_digest(Hash);
    }
};

struct ServerTime {
    uint32_t Seconds : 24, Sub : 8;
};

struct VoxelCube {
    DefVoxelId_t VoxelId;
    Pos::bvec256u Left, Right;

    auto operator<=>(const VoxelCube&) const = default;
};

struct VoxelCube_Region {
    Pos::bvec4096u Left, Right;
    DefVoxelId_t VoxelId;

    auto operator<=>(const VoxelCube_Region&) const = default;
};

struct Node {
    DefNodeId_t NodeId;
    uint8_t Rotate : 6;
};


struct AABB {
    Pos::Object VecMin, VecMax;

    void sortMinMax() {
        Pos::Object::Type left, right;

        for(int iter = 0; iter < 3; iter++) {
            left = std::min(VecMin[iter], VecMax[iter]);
            right = std::max(VecMin[iter], VecMax[iter]);
            VecMin.set(iter, left);
            VecMax.set(iter, right);
        }
    }

    bool isCollideWith(const AABB &other, bool axis[3] = nullptr) {
        return calcBoxToBoxCollide(VecMin, VecMax, other.VecMin, other.VecMax, axis);
    }

    bool collideWithDelta(const AABB &other, const Pos::Object &my_speed, int32_t &delta, bool axis[3] = nullptr) {
        return calcBoxToBoxCollideWithDelta(VecMin, VecMax, other.VecMin, other.VecMax, my_speed, &delta, Pos::Object_t::BS, axis);
    }
};

struct LocalAABB {
    uint64_t x : 20, y : 20, z : 20;

    AABB atPos(const Pos::Object &pos) const {
        return {pos-Pos::Object(x/2, y/2, z/2), pos+Pos::Object(x/2, y/2, z/2)};
    }
};

struct CollisionAABB : public AABB {
    enum struct EnumType {
        Voxel, Node, Entity, Barrier, Portal, Another
    } Type;

    union {
        struct {
            EntityId_t Index;
        } Entity;

        struct {
            FuncEntityId_t Index;
        } FuncEntity;

        struct {
            Pos::bvec16u Pos;
        } Node;

        struct {
            Pos::bvec16u Chunk;
            uint32_t Index;
            DefVoxelId_t Id;
        } Voxel;

        struct {

        } Barrier;

        struct {

        } Portal;

        struct {

        } Another;
    };

    bool Skip = false;
};


class Entity  {
    DefEntityId_t DefId;

public:
    LocalAABB ABBOX;

    // PosQuat
    DefWorldId_t WorldId;
    Pos::Object Pos, Speed, Acceleration;
    glm::quat Quat;
    static constexpr uint16_t HP_BS = 4096, HP_BS_Bit = 12;
    uint32_t HP = 0;

    Pos::GlobalRegion InRegionPos;

    // State
    std::unordered_map<std::string, float> Tags;
    // m_attached_particle_spawners
    // states

    bool 
        // Сущность будет удалена в слудующем такте
        NeedRemove = false, 
        // Сущность была удалена или не действительна
        IsRemoved = false;

public:
    Entity(DefEntityId_t defId);
    
    AABB aabbAtPos() {
        return {Pos-Pos::Object(ABBOX.x/2, ABBOX.y/2, ABBOX.z/2), Pos+Pos::Object(ABBOX.x/2, ABBOX.y/2, ABBOX.z/2)};
    }

    DefEntityId_t getDefId() const { return DefId; }
};


template<typename Vec>
struct VoxelCuboidsFuncs {

    // Кубы должны быть отсортированы
    static bool canMerge(const Vec& a, const Vec& b) {
        if (a.VoxelId != b.VoxelId) return false;

        // Проверяем, что кубы смежны по одной из осей
        bool xAdjacent = (a.Right.x == b.Left.x) && (a.Left.y == b.Left.y) && (a.Right.z == b.Right.z) && (a.Left.z == b.Left.z) && (a.Right.z == b.Right.z);
        bool yAdjacent = (a.Right.y == b.Left.y) && (a.Left.x == b.Left.x) && (a.Right.x == b.Right.x) && (a.Left.z == b.Left.z) && (a.Right.z == b.Right.z);
        bool zAdjacent = (a.Right.z == b.Left.z) && (a.Left.x == b.Left.x) && (a.Right.x == b.Right.x) && (a.Left.y == b.Left.y) && (a.Right.y == b.Right.y);

        return xAdjacent || yAdjacent || zAdjacent;
    }

    static Vec mergeCubes(const Vec& a, const Vec& b) {
        Vec merged;
        merged.VoxelId = a.VoxelId;

        // Объединяем кубы по минимальным и максимальным координатам
        merged.Left.x = std::min(a.Left.x, b.Left.x);
        merged.Left.y = std::min(a.Left.y, b.Left.y);
        merged.Left.z = std::min(a.Left.z, b.Left.z);

        merged.Right.x = std::max(a.Right.x, b.Right.x);
        merged.Right.y = std::max(a.Right.y, b.Right.y);
        merged.Right.z = std::max(a.Right.z, b.Right.z);

        return merged;
    }

    static std::vector<Vec> optimizeVoxelRegions(std::vector<Vec> regions) {
        bool changed;
        do {
            changed = false;
            for (size_t i = 0; i < regions.size(); ++i) {
                for (size_t j = i + 1; j < regions.size(); ++j) {
                    if (canMerge(regions[i], regions[j])) {
                        regions[i] = mergeCubes(regions[i], regions[j]);
                        regions.erase(regions.begin() + j);
                        changed = true;
                        --j;
                    }
                }
            }
        } while (changed);

        return regions;
    }

    static bool isCubesIntersect(const Vec& a, const Vec& b) {
        return !(a.Right.X < b.Left.X || a.Left.X > b.Right.X ||
                a.Right.Y < b.Left.Y || a.Left.Y > b.Right.Y ||
                a.Right.Z < b.Left.Z || a.Left.Z > b.Right.Z);
    }

    static std::vector<Vec> subtractCube(const Vec& a, const Vec& b) {
        std::vector<Vec> result;

        if (!isCubesIntersect(a, b)) {
            result.push_back(a);
            return result;
        }

        decltype(a.Left) intersectLeft = {
            std::max(a.Left.X, b.Left.X),
            std::max(a.Left.Y, b.Left.Y),
            std::max(a.Left.Z, b.Left.Z)
        };
        decltype(a.Left) intersectRight = {
            std::min(a.Right.X, b.Right.X),
            std::min(a.Right.Y, b.Right.Y),
            std::min(a.Right.Z, b.Right.Z)
        };

        // Разделяем куб a на меньшие кубы, исключая пересечение
        if (a.Left.X < intersectLeft.X) {
            result.push_back({a.Left, decltype(a.Left)(intersectLeft.X - 1, a.Right.Y, a.Right.Z), a.Material});
        }

        if (a.Right.X > intersectRight.X) {
            result.push_back({decltype(a.Left)(intersectRight.X + 1, a.Left.Y, a.Left.Z), a.Right, a.Material});
        }

        if (a.Left.Y < intersectLeft.Y) {
            result.push_back({
                {intersectLeft.X, a.Left.Y, a.Left.Z},
                decltype(a.Left)(intersectRight.X, intersectLeft.Y - 1, a.Right.Z),
                a.Material
            });
        }

        if (a.Right.Y > intersectRight.Y) {
            result.push_back({
                decltype(a.Left)(intersectLeft.X, intersectRight.Y + 1, a.Left.Z),
                {intersectRight.X, a.Right.Y, a.Right.Z},
                a.Material
            });
        }

        if (a.Left.Z < intersectLeft.Z) {
            result.push_back({
                {intersectLeft.X, intersectLeft.Y, a.Left.Z},
                decltype(a.Left)(intersectRight.X, intersectRight.Y, intersectLeft.Z - 1),
                a.Material
            });
        }

        if (a.Right.Z > intersectRight.Z) {
            result.push_back({
                decltype(a.Left)(intersectLeft.X, intersectLeft.Y, intersectRight.Z + 1),
                {intersectRight.X, intersectRight.Y, a.Right.Z},
                a.Material
            });
        }

        return result;
    }
};

inline void convertRegionVoxelsToChunks(const std::vector<VoxelCube_Region>& regions, std::vector<VoxelCube> *chunks) {
    for (const auto& region : regions) {
        int minX = region.Left.x >> 8;
        int minY = region.Left.y >> 8;
        int minZ = region.Left.z >> 8;
        int maxX = region.Right.x >> 8;
        int maxY = region.Right.y >> 8;
        int maxZ = region.Right.z >> 8;

        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    Pos::bvec256u left {
                        static_cast<uint8_t>(std::max<uint16_t>((x << 8), region.Left.x) - (x << 8)),
                        static_cast<uint8_t>(std::max<uint16_t>((y << 8), region.Left.y) - (y << 8)),
                        static_cast<uint8_t>(std::max<uint16_t>((z << 8), region.Left.z) - (z << 8))
                    };
                    Pos::bvec256u right {
                        static_cast<uint8_t>(std::min<uint16_t>(((x+1) << 8)-1, region.Right.x) - (x << 8)),
                        static_cast<uint8_t>(std::min<uint16_t>(((y+1) << 8)-1, region.Right.y) - (y << 8)),
                        static_cast<uint8_t>(std::min<uint16_t>(((z+1) << 8)-1, region.Right.z) - (z << 8))
                    };

                    int chunkIndex = z * 16 * 16 + y * 16 + x;
                    chunks[chunkIndex].emplace_back(region.VoxelId, left, right);
                }
            }
        }
    }
}

inline void convertChunkVoxelsToRegion(const std::vector<VoxelCube> *chunks, std::vector<VoxelCube_Region> &regions) {
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            for (int z = 0; z < 16; ++z) {
                int chunkIndex = z * 16 * 16 + y * 16 + x;

                Pos::bvec4096u left(x << 8, y << 8, z << 8);
                
                for (const auto& cube : chunks[chunkIndex]) {
                    regions.emplace_back(
                        Pos::bvec4096u(left.x+cube.Left.x, left.y+cube.Left.y, left.z+cube.Left.z),
                        Pos::bvec4096u(left.x+cube.Right.x, left.y+cube.Right.y, left.z+cube.Right.z), 
                        cube.VoxelId
                    );
                }
            }
        }
    }

    std::sort(regions.begin(), regions.end());
    regions = VoxelCuboidsFuncs<VoxelCube_Region>::optimizeVoxelRegions(regions);
}

}