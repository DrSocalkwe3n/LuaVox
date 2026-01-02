#include "Abstract.hpp"
#include <csignal>


namespace LV::Server {

Entity::Entity(DefEntityId defId)
    : DefId(defId)
{
    ABBOX = {Pos::Object_t::BS, Pos::Object_t::BS, Pos::Object_t::BS};
    WorldId = 0;
    Pos = Pos::Object(0);
    Speed = Pos::Object(0);
    Acceleration = Pos::Object(0);
    Quat = glm::quat(1.f, 0.f, 0.f, 0.f);
    InRegionPos = Pos::GlobalRegion(0);
}

}

namespace std {

template <>
struct hash<LV::Server::ServerObjectPos> {
    std::size_t operator()(const LV::Server::ServerObjectPos& obj) const {
        return std::hash<uint32_t>()(obj.WorldId) ^ std::hash<LV::Pos::Object>()(obj.ObjectPos); 
    } 
};
}
