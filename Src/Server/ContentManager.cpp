#include "ContentManager.hpp"
#include "Common/Abstract.hpp"
#include <algorithm>

namespace LV::Server {

ContentManager::ContentManager(AssetsPreloader& am)
    : AM(am)
{
    std::fill(std::begin(NextId), std::end(NextId), 1);
}

ContentManager::~ContentManager() = default;

void ContentManager::registerBase_Node(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile) {
    std::optional<DefNode>& node = getEntry_Node(id);
    if(!node)
        node.emplace();

    DefNode& def = *node;
    def.Domain = domain;
    def.Key = key;

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

void ContentManager::registerBase_World(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile) {
    std::optional<DefWorld>& world = getEntry_World(id);
    if(!world)
        world.emplace();
}

void ContentManager::registerBase_Entity(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile) {
    std::optional<DefEntity>& entity = getEntry_Entity(id);
    if(!entity)
        entity.emplace();

    DefEntity& def = *entity;
    def.Domain = domain;
    def.Key = key;
}

void ContentManager::registerBase(EnumDefContent type, const std::string& domain, const std::string& key, const sol::table& profile)
{
    ResourceId id = getId(type, domain, key);
    ProfileChanges[(int) type].push_back(id);

    if(type == EnumDefContent::Node)
        registerBase_Node(id, domain, key, profile);
    else if(type == EnumDefContent::World)
        registerBase_World(id, domain, key, profile);
    else if(type == EnumDefContent::Entity)
        registerBase_Entity(id, domain, key, profile);
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

void ContentManager::markAllProfilesDirty(EnumDefContent type) {
    const auto &table = ContentKeyToId[(int) type];
    for(const auto& domainPair : table) {
        for(const auto& keyPair : domainPair.second) {
            ProfileChanges[(int) type].push_back(keyPair.second);
        }
    }
}

std::vector<ResourceId> ContentManager::collectProfileIds(EnumDefContent type) const {
    std::vector<ResourceId> ids;
    const auto &table = ContentKeyToId[(int) type];

    for(const auto& domainPair : table) {
        for(const auto& keyPair : domainPair.second) {
            ids.push_back(keyPair.second);
        }
    }

    std::sort(ids.begin(), ids.end());
    auto last = std::unique(ids.begin(), ids.end());
    ids.erase(last, ids.end());
    return ids;
}

ContentManager::Out_buildEndProfiles ContentManager::buildEndProfiles() {
    Out_buildEndProfiles result;

    for(int type = 0; type < (int) EnumDefContent::MAX_ENUM; type++) {
        auto& keys = ProfileChanges[type];
        std::sort(keys.begin(), keys.end());
        auto iterErase = std::unique(keys.begin(), keys.end());
        keys.erase(iterErase, keys.end());
    }

    for(ResourceId id : ProfileChanges[(int) EnumDefContent::Node]) {
        std::optional<DefNode>& node = getEntry_Node(id);
        if(!node) {
            continue;
        }

        auto [nodestateId, assetsModel, assetsTexture]
            = AM.getNodeDependency(node->Domain, node->Key);

        node->NodestateId = nodestateId;
        node->ModelDeps = std::move(assetsModel);
        node->TextureDeps = std::move(assetsTexture);
    }

    return result;
}

}
