#include "Abstract.hpp"
#include <csignal>


namespace LV::Server {

}

namespace std {

template <>
struct hash<LV::Server::ServerObjectPos> {
    std::size_t operator()(const LV::Server::ServerObjectPos& obj) const {
        return std::hash<uint32_t>()(obj.WorldId) ^ std::hash<LV::Pos::Object>()(obj.ObjectPos); 
    } 
};
}