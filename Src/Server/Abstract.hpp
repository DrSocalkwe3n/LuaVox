#pragma once

#include <cstdint>
#include <Common/Abstract.hpp>
#include <Common/Collide.hpp>
#include <boost/uuid/detail/sha1.hpp>


namespace AL::Server {

using VoxelId_c = uint16_t;
using NodeId_c = uint16_t;
using WorldId_c = uint8_t;
using PortalId_c = uint8_t;
using EntityId_c = uint16_t;
using TextureId_c = uint16_t;
using ModelId_c = uint16_t;

using ResourceId_t = uint32_t;

using VoxelId_t = ResourceId_t;
using NodeId_t = ResourceId_t;
using WorldId_t = ResourceId_t;
using PortalId_t = uint16_t;
// В одном регионе может быть максимум 2^16 сущностей. Клиенту адресуются сущности в формате <позиция региона>+<uint16_t>
// И если сущность перешла из одного региона в другой адресация сохраняется
using EntityId_t = uint16_t;
using TextureId_t = ResourceId_t;
using ModelId_t = ResourceId_t;
using SoundId_t = ResourceId_t;
using MediaStreamId_t = uint16_t;
using ContentBridgeId_t = uint16_t;
using PlayerId_t = uint32_t;

/*
    Сервер загружает информацию о локальных текстурах
    Синхронизация часто используемых текстур?
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
    Pos::Local256_u Left, Right;
    VoxelId_t Material;

    auto operator<=>(const VoxelCube&) const = default;
};

struct VoxelCube_Region {
    Pos::Local4096_u Left, Right;
    VoxelId_t Material;

    auto operator<=>(const VoxelCube_Region&) const = default;
};

struct Node {
    NodeId_t NodeId;
    uint8_t Rotate : 6;
};


struct AABB {
    Pos::Object VecMin, VecMax;

    void sortMinMax() {
        Pos::Object::value_type left, right;

        for(int iter = 0; iter < 3; iter++) {
            left = std::min(VecMin[iter], VecMax[iter]);
            right = std::max(VecMin[iter], VecMax[iter]);
            VecMin[iter] = left;
            VecMax[iter] = right;
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
            Pos::Local16_u Pos;
        } Node;

        struct {
            Pos::Local16_u Chunk;
            uint32_t Index;
            VoxelId_t Id;
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
public:
    LocalAABB ABBOX;

    // PosQuat
    WorldId_t WorldId;
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
    Entity();
    
    AABB aabbAtPos() {
        return {Pos-Pos::Object(ABBOX.x/2, ABBOX.y/2, ABBOX.z/2), Pos+Pos::Object(ABBOX.x/2, ABBOX.y/2, ABBOX.z/2)};
    }
};


template<typename Vec>
struct VoxelCuboidsFuncs {

    // Кубы должны быть отсортированы
    static bool canMerge(const Vec& a, const Vec& b) {
        if (a.Material != b.Material) return false;

        // Проверяем, что кубы смежны по одной из осей
        bool xAdjacent = (a.Right.X == b.Left.X) && (a.Left.Y == b.Left.Y) && (a.Right.Y == b.Right.Y) && (a.Left.Z == b.Left.Z) && (a.Right.Z == b.Right.Z);
        bool yAdjacent = (a.Right.Y == b.Left.Y) && (a.Left.X == b.Left.X) && (a.Right.X == b.Right.X) && (a.Left.Z == b.Left.Z) && (a.Right.Z == b.Right.Z);
        bool zAdjacent = (a.Right.Z == b.Left.Z) && (a.Left.X == b.Left.X) && (a.Right.X == b.Right.X) && (a.Left.Y == b.Left.Y) && (a.Right.Y == b.Right.Y);

        return xAdjacent || yAdjacent || zAdjacent;
    }

    static Vec mergeCubes(const Vec& a, const Vec& b) {
        Vec merged;
        merged.Material = a.Material;

        // Объединяем кубы по минимальным и максимальным координатам
        merged.Left.X = std::min(a.Left.X, b.Left.X);
        merged.Left.Y = std::min(a.Left.Y, b.Left.Y);
        merged.Left.Z = std::min(a.Left.Z, b.Left.Z);

        merged.Right.X = std::max(a.Right.X, b.Right.X);
        merged.Right.Y = std::max(a.Right.Y, b.Right.Y);
        merged.Right.Z = std::max(a.Right.Z, b.Right.Z);

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
        int minX = region.Left.X >> 8;
        int minY = region.Left.Y >> 8;
        int minZ = region.Left.Z >> 8;
        int maxX = region.Right.X >> 8;
        int maxY = region.Right.Y >> 8;
        int maxZ = region.Right.Z >> 8;

        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    Pos::Local256_u left {
                        static_cast<uint8_t>(std::max<uint16_t>((x << 8), region.Left.X) - (x << 8)),
                        static_cast<uint8_t>(std::max<uint16_t>((y << 8), region.Left.Y) - (y << 8)),
                        static_cast<uint8_t>(std::max<uint16_t>((z << 8), region.Left.Z) - (z << 8))
                    };
                    Pos::Local256_u right {
                        static_cast<uint8_t>(std::min<uint16_t>(((x+1) << 8)-1, region.Right.X) - (x << 8)),
                        static_cast<uint8_t>(std::min<uint16_t>(((y+1) << 8)-1, region.Right.Y) - (y << 8)),
                        static_cast<uint8_t>(std::min<uint16_t>(((z+1) << 8)-1, region.Right.Z) - (z << 8))
                    };

                    int chunkIndex = z * 16 * 16 + y * 16 + x;
                    chunks[chunkIndex].emplace_back(left, right, region.Material);
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

                Pos::Local4096_u left(x << 8, y << 8, z << 8);
                
                for (const auto& cube : chunks[chunkIndex]) {
                    regions.emplace_back(
                        Pos::Local4096_u(left.X+cube.Left.X, left.Y+cube.Left.Y, left.Z+cube.Left.Z),
                        Pos::Local4096_u(left.X+cube.Right.X, left.Y+cube.Right.Y, left.Z+cube.Right.Z), 
                        cube.Material
                    );
                }
            }
        }
    }

    std::sort(regions.begin(), regions.end());
    regions = VoxelCuboidsFuncs<VoxelCube_Region>::optimizeVoxelRegions(regions);
}

}