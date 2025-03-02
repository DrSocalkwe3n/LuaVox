#pragma once

#include <cstdint>
#include <glm/ext.hpp>

namespace LV {
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

    Local16_u& operator=(const Key &key) {
        X = key & 0xf;
        Y = (key >> 4) & 0xf;
        Z = (key >> 8) & 0xf;
        return *this;
    }

    Local4_u left() const { return Local4_u{uint8_t(uint16_t(X) >> 2), uint8_t(uint16_t(Y) >> 2), uint8_t(uint16_t(Z) >> 2)}; }
    Local4_u right() const { return Local4_u{uint8_t(uint16_t(X) & 0b11), uint8_t(uint16_t(Y) & 0b11), uint8_t(uint16_t(Z) & 0b11)}; }
};

struct Local16 {
    int8_t X : 4, Y : 4, Z : 4;

    using Key = uint16_t;
    operator Key() const {
        return Key(uint8_t(X)) | (Key(uint8_t(Y) << 4)) | (Key(uint8_t(Z)) << 8);
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
        return Key(uint32_t(X)) | (Key(uint32_t(Y) << 20)) | (Key(uint32_t(Z)) << 40);
    };

    auto operator<=>(const GlobalNode&) const = default;
};

struct GlobalChunk {
    int16_t X, Y, Z;

    using Key = uint64_t;
    operator Key() const {
        return Key(uint16_t(X)) | (Key(uint16_t(Y)) << 16) | (Key(uint16_t(Z)) << 32);
    };

    operator glm::i16vec3() const {
        return {X, Y, Z};
    }

    auto operator<=>(const GlobalChunk&) const = default;

    Local16_u toLocal() const {
        return Local16_u(X & 0xf, Y & 0xf, Z & 0xf);
    }
};

struct GlobalRegion {
    int16_t X : 12, Y : 12, Z : 12;

    using Key = uint64_t;
    operator Key() const {
        return Key(uint16_t(X)) | (Key(uint16_t(Y) << 12)) | (Key(uint16_t(Z)) << 24);
    };

    auto operator<=>(const GlobalRegion&) const = default;

    void fromChunk(const GlobalChunk &posChunk) {
        X = posChunk.X >> 4;
        Y = posChunk.Y >> 4;
        Z = posChunk.Z >> 4;
    }

    GlobalChunk toChunk() const {
        return GlobalChunk(uint16_t(X) << 4, uint16_t(Y) << 4, uint16_t(Z) << 4);
    }

    GlobalChunk toChunk(const Pos::Local16_u &posLocal) const {
        return GlobalChunk(
            (uint16_t(X) << 4) | posLocal.X,
            (uint16_t(Y) << 4) | posLocal.Y,
            (uint16_t(Z) << 4) | posLocal.Z
        );
    }
};

using Object = glm::i32vec3;

struct Object_t {
    // Позиции объектов целочисленные, BS единиц это один метр
    static constexpr int32_t BS = 4096, BS_Bit = 12;

    static glm::vec3 asFloatVec(Object &obj) { return glm::vec3(float(obj.x)/float(BS), float(obj.y)/float(BS), float(obj.z)/float(BS)); }
    static GlobalNode asNodePos(Object &obj) { return GlobalNode(obj.x >> BS_Bit, obj.y >> BS_Bit, obj.z >> BS_Bit); }
    static GlobalChunk asChunkPos(Object &obj) { return GlobalChunk(obj.x >> BS_Bit >> 4, obj.y >> BS_Bit >> 4, obj.z >> BS_Bit >> 4); }
    static GlobalChunk asRegionsPos(Object &obj) { return GlobalChunk(obj.x >> BS_Bit >> 8, obj.y >> BS_Bit >> 8, obj.z >> BS_Bit >> 8); }
};

}

struct LightPrism {
    uint8_t R : 2, G : 2, B : 2;
};

// Идентификаторы на стороне клиента
using TextureId_c = uint16_t;
using SoundId_c = uint16_t;
using ModelId_c = uint16_t;

using DefWorldId_c = uint8_t;
using DefVoxelId_c = uint16_t;
using DefNodeId_c = uint16_t;
using DefPortalId_c = uint8_t;
using WorldId_c = uint8_t;
using PortalId_c = uint8_t;
using DefEntityId_c = uint16_t;
using EntityId_c = uint16_t;

}


#include <functional>

namespace std {

#define hash_for_pos(type) template <> struct hash<LV::Pos::type> { std::size_t operator()(const LV::Pos::type& obj) const { return std::hash<LV::Pos::type::Key>()((LV::Pos::type::Key) obj); } };
hash_for_pos(Local4_u)
hash_for_pos(Local16_u)
hash_for_pos(Local16)
hash_for_pos(GlobalChunk)
hash_for_pos(GlobalRegion)
#undef hash_for_pos

}
