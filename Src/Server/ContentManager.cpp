#include "ContentManager.hpp"
#include "Common/Abstract.hpp"
#include <algorithm>

namespace LV::Server {

ContentManager::ContentManager(asio::io_context& ioc) {

}

ContentManager::~ContentManager() = default;

void ContentManager::registerBase_Node(ResourceId id, const sol::table& profile) {
    std::optional<DefNode>& node = getEntry_Node(id);
    if(!node)
        node.emplace();

    DefNode& def = *node;

    {
        std::optional<std::variant<std::string, sol::table>> parent = profile.get<std::optional<std::variant<std::string, sol::table>>>("parent");
        if(parent) {
            if(const sol::table* table = std::get_if<sol::table>(&*parent)) {
                // result = createNodeProfileByLua(*table);
            } else if(const std::string* key = std::get_if<std::string>(&*parent)) {
                auto regResult = TOS::Str::match(*key, "(?:([\\w\\d_]+):)?([\\w\\d_]+)");
                if(!regResult)
                    MAKE_ERROR("Недействительный ключ в определении parent");

                std::string realKey;

                if(!regResult->at(1)) {
                    realKey = *key;
                } else {
                    realKey = "core:" + *regResult->at(2);
                }

                DefNodeId parentId;

                // {
                //     auto& list = Content.ContentKeyToId[(int) EnumDefContent::Node];
                //     auto iter = list.find(realKey);
                //     if(iter == list.end())
                //         MAKE_ERROR("Идентификатор parent не найден");

                //     parentId = iter->second;
                // }

                // result = Content.ContentIdToDef_Node.at(parentId);
            }
        }
    }

    {
        std::optional<sol::table> nodestate = profile.get<std::optional<sol::table>>("nodestate");
    }

    {
        std::optional<sol::table> render = profile.get<std::optional<sol::table>>("render");
    }

    {
        std::optional<sol::table> collision = profile.get<std::optional<sol::table>>("collision");
    }

    {
        std::optional<sol::table> events = profile.get<std::optional<sol::table>>("events");
    }

    // result.NodeAdvancementFactory = profile["node_advancement_factory"];
}

void ContentManager::registerBase_World(ResourceId id, const sol::table& profile) {
    std::optional<DefWorld>& world = getEntry_World(id);
    if(!world)
        world.emplace();
}

void ContentManager::registerBase(EnumDefContent type, const std::string& domain, const std::string& key, const sol::table& profile)
{
    ResourceId id = getId(type, domain, key);
    ProfileChanges[(int) type].push_back(id);

    if(type == EnumDefContent::Node)
        registerBase_Node(id, profile);
    else if(type == EnumDefContent::World)
        registerBase_World(id, profile);
    else
        MAKE_ERROR("Не реализовано");
}

void ContentManager::unRegisterBase(EnumDefContent type, const std::string& domain, const std::string& key)
{
    ResourceId id = getId(type, domain, key);
    ProfileChanges[(int) type].push_back(id);
}

void ContentManager::registerModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key, const sol::table& profile)
{
    ResourceId id = getId(type, domain, key);
    ProfileChanges[(int) type].push_back(id);
}

void ContentManager::unRegisterModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key)
{
    ResourceId id = getId(type, domain, key);
    ProfileChanges[(int) type].push_back(id);
}

ContentManager::Out_buildEndProfiles ContentManager::buildEndProfiles() {
    Out_buildEndProfiles result;

    for(int type = 0; type < (int) EnumDefContent::MAX_ENUM; type++) {
        auto& keys = ProfileChanges[type];
        std::sort(keys.begin(), keys.end());
        auto iterErase = std::unique(keys.begin(), keys.end());
        keys.erase(iterErase, keys.end());
    }

    for(ResourceId id : ProfileChanges[(int) EnumDefContent::Voxel]) {

    }

    return result;
}

}