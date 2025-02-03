#pragma once

#include <cstdint>
#include <glm/ext.hpp>

namespace AL {
namespace Pos {

struct Local4_u {
    uint8_t X : 2, Y : 2, Z : 2;

    using Key = uint8_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 2) | (Key(Z) << 4);
    };
};

struct Local16_u {
    uint8_t X : 4, Y : 4, Z : 4;

    using Key = uint16_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 4) | (Key(Z) << 8);
    };

    Local4_u left() const { return Local4_u{uint8_t(uint16_t(X) >> 2), uint8_t(uint16_t(Y) >> 2), uint8_t(uint16_t(Z) >> 2)}; }
    Local4_u right() const { return Local4_u{uint8_t(uint16_t(X) & 0b11), uint8_t(uint16_t(Y) & 0b11), uint8_t(uint16_t(Z) & 0b11)}; }
};

struct Local16 {
    int8_t X : 4, Y : 4, Z : 4;

    using Key = uint16_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 4) | (Key(Z) << 8);
    };

    Local4_u left() const { return Local4_u{uint8_t(uint16_t(X) >> 2), uint8_t(uint16_t(Y) >> 2), uint8_t(uint16_t(Z) >> 2)}; }
    Local4_u right() const { return Local4_u{uint8_t(uint16_t(X) & 0b11), uint8_t(uint16_t(Y) & 0b11), uint8_t(uint16_t(Z) & 0b11)}; }
};

struct Local256 {
    int8_t X : 8, Y : 8, Z : 8;

    auto operator<=>(const Local256&) const = default;
};

struct Local256_u {
    uint8_t X : 8, Y : 8, Z : 8;

    auto operator<=>(const Local256_u&) const = default;
};

struct Local4096 {
    int16_t X : 12, Y : 12, Z : 12;

    auto operator<=>(const Local4096&) const = default;
};

struct Local4096_u {
    uint16_t X : 12, Y : 12, Z : 12;

    auto operator<=>(const Local4096_u&) const = default;
};

struct GlobalVoxel {
    int32_t X : 24, Y : 24, Z : 24;

    auto operator<=>(const GlobalVoxel&) const = default;
};

struct GlobalNode {
    int32_t X : 20, Y : 20, Z : 20;

    using Key = uint64_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 20) | (Key(Z) << 40);
    };

    auto operator<=>(const GlobalNode&) const = default;
};

struct GlobalChunk {
    int16_t X : 16, Y : 16, Z : 16;

    using Key = uint64_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 16) | (Key(Z) << 32);
    };

    auto operator<=>(const GlobalChunk&) const = default;
};

struct GlobalRegion {
    int16_t X : 12, Y : 12, Z : 12;

    using Key = uint64_t;
    operator Key() const {
        return Key(X) | (Key(Y) << 12) | (Key(Z) << 24);
    };

    auto operator<=>(const GlobalRegion&) const = default;
};

using Object = glm::i32vec3;

struct Object_t {
    // Позиции объектов целочисленные, BS единиц это один метр
    static constexpr int32_t BS = 4096, BS_Bit = 12;

    static glm::vec3 asFloatVec(Object &obj) { return glm::vec3(float(obj.x)/float(BS), float(obj.y)/float(BS), float(obj.z)/float(BS)); }
    static GlobalNode asNodePos(Object &obj) { return GlobalNode(obj.x >> BS_Bit, obj.y >> BS_Bit, obj.z >> BS_Bit); }
};

}

struct LightPrism {
    uint8_t R : 2, G : 2, B : 2;
};

}


#include <functional>

namespace std {

#define hash_for_pos(type) template <> struct hash<AL::Pos::type> { std::size_t operator()(const AL::Pos::type& obj) const { return std::hash<AL::Pos::type::Key>()((AL::Pos::type::Key) obj); } };
hash_for_pos(Local4_u)
hash_for_pos(Local16_u)
hash_for_pos(Local16)
hash_for_pos(GlobalChunk)
hash_for_pos(GlobalRegion)
#undef hash_for_pos

}