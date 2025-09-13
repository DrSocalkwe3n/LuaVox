#pragma once

#include "TOSLib.hpp"
#include "boost/json/array.hpp"
#include <cstdint>
#include <glm/ext.hpp>
#include <memory>
#include <sol/forward.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <boost/json.hpp>
#include <boost/container/small_vector.hpp>
#include <execution>


namespace LV {

namespace js = boost::json;

namespace Pos {

template<typename T, size_t BitsPerComponent>
class BitVec3 {
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    static_assert(BitsPerComponent > 0, "Bits per component must be at least 1");
    static constexpr size_t N = 3;

    static constexpr auto getType() {
        constexpr size_t bits = N*BitsPerComponent;
        if constexpr(bits <= 8)
            return uint8_t(0);
        else if constexpr(bits <= 16)
            return uint16_t(0);
        else if constexpr(bits <= 32)
            return uint32_t(0);
        else if constexpr(bits <= 64)
            return uint64_t(0);
        else {
            static_assert("Нет подходящего хранилища");
            return uint8_t(0);
        }
    }
public:
    using Pack = decltype(getType());
    using Type = T;
    using value_type = Type;

    T x : BitsPerComponent, y : BitsPerComponent, z : BitsPerComponent;

public:
    BitVec3() = default;
    BitVec3(T value)
        : x(value), y(value), z(value)
    {}
    BitVec3(const T x, const T y, const T z)
        : x(x), y(y), z(z)
    {}
	template<typename vT, glm::qualifier vQ>
    BitVec3(const glm::vec<3, vT, vQ> vec)
        : x(vec.x), y(vec.y), z(vec.z)
    {}
    BitVec3(const BitVec3&) = default;
    BitVec3(BitVec3&&) = default;

    BitVec3& operator=(const BitVec3&) = default;
    BitVec3& operator=(BitVec3&&) = default;

    void set(size_t index, const T value) {
        assert(index < N);
        if(index == 0)
            x = value;
        else if(index == 1)
            y = value;
        else if(index == 2)
            z = value;
    }

    const T get(size_t index) const {
        assert(index < N);
        if(index == 0)
            return x;
        else if(index == 1)
            return y;
        else if(index == 2)
            return z;

        return 0;
    }

    const T operator[](size_t index) const {
        return get(index);
    }

    Pack pack() const requires (N*BitsPerComponent <= 64) {
        Pack out = 0;
        using U = std::make_unsigned_t<T>;

        for(size_t iter = 0; iter < N; iter++) {
            out |= Pack(U(get(iter)) & U((Pack(1) << BitsPerComponent)-1)) << BitsPerComponent*iter;
        } 

        return out;
    }
    
    BitVec3 unpack(const Pack pack) requires (N*BitsPerComponent <= 64) {
        using U = std::make_unsigned_t<T>;

        for(size_t iter = 0; iter < N; iter++) {
            set(iter, T(U((pack >> BitsPerComponent*iter) & U((Pack(1) << BitsPerComponent)-1))));
        }

        return *this;
    }

    auto operator<=>(const BitVec3&) const = default;

    template<typename T2, size_t BitsPerComponent2>
    operator BitVec3<T2, BitsPerComponent2>() const {
        BitVec3<T2, BitsPerComponent2> out;
        for(size_t iter = 0; iter < N; iter++) {
            out.set(iter, T2(std::make_unsigned_t<T2>(std::make_unsigned_t<T>(get(iter)))));
        }

        return out;
    }


	template<typename vT, glm::qualifier vQ>
    operator glm::vec<3, vT, vQ>() const {
        return {x, y, z};
    }

    BitVec3 operator+(const BitVec3 &other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) + other[iter]);

        return out;
    }

    BitVec3& operator+=(const BitVec3 &other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) + other[iter]);

        return *this;
    }

    BitVec3 operator+(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) + value);

        return out;
    }

    BitVec3& operator+=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) + value);

        return *this;
    }

    BitVec3 operator-(const BitVec3 &other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) - other[iter]);

        return out;
    }

    BitVec3& operator-=(const BitVec3 &other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) - other[iter]);

        return *this;
    }

    BitVec3 operator-(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) - value);

        return out;
    }

    BitVec3& operator-=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) - value);

        return *this;
    }

    BitVec3 operator*(const BitVec3 &other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) * other[iter]);

        return out;
    }

    BitVec3& operator*=(const BitVec3 &other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) * other[iter]);

        return *this;
    }

    BitVec3 operator*(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) * value);

        return out;
    }

    BitVec3& operator*=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) * value);

        return *this;
    }

    BitVec3 operator/(const BitVec3 &other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) / other[iter]);

        return out;
    }

    BitVec3& operator/=(const BitVec3 &other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) / other[iter]);

        return *this;
    }

    BitVec3 operator/(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) / value);

        return out;
    }

    BitVec3& operator/=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) / value);

        return *this;
    }

    BitVec3 operator>>(const auto offset) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) >> offset);

        return out;
    }

    BitVec3& operator>>=(const auto offset) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) >> offset);

        return *this;
    }

    BitVec3 operator<<(const auto offset) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) << offset);

        return out;
    }

    BitVec3& operator<<=(const auto offset) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) << offset);

        return *this;
    }

    BitVec3 operator|(const BitVec3 other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) | other[iter]);

        return out;
    }

    BitVec3& operator|=(const BitVec3 other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) | other[iter]);

        return *this;
    }

    BitVec3 operator|(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) | value);

        return out;
    }

    BitVec3& operator|=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) | value);

        return *this;
    }

    BitVec3 operator&(const BitVec3 other) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) & other[iter]);

        return out;
    }

    BitVec3& operator&=(const BitVec3 other) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) & other[iter]);

        return *this;
    }

    BitVec3 operator&(const T value) const {
        BitVec3 out;

        for(size_t iter = 0; iter < N; iter++)
            out.set(iter, get(iter) & value);

        return out;
    }

    BitVec3& operator&=(const T value) {
        for(size_t iter = 0; iter < N; iter++)
            set(iter, get(iter) & value);

        return *this;
    }

    static constexpr size_t length() { return N; }
};

using bvec4i = BitVec3<int8_t, 2>;
using bvec4u = BitVec3<uint8_t, 2>;
using bvec16i = BitVec3<int8_t, 4>;
using bvec16u = BitVec3<uint8_t, 4>;
using bvec64i = BitVec3<int8_t, 6>;
using bvec64u = BitVec3<uint8_t, 6>;
using bvec256i = BitVec3<int8_t, 8>;
using bvec256u = BitVec3<uint8_t, 8>;
using bvec1024i = BitVec3<int16_t, 10>;
using bvec1024u = BitVec3<uint16_t, 10>;
using bvec4096i = BitVec3<int16_t, 12>;
using bvec4096u = BitVec3<uint16_t, 12>;

using GlobalVoxel = BitVec3<int32_t, 24>;
using GlobalNode = BitVec3<int32_t, 20>;
using GlobalChunk = BitVec3<int16_t, 16>;
using GlobalRegion = BitVec3<int16_t, 14>;
using Object = BitVec3<int32_t, 32>;

struct Object_t {
    // Позиции объектов целочисленные, BS единиц это один метр
    static constexpr int32_t BS = 4096, BS_Bit = 12;

    static glm::vec3 asFloatVec(const Object &obj) { return glm::vec3(float(obj[0])/float(BS), float(obj[1])/float(BS), float(obj[2])/float(BS)); }
    static GlobalNode asNodePos(const Object &obj) { return (GlobalNode) (obj >> BS_Bit); }
    static GlobalChunk asChunkPos(const Object &obj) { return (GlobalChunk) (obj >> BS_Bit >> 4); }
    static GlobalRegion asRegionsPos(const Object &obj) { return (GlobalRegion) (obj >> BS_Bit >> 6); }
};

}


using ResourceId = uint32_t;
struct Resource;

/*
    Объекты, собранные из папки assets или зарегистрированные модами.
    Клиент получает полную информацию о таких объектах и при надобности
    запрашивает получение файла.
    Id -> Key + SHA256

    Если объекты удаляются, то сторона клиента об этом не уведомляется
*/
enum class EnumAssets {
   Nodestate, Particle, Animation, Model, Texture, Sound, Font, MAX_ENUM
};

using AssetsNodestate   = ResourceId;
using AssetsParticle    = ResourceId;
using AssetsAnimation   = ResourceId;
using AssetsModel       = ResourceId;
using AssetsTexture     = ResourceId;
using AssetsSound       = ResourceId;
using AssetsFont        = ResourceId;

/*
    Определения контента, доставляются клиентам сразу
*/
enum class EnumDefContent {
    Voxel, Node, World, Portal, Entity, Item, MAX_ENUM
};

using DefVoxelId      = ResourceId;
using DefNodeId       = ResourceId;
using DefWorldId      = ResourceId;
using DefPortalId     = ResourceId;
using DefEntityId     = ResourceId;
using DefItemId       = ResourceId;

/*
    Контент, основанный на определениях.
    Отдельные свойства могут менятся в самих объектах
*/

using WorldId_t = ResourceId;

// struct LightPrism {
//     uint8_t R : 2, G : 2, B : 2;
// };



struct VoxelCube {
    union {
        struct {
            DefVoxelId VoxelId : 24, Meta : 8;
        };

        DefVoxelId Data = 0;
    };

    Pos::bvec256u Pos, Size; // Размер+1, 0 это единичный размер

    auto operator<=>(const VoxelCube& other) const {
        if (auto cmp = Pos <=> other.Pos; cmp != 0)
            return cmp;

        if (auto cmp = Size <=> other.Size; cmp != 0)
            return cmp;

        return Data <=> other.Data;
    }

    bool operator==(const VoxelCube& other) const {
        return Pos == other.Pos && Size == other.Size && Data == other.Data;
    }
};

struct CompressedVoxels {
    std::u8string Compressed;
    // Уникальный сортированный список идентификаторов вокселей
    std::vector<DefVoxelId> Defines;
};

CompressedVoxels compressVoxels(const std::vector<VoxelCube>& voxels, bool fast = true);
std::vector<VoxelCube> unCompressVoxels(const std::u8string& compressed);

struct Node {
    union {
        struct {
            DefNodeId NodeId : 24, Meta : 8;
        };
        DefNodeId Data;
    };
};

struct CompressedNodes {
    std::u8string Compressed;
    // Уникальный сортированный список идентификаторов нод
    std::vector<DefNodeId> Defines;
};

CompressedNodes compressNodes(const Node* nodes, bool fast = true);
void unCompressNodes(const std::u8string& compressed, Node* ptr);

std::u8string compressLinear(const std::u8string& data);
std::u8string unCompressLinear(const std::u8string& data);

inline std::pair<std::string, std::string> parseDomainKey(const std::string& value, const std::string_view defaultDomain = "core") {
    auto regResult = TOS::Str::match(value, "(?:([\\w\\d_]+):)?([\\w\\d/_.]+)");
    if(!regResult)
        MAKE_ERROR("Недействительный домен:ключ");

    if(regResult->at(1)) {
        return std::pair<std::string, std::string>{*regResult->at(1), *regResult->at(2)};
    } else {
        return std::pair<std::string, std::string>{defaultDomain, *regResult->at(2)};
    }
}

struct PrecompiledTexturePipeline {
    // Локальные идентификаторы пайплайна в домен+ключ
    std::vector<std::pair<std::string, std::string>> Assets;
    // Чистый код текстурных преобразований, локальные идентификаторы связаны с Assets
    std::u8string Pipeline;
};

struct TexturePipeline {
    // Разыменованые идентификаторы
    std::vector<AssetsTexture> BinTextures;
    // Чистый код текстурных преобразований, локальные идентификаторы связаны с BinTextures
    std::u8string Pipeline;
};

// Компилятор текстурных потоков
inline PrecompiledTexturePipeline compileTexturePipeline(const std::string &cmd, const std::string_view defaultDomain = "core") {
    PrecompiledTexturePipeline result;

    auto [domain, key] = parseDomainKey(cmd, defaultDomain);

    result.Assets.emplace_back(domain, key);

    return result;
}

struct NodestateEntry {
    std::string Name;
    int Variability = 0;                    // Количество возможный значений состояния
    std::vector<std::string> ValueNames;    // Имена состояний, если имеются
};

struct Vertex {
    glm::vec3 Pos;
    glm::vec2 UV;
    uint32_t TexId;
};

struct Transformation {
    enum EnumTransform {
        MoveX, MoveY, MoveZ,
        RotateX, RotateY, RotateZ,
        ScaleX, ScaleY, ScaleZ,
        MAX_ENUM
    } Op;

    float Value;
};

struct Transformations {
    std::vector<Transformation> OPs;

    void apply(std::vector<Vertex>& vertices) const {
        if (vertices.empty() || OPs.empty())
            return;

        glm::mat4 transform(1.0f);

        for (const auto& op : OPs) {
            switch (op.Op) {
                case Transformation::MoveX:   transform = glm::translate(transform, glm::vec3(op.Value, 0.0f, 0.0f)); break;
                case Transformation::MoveY:   transform = glm::translate(transform, glm::vec3(0.0f, op.Value, 0.0f)); break;
                case Transformation::MoveZ:   transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, op.Value)); break;
                case Transformation::ScaleX:  transform = glm::scale(transform, glm::vec3(op.Value, 1.0f, 1.0f)); break;
                case Transformation::ScaleY:  transform = glm::scale(transform, glm::vec3(1.0f, op.Value, 1.0f)); break;
                case Transformation::ScaleZ:  transform = glm::scale(transform, glm::vec3(1.0f, 1.0f, op.Value)); break;
                case Transformation::RotateX: transform = glm::rotate(transform, op.Value, glm::vec3(1.0f, 0.0f, 0.0f)); break;
                case Transformation::RotateY: transform = glm::rotate(transform, op.Value, glm::vec3(0.0f, 1.0f, 0.0f)); break;
                case Transformation::RotateZ: transform = glm::rotate(transform, op.Value, glm::vec3(0.0f, 0.0f, 1.0f)); break;
                default: break;
            }
        }

        std::transform(
            std::execution::unseq,
            vertices.begin(),
            vertices.end(),
            vertices.begin(),
            [transform](Vertex v) -> Vertex {
                glm::vec4 pos_h(v.Pos, 1.0f);
                pos_h = transform * pos_h;
                v.Pos = glm::vec3(pos_h) / pos_h.w;
                return v;
            }
        );
    }

    std::vector<Vertex> apply(const std::vector<Vertex>& vertices) const {
        std::vector<Vertex> result = vertices;
        apply(result);
        return result;
    }
};

/*
    Хранит распаршенное определение состояний нод.
    Не привязано ни к какому окружению.
*/
struct PreparedNodeState {
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        LT, LE, GT, GE, EQ, NE,
        And, Or,
        Pos, Neg, Not
    };

    struct Node {
        struct Num { int32_t v; };
        struct Var { std::string name; };
        struct Unary { Op op; uint16_t rhs; };
        struct Binary { Op op; uint16_t lhs, rhs; };
        std::variant<Num, Var, Unary, Binary> v;
    };

    struct Model {
        uint16_t Id;
        bool UVLock = false;
        std::vector<Transformation> Transforms;
    };

    struct VectorModel {
        std::vector<Model> Models;
        bool UVLock = false;
        // Может добавить возможность использовать переменную рандома в трансформациях?
        std::vector<Transformation> Transforms;
    };

    // Локальный идентификатор в именной ресурс
    std::vector<std::pair<std::string, std::string>> LocalToModelKD;
    // Локальный идентификатор в глобальный идентификатор
    std::vector<AssetsModel> LocalToModel;
    // Ноды выражений
    std::vector<Node> Nodes;
    // Условия -> вариации модели + веса
    boost::container::small_vector<
        std::pair<uint16_t,
            boost::container::small_vector<
                std::pair<float, std::variant<Model, VectorModel>>,
                1
            >
        >
    , 1> Routes;

    PreparedNodeState(const std::string_view modid, const js::object& profile);
    PreparedNodeState(const std::string_view modid, const sol::table& profile);
    PreparedNodeState(const std::u8string& data);

    PreparedNodeState() = default;
    PreparedNodeState(const PreparedNodeState&) = default;
    PreparedNodeState(PreparedNodeState&&) = default;

    PreparedNodeState& operator=(const PreparedNodeState&) = default;
    PreparedNodeState& operator=(PreparedNodeState&&) = default;

    // Пишет в сжатый двоичный формат
    std::u8string dump() const;

    // Если зависит от случайного распределения по миру
    bool hasVariability() const {
        return HasVariability;
    }

private:
    bool HasVariability = false;

    uint16_t parseCondition(const std::string_view condition);
    std::pair<float, std::variant<Model, VectorModel>> parseModel(const std::string_view modid, const js::object& obj);
    std::vector<Transformation> parseTransormations(const js::array& arr);
};


enum class EnumFace {
    Down, Up, North, South, West, East, None
};

/*
    Парсит json модель
*/
struct PreparedModel {
    enum class EnumGuiLight {
        Default
    };
    
    std::optional<EnumGuiLight> GuiLight = EnumGuiLight::Default;
    std::optional<bool> AmbientOcclusion = false;
    
    struct FullTransformation {
        glm::vec3
            Rotation = glm::vec3(0),
            Translation = glm::vec3(0),
            Scale = glm::vec3(1);
    };
    
    std::unordered_map<std::string, FullTransformation> Display;
    std::unordered_map<std::string, PrecompiledTexturePipeline> Textures;
    std::unordered_map<std::string, TexturePipeline> CompiledTextures;

    struct Cuboid {
        bool Shade;
        glm::vec3 From, To;

        struct Face {
            glm::vec4 UV;
            std::string Texture;
            EnumFace Cullface = EnumFace::None;
            int TintIndex = -1;
            int16_t Rotation = 0;
        };

        std::unordered_map<EnumFace, Face> Faces;
    
        Transformations Trs;
    };

    std::vector<Cuboid> Cuboids;
    
    struct SubModel {
        std::string Domain, Key;
        std::optional<uint16_t> Scene; 
    };
    
    std::vector<SubModel> SubModels;

    // Json
    PreparedModel(const std::string_view modid, const js::object& profile);
    PreparedModel(const std::string_view modid, const sol::table& profile);
    PreparedModel(const std::u8string& data);

    PreparedModel() = default;
    PreparedModel(const PreparedModel&) = default;
    PreparedModel(PreparedModel&&) = default;

    PreparedModel& operator=(const PreparedModel&) = default;
    PreparedModel& operator=(PreparedModel&&) = default;

    // Пишет в сжатый двоичный формат
    std::u8string dump() const;

private:
    void load(std::u8string_view data);
};

struct PreparedGLTF {
    std::vector<std::string> TextureKey;
    std::unordered_map<std::string, PrecompiledTexturePipeline> Textures;
    std::unordered_map<std::string, TexturePipeline> CompiledTextures;
    std::vector<Vertex> Vertices;


    PreparedGLTF(const std::string_view modid, const js::object& gltf);
    PreparedGLTF(const std::string_view modid, Resource glb);
    PreparedGLTF(std::u8string_view data);

    PreparedGLTF() = default;
    PreparedGLTF(const PreparedGLTF&) = default;
    PreparedGLTF(PreparedGLTF&&) = default;

    PreparedGLTF& operator=(const PreparedGLTF&) = default;
    PreparedGLTF& operator=(PreparedGLTF&&) = default;

    // Пишет в сжатый двоичный формат
    std::u8string dump() const;

private:
    void load(std::u8string_view data);
};

enum struct TexturePipelineCMD : uint8_t {
    Texture,    // Указание текстуры
    Combine,    // Комбинирование
};

using Hash_t = std::array<uint8_t, 32>;

struct Resource {
private:
    struct InlineMMap;
    struct InlinePtr;

    std::shared_ptr<std::variant<InlineMMap, InlinePtr>> In;

public:
    Resource() = default;
    Resource(std::filesystem::path path);
    Resource(const uint8_t* data, size_t size);
    Resource(const std::u8string& data);
    Resource(std::u8string&& data);

    Resource(const Resource&) = default;
    Resource(Resource&&) = default;
    Resource& operator=(const Resource&) = default;
    Resource& operator=(Resource&&) = default;
    auto operator<=>(const Resource&) const;

    const std::byte* data() const;
    size_t size() const;
    Hash_t hash() const;

    Resource convertToMem() const;

    operator bool() const {
        return (bool) In;
    }
};

}


#include <functional>

namespace std {

template<typename T, size_t BitsPerComponent>
struct hash<LV::Pos::BitVec3<T, BitsPerComponent>> { 
    std::size_t operator()(const LV::Pos::BitVec3<T, BitsPerComponent>& obj) const {
        std::size_t result = 0;
        constexpr std::size_t seed = 0x9E3779B9;

        for (size_t i = 0; i < 3; ++i) {
            T value = obj[i];

            std::hash<T> hasher;
            std::size_t h = hasher(value);

            result ^= h + seed + (result << 6) + (result >> 2);
        }

        return result;
    } 
};

template <>
struct hash<LV::Hash_t> {
    std::size_t operator()(const LV::Hash_t& hash) const noexcept {
        std::size_t v = 14695981039346656037ULL;
        for (const auto& byte : hash) {
            v ^= static_cast<std::size_t>(byte);
            v *= 1099511628211ULL;
        }
        return v;
    }
};

}
