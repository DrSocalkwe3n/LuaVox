#pragma once

#include <cstdint>
#include <glm/ext.hpp>
#include <memory>
#include <type_traits>


namespace LV {
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
            out |= Pack(U(get(iter))) << BitsPerComponent*iter;
        } 

        return out;
    }
    
    void unpack(const Pack pack) requires (N*BitsPerComponent <= 64) {
        using U = std::make_unsigned_t<T>;

        for(size_t iter = 0; iter < N; iter++) {
            set(iter, U((pack >> BitsPerComponent*iter) & ((Pack(1) << BitsPerComponent)-1)));
        }
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
    static GlobalRegion asRegionsPos(const Object &obj) { return (GlobalRegion) (obj >> BS_Bit >> 8); }
};

}


using ResourceId_t = uint32_t;

/*
    Bin привязывается к путю. Если по путю обновляется объект, пересчитывается его кеш и на клиентах обновляется
*/

enum class EnumBinResource {
    Texture, Animation, Model, Sound, Font
};

using BinaryResource = std::shared_ptr<const std::u8string>;

// Двоичные данные
using BinTextureId_t = ResourceId_t;
using BinAnimationId_t = ResourceId_t;
using BinModelId_t = ResourceId_t;
using BinSoundId_t = ResourceId_t;
using BinFontId_t = ResourceId_t;

// Шаблоны использования бинарных ресурсов
// using DefTextureId_t   = ResourceId_t;
// using DefModelId_t     = ResourceId_t;
// using DefSoundId_t     = ResourceId_t;
// using DefFontId_t      = ResourceId_t;

enum class EnumDefContent {
    Voxel, Node, Generator, World, Portal, Entity, FuncEntitry
};

// Игровые определения
using DefVoxelId_t      = ResourceId_t;
using DefNodeId_t       = ResourceId_t;
using DefWorldId_t      = ResourceId_t;
using DefPortalId_t     = ResourceId_t;
using DefEntityId_t     = ResourceId_t;
using DefFuncEntityId_t = ResourceId_t;
using DefItemId_t       = ResourceId_t;

// Контент, основанный на игровых определениях
using WorldId_t = ResourceId_t;
using PortalId_t = ResourceId_t;

// struct LightPrism {
//     uint8_t R : 2, G : 2, B : 2;
// };

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

}
