#pragma once

#include "TOSLib.hpp"
#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <Common/Abstract.hpp>
#include <Common/Collide.hpp>
#include <sha2.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <boost/json.hpp>
#include <variant>


namespace LV::Server {

namespace js = boost::json;

// В одном регионе может быть максимум 2^16 сущностей. Клиенту адресуются сущности в формате <мир>+<позиция региона>+<uint16_t>
// И если сущность перешла из одного региона в другой, идентификатор сущности на стороне клиента сохраняется
using RegionEntityId_t = uint16_t;
using ClientEntityId_t = RegionEntityId_t;
using ServerEntityId_t = std::tuple<WorldId_t, Pos::GlobalRegion, RegionEntityId_t>;
using RegionFuncEntityId_t = uint16_t;
using ClientFuncEntityId_t = RegionFuncEntityId_t;
using ServerFuncEntityId_t = std::tuple<WorldId_t, Pos::GlobalRegion, RegionFuncEntityId_t>;

using MediaStreamId_t = uint16_t;
using ContentBridgeId_t = ResourceId_t;
using PlayerId_t = ResourceId_t;
using DefGeneratorId_t = ResourceId_t;


/*
    Сервер загружает информацию о локальных текстурах
    Пересмотр списка текстур?
    Динамичные текстуры?

*/

struct ResourceFile {
    using Hash_t = sha2::sha256_hash; // boost::uuids::detail::sha1::digest_type;

    Hash_t Hash;
    std::vector<std::byte> Data;

    void calcHash() {
        Hash = sha2::sha256((const uint8_t*) Data.data(), Data.size());
    }
};

struct ServerTime {
    uint32_t Seconds : 24, Sub : 8;
};

struct VoxelCube_Region {
    union {
        struct {
            DefVoxelId_t VoxelId : 24, Meta : 8;
        };

        DefVoxelId_t Data = 0;
    };

    Pos::bvec1024u Left, Right; // TODO: заменить на позицию и размер

    auto operator<=>(const VoxelCube_Region& other) const {
        if (auto cmp = Left <=> other.Left; cmp != 0)
            return cmp;

        if (auto cmp = Right <=> other.Right; cmp != 0)
            return cmp;

        return Data <=> other.Data;
    }

    bool operator==(const VoxelCube_Region& other) const {
        return Left == other.Left && Right == other.Right && Data == other.Data;
    }
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
        Voxel, Node, Entity, FuncEntity, Barrier, Portal, Another
    } Type;

    union {
        struct {
            RegionEntityId_t Index;
        } Entity;

        struct {
            RegionFuncEntityId_t Index;
        } FuncEntity;

        struct {
            Pos::bvec4u Chunk;
            Pos::bvec16u Pos;
        } Node;

        struct {
            Pos::bvec4u Chunk;
            uint32_t Index;
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

/*
    Модель, считанная с файла и предкомпилированна
    Для компиляции нужно собрать зависимости модели,
    и в случае их изменения нужно перекомпилировать модель (может просто перекомпилировать всё разом?)
*/
struct PreCompiledModel_t {

};

struct NodestateEntry {
    std::string Name;
    int Variability = 0;
    std::vector<std::string> ValueNames;
};

using NodeStateCondition = std::bitset<256>;
NodeStateCondition nodestateExpression(const std::vector<NodestateEntry>& entries, const std::string& expression);

struct ModelTransform {
    std::vector<AssetsModel> Ids;
    uint16_t Weight = 1;
    bool UVLock = false;
};

/*
    Указать модель, текстуры и поворот по конкретным осям.
    Может быть вариативность моделей относительно одного условия (случайность в зависимости от координат?)
    Допускается активация нескольких условий одновременно

    условия snowy=false

    "snowy=false": [{"model": "node/grass_node"}, {"model": "node/grass_node", transformations: ["y=90", "x=67"]}] <- модель будет выбрана случайно
    или
    : [{models: [], weight: 1}, {}] <- в models можно перечислить сразу несколько моделей, и они будут использоваться одновременно
    или
    "": {"model": "node/grass", weight <вес влияющий на шанс отображения именно этой модели>}
    или просто
    "model": "node/grass_node"
    В условия добавить простые проверки !><=&|()
    в задании параметров модели использовать формулы с применением состояний

    uvlock ? https://minecraft.wiki/w/Blockstates_definition/format
*/

using DefNodestates_t = std::unordered_map<NodeStateCondition, std::vector<ModelTransform>>;

// Скомпилированный профиль ноды
struct DefNode_t {
    // Зарегистрированные состояния (мета)
    // Подгружается с файла assets/<modid>/nodestate/node/nodeId.json
    std::variant<DefNodestates_t, std::vector<ModelTransform>> StatesRouter;

    // Параметры рендера
    struct {
        bool HasHalfTransparency = false;
    } Render;

    // Параметры коллизии
    struct {
        enum class EnumCollisionType {
            None, ByRender, 
        };

        std::variant<EnumCollisionType> CollisionType = EnumCollisionType::None;
    } Collision;

    // События
    struct {

    } Events;

    // Если нода умная, то для неё будет создаваться дополнительный более активный объект
    std::optional<sol::protected_function> NodeAdvancementFactory;
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

inline void convertRegionVoxelsToChunks(const std::vector<VoxelCube_Region>& regions, std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> &chunks) {
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

                    chunks[Pos::bvec4u(x, y, z)].push_back({
                        region.VoxelId, region.Meta, left, right
                    });
                }
            }
        }
    }
}

inline void convertChunkVoxelsToRegion(const std::unordered_map<Pos::bvec4u, std::vector<VoxelCube>> &chunks, std::vector<VoxelCube_Region> &regions) {
    for(const auto& [pos, voxels] : chunks) {
        Pos::bvec1024u left = pos << 8;
        for (const auto& cube : voxels) {
            regions.push_back({
                cube.VoxelId, cube.Meta,
                Pos::bvec1024u(left.x+cube.Pos.x, left.y+cube.Pos.y, left.z+cube.Pos.z),
                Pos::bvec1024u(left.x+cube.Pos.x+cube.Size.x, left.y+cube.Pos.y+cube.Size.y, left.z+cube.Pos.z+cube.Size.z)
            });
        }
    }

    std::sort(regions.begin(), regions.end());
    regions = VoxelCuboidsFuncs<VoxelCube_Region>::optimizeVoxelRegions(regions);
}


struct ServerObjectPos {
    WorldId_t WorldId;
    Pos::Object ObjectPos;
};

/*
    Разница между информацией о наблюдаемых регионах
*/
struct ContentViewInfo_Diff {
    // Изменения на уровне миров (увиден или потерян)
    std::vector<WorldId_t> WorldsNew, WorldsLost;
    // Изменения на уровне регионов
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> RegionsNew, RegionsLost;

    bool empty() const {
        return WorldsNew.empty() && WorldsLost.empty() && RegionsNew.empty() && RegionsLost.empty();
    }
};

/*
    То, какие регионы наблюдает игрок
*/
struct ContentViewInfo {
    // std::vector<Pos::GlobalRegion> - сортированный и с уникальными значениями
    std::unordered_map<WorldId_t, std::vector<Pos::GlobalRegion>> Regions;

    // Что изменилось относительно obj
    // Перерасчёт должен проводится при смещении игрока или ContentBridge за границу региона
    ContentViewInfo_Diff diffWith(const ContentViewInfo& obj) const {
        ContentViewInfo_Diff out;

        // Проверяем новые миры и регионы
        for(const auto& [key, regions] : Regions) {
            auto iterWorld = obj.Regions.find(key);

            if(iterWorld == obj.Regions.end()) {
                out.WorldsNew.push_back(key);
                out.RegionsNew[key] = regions;
            } else {
                auto &vec = out.RegionsNew[key];
                vec.reserve(8*8);
                std::set_difference(
                    regions.begin(), regions.end(),
                    iterWorld->second.begin(), iterWorld->second.end(),
                    std::back_inserter(vec)
                );
            }
        }

        // Проверяем потерянные миры и регионы
        for(const auto& [key, regions] : obj.Regions) {
            auto iterWorld = Regions.find(key);

            if(iterWorld == Regions.end()) {
                out.WorldsLost.push_back(key);
                out.RegionsLost[key] = regions;
            } else {
                auto &vec = out.RegionsLost[key];
                vec.reserve(8*8);
                std::set_difference(
                    regions.begin(), regions.end(),
                    iterWorld->second.begin(), iterWorld->second.end(),
                    std::back_inserter(vec)
                );
            }
        }

        // shrink_to_feet
        for(auto& [_, regions] : out.RegionsNew)
            regions.shrink_to_fit();
        for(auto& [_, regions] : out.RegionsLost)
            regions.shrink_to_fit();

        return out;
    }
};

/*
    Мост контента, для отслеживания событий из удалённых точек
    По типу портала, через который можно видеть контент на расстоянии
*/
struct ContentBridge {
    /* 
        false -> Из точки Left видно контент в точки Right 
        true -> Контент виден в обе стороны
    */
    bool IsTwoWay = false;
    WorldId_t LeftWorld;
    Pos::GlobalRegion LeftPos;
    WorldId_t RightWorld;
    Pos::GlobalRegion RightPos;
};

struct ContentViewCircle {
    WorldId_t WorldId;
    Pos::GlobalRegion Pos;
    // Радиус в регионах в квадрате
    int16_t Range;
};

}