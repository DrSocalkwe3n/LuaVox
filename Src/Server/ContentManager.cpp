#include "ContentManager.hpp"
#include "Common/Abstract.hpp"
#include <algorithm>
#include <optional>
#include <utility>

namespace LV::Server {

ContentManager::ContentManager(AssetsManager& am)
    : AM(am)
{
}

ContentManager::~ContentManager() = default;

void ContentManager::registerBase_Node(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile) {
    std::optional<DefNode_Base>* basePtr;

    {
        size_t entryIndex = id / TableEntry<DefNode_Base>::ChunkSize;
        size_t entryId = id % TableEntry<DefNode_Base>::ChunkSize;

        size_t need = entryIndex+1-Profiles_Base_Node.size();
        for(size_t iter = 0; iter < need; iter++) {
            Profiles_Base_Node.emplace_back(std::make_unique<TableEntry<DefNode_Base>>());
        }

        basePtr = &Profiles_Base_Node[entryIndex]->Entries[entryId];
        *basePtr = DefNode_Base();
    }

    DefNode_Base& def = **basePtr;

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
    ProfileChanges[static_cast<size_t>(type)].push_back(id);

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

// void ContentManager::markAllProfilesDirty(EnumDefContent type) {
//     const auto &table = this->idToDK()[(int) type];
//     size_t counter = 0;
//     for(const auto& [domain, key] : table) {
//         ProfileChanges[static_cast<size_t>(type)].push_back(counter++);
//     }
// }

template<class type, class modType>
void ContentManager::buildEndProfilesByType(auto& profiles, auto enumType, auto& base, auto& keys, auto& result, auto& modsTable) {
    // Расширяем таблицу итоговых профилей до нужного количества
    if(!keys.empty()) {
        size_t need = keys.back() / TableEntry<type>::ChunkSize;
        if(need >= profiles.size()) {
            profiles.reserve(need);

            for(size_t iter = 0; iter <= need-profiles.size(); ++iter)
                profiles.emplace_back(std::make_unique<TableEntry<type>>());
        }
    }

    TOS::Logger("CM").debug() << "type: " << static_cast<size_t>(enumType);

    // Пересчитываем профили
    for(size_t id : keys) {
        size_t entryIndex = id / TableEntry<type>::ChunkSize;
        size_t subIndex = id % TableEntry<type>::ChunkSize;

        if(
            entryIndex >= base.size()
            || !base[entryIndex]->Entries[subIndex]
        ) {
            // Базовый профиль не существует
            profiles[entryIndex]->Entries[subIndex] = std::nullopt;
            // Уведомляем о потере профиля
            result.LostProfiles[static_cast<size_t>(enumType)].push_back(id);
        } else {
            // Собираем конечный профиль
            std::vector<std::tuple<std::string, modType>> mods_default, *mods = &mods_default;
            auto iter = modsTable.find(id);
            if(iter != modsTable.end())
                mods = &iter->second;

            std::optional<BindDomainKeyInfo> dk = getDK(enumType, id);
            assert(dk);
            TOS::Logger("CM").debug() << "\t" << dk->Domain << ":" << dk->Key << " -> " << id;
            profiles[entryIndex]->Entries[subIndex] = base[entryIndex]->Entries[subIndex]->compile(AM, *this, dk->Domain, dk->Key, *mods);
        }
    }
}

ContentManager::Out_buildEndProfiles ContentManager::buildEndProfiles() {
    Out_buildEndProfiles result;

    for(int type = 0; type < (int) EnumDefContent::MAX_ENUM; type++) {
        std::shared_lock lock(Profiles_Mtx[type]);
        auto& keys = ProfileChanges[type];
        std::sort(keys.begin(), keys.end());
        auto iterErase = std::unique(keys.begin(), keys.end());
        keys.erase(iterErase, keys.end());

        switch(type) {
            case 0: buildEndProfilesByType<DefVoxel, DefVoxel_Mod>      (Profiles_Voxel,    EnumDefContent::Voxel,  Profiles_Base_Voxel,    keys, result, Profiles_Mod_Voxel);  break;
            case 1: buildEndProfilesByType<DefNode, DefNode_Mod>        (Profiles_Node,     EnumDefContent::Node,   Profiles_Base_Node,     keys, result, Profiles_Mod_Node);   break;
            case 2: buildEndProfilesByType<DefWorld, DefWorld_Mod>      (Profiles_World,    EnumDefContent::World,  Profiles_Base_World,    keys, result, Profiles_Mod_World);  break;
            case 3: buildEndProfilesByType<DefPortal, DefPortal_Mod>    (Profiles_Portal,   EnumDefContent::Portal, Profiles_Base_Portal,   keys, result, Profiles_Mod_Portal); break;
            case 4: buildEndProfilesByType<DefEntity, DefEntity_Mod>    (Profiles_Entity,   EnumDefContent::Entity, Profiles_Base_Entity,   keys, result, Profiles_Mod_Entity); break;
            case 5: buildEndProfilesByType<DefItem, DefItem_Mod>        (Profiles_Item,     EnumDefContent::Item,   Profiles_Base_Item,     keys, result, Profiles_Mod_Item);   break;
            default: std::unreachable();
        }
    }

    return result;
}

ContentManager::Out_getAllProfiles ContentManager::getAllProfiles() {
    Out_getAllProfiles result;

    size_t counter;
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Voxel)]);
        result.ProfilesIds_Voxel.reserve(Profiles_Voxel.size()*TableEntry<DefVoxel>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_Voxel)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_Voxel.push_back(id);
            }

        result.ProfilesIds_Voxel.shrink_to_fit();
        result.Profiles_Voxel.reserve(result.ProfilesIds_Voxel.size());
        for(const auto& entry : Profiles_Voxel)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_Voxel.push_back(&item.value());
    }
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Node)]);
        result.ProfilesIds_Node.reserve(Profiles_Node.size()*TableEntry<DefNode>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_Node)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_Node.push_back(id);
            }

        result.ProfilesIds_Node.shrink_to_fit();
        result.Profiles_Node.reserve(result.ProfilesIds_Node.size());
        for(const auto& entry : Profiles_Node)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_Node.push_back(&item.value());
    }
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::World)]);
        result.ProfilesIds_World.reserve(Profiles_World.size()*TableEntry<DefWorld>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_World)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_World.push_back(id);
            }

        result.ProfilesIds_World.shrink_to_fit();
        result.Profiles_World.reserve(result.ProfilesIds_World.size());
        for(const auto& entry : Profiles_World)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_World.push_back(&item.value());
    }
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Portal)]);
        result.ProfilesIds_Portal.reserve(Profiles_Portal.size()*TableEntry<DefPortal>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_Portal)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_Portal.push_back(id);
            }

        result.ProfilesIds_Portal.shrink_to_fit();
        result.Profiles_Portal.reserve(result.ProfilesIds_Portal.size());
        for(const auto& entry : Profiles_Portal)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_Portal.push_back(&item.value());
    }
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Entity)]);
        result.ProfilesIds_Entity.reserve(Profiles_Entity.size()*TableEntry<DefEntity>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_Entity)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_Entity.push_back(id);
            }

        result.ProfilesIds_Entity.shrink_to_fit();
        result.Profiles_Entity.reserve(result.ProfilesIds_Entity.size());
        for(const auto& entry : Profiles_Entity)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_Entity.push_back(&item.value());
    }
    
    {
        std::shared_lock lock(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Item)]);
        result.ProfilesIds_Item.reserve(Profiles_Item.size()*TableEntry<DefItem>::ChunkSize);
        counter = 0;
        for(const auto& entry : Profiles_Item)
            for(const auto& item : entry->Entries) {
                size_t id = counter++;
                if(item)
                    result.ProfilesIds_Item.push_back(id);
            }

        result.ProfilesIds_Item.shrink_to_fit();
        result.Profiles_Item.reserve(result.ProfilesIds_Item.size());
        for(const auto& entry : Profiles_Item)
            for(const auto& item : entry->Entries)
                if(item)
                    result.Profiles_Item.push_back(&item.value());
    }

    result.IdToDK = idToDK();

    return result;
}

}
