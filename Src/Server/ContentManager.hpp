#pragma once

#include "Common/Abstract.hpp"
#include "Server/Abstract.hpp"
#include "Server/AssetsManager.hpp"
#include <sol/table.hpp>
#include <unordered_map>
#include <unordered_set>


namespace LV::Server {

struct DefVoxel_Base { };
struct DefNode_Base { };
struct DefWorld_Base { };
struct DefPortal_Base { };
struct DefEntity_Base { };
struct DefItem_Base { };

struct DefVoxel_Mod { };
struct DefNode_Mod { };
struct DefWorld_Mod { };
struct DefPortal_Mod { };
struct DefEntity_Mod { };
struct DefItem_Mod { };

struct ResourceBase {
    std::string Domain, Key;
};

struct DefVoxel : public ResourceBase { };
struct DefNode : public ResourceBase { };
struct DefWorld : public ResourceBase { };
struct DefPortal : public ResourceBase { };
struct DefEntity : public ResourceBase { };
struct DefItem : public ResourceBase { };

class ContentManager {
    template<typename T>
    struct TableEntry {
        static constexpr size_t ChunkSize = 4096;
        std::array<std::optional<T>, ChunkSize> Entries;
    };


    // Следующие идентификаторы регистрации контента
    ResourceId NextId[(int) EnumDefContent::MAX_ENUM] = {0};
    // Домен -> {ключ -> идентификатор}
    std::unordered_map<std::string, std::unordered_map<std::string, ResourceId>> ContentKeyToId[(int) EnumDefContent::MAX_ENUM];

    // Профили зарегистрированные модами
    std::vector<std::unique_ptr<TableEntry<DefVoxel>>>     Profiles_Base_Voxel;
    std::vector<std::unique_ptr<TableEntry<DefNode>>>       Profiles_Base_Node;
    std::vector<std::unique_ptr<TableEntry<DefWorld>>>     Profiles_Base_World;
    std::vector<std::unique_ptr<TableEntry<DefPortal>>>   Profiles_Base_Portal;
    std::vector<std::unique_ptr<TableEntry<DefEntity>>>   Profiles_Base_Entity;
    std::vector<std::unique_ptr<TableEntry<DefItem>>>       Profiles_Base_Item;

    // Изменения, накладываемые на профили
    // Идентификатор [домен мода модификатора, модификатор]
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefVoxel_Mod>>> Profiles_Mod_Voxel;
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefNode_Mod>>> Profiles_Mod_Node;
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefWorld_Mod>>> Profiles_Mod_World;
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefPortal_Mod>>> Profiles_Mod_Portal;
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefEntity_Mod>>> Profiles_Mod_Entity;
    std::unordered_map<ResourceId, std::vector<std::tuple<std::string, DefItem_Mod>>> Profiles_Mod_Item;

    // Затронутые профили в процессе регистраций
    // По ним будут пересобраны профили
    std::vector<ResourceId> ProfileChanges[(int) EnumDefContent::MAX_ENUM];

    // Конечные профили контента
    std::vector<std::unique_ptr<TableEntry<DefVoxel>>>     Profiles_Voxel;
    std::vector<std::unique_ptr<TableEntry<DefNode>>>       Profiles_Node;
    std::vector<std::unique_ptr<TableEntry<DefWorld>>>     Profiles_World;
    std::vector<std::unique_ptr<TableEntry<DefPortal>>>   Profiles_Portal;
    std::vector<std::unique_ptr<TableEntry<DefEntity>>>   Profiles_Entity;
    std::vector<std::unique_ptr<TableEntry<DefItem>>>       Profiles_Item;

    std::optional<DefVoxel>&    getEntry_Voxel(ResourceId resId)    { return Profiles_Voxel[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    std::optional<DefNode>&     getEntry_Node(ResourceId resId)     { return Profiles_Node[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    std::optional<DefWorld>&    getEntry_World(ResourceId resId)    { return Profiles_World[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    std::optional<DefPortal>&   getEntry_Portal(ResourceId resId)   { return Profiles_Portal[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    std::optional<DefEntity>&   getEntry_Entity(ResourceId resId)   { return Profiles_Entity[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    std::optional<DefItem>&     getEntry_Item(ResourceId resId)     { return Profiles_Item[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];}
    
    ResourceId getId(EnumDefContent type, const std::string& domain, const std::string& key) {
        if(auto iterCKTI = ContentKeyToId[(int) type].find(domain); iterCKTI != ContentKeyToId[(int) type].end()) {
            if(auto iterKey = iterCKTI->second.find(key); iterKey != iterCKTI->second.end()) {
                return iterKey->second;
            }
        }

        ResourceId resId = NextId[(int) type]++;
        ContentKeyToId[(int) type][domain][key] = resId;

        switch(type) {
        case EnumDefContent::Voxel:
            if(resId >= Profiles_Voxel.size()*TableEntry<DefVoxel>::ChunkSize) 
                Profiles_Voxel.push_back(std::make_unique<TableEntry<DefVoxel>>()); 
            break;
        case EnumDefContent::Node:
            if(resId >= Profiles_Node.size()*TableEntry<DefNode>::ChunkSize) 
                Profiles_Node.push_back(std::make_unique<TableEntry<DefNode>>()); 
            break;
        case EnumDefContent::World:
            if(resId >= Profiles_World.size()*TableEntry<DefWorld>::ChunkSize) 
                Profiles_World.push_back(std::make_unique<TableEntry<DefWorld>>()); 
            break;
        case EnumDefContent::Portal:
            if(resId >= Profiles_Portal.size()*TableEntry<DefPortal>::ChunkSize) 
                Profiles_Portal.push_back(std::make_unique<TableEntry<DefPortal>>()); 
            break;
        case EnumDefContent::Entity:
            if(resId >= Profiles_Entity.size()*TableEntry<DefEntity>::ChunkSize) 
                Profiles_Entity.push_back(std::make_unique<TableEntry<DefEntity>>()); 
            break;
        case EnumDefContent::Item:
            if(resId >= Profiles_Item.size()*TableEntry<DefItem>::ChunkSize) 
                Profiles_Item.push_back(std::make_unique<TableEntry<DefItem>>()); 
            break;
        default:
            assert(false);
        }

        return resId;
    }

    void registerBase_Node(ResourceId id, const sol::table& profile);
    void registerBase_World(ResourceId id, const sol::table& profile);

public:
    ContentManager(asio::io_context& ioc);
    ~ContentManager();

    // Регистрирует определение контента
    void registerBase(EnumDefContent type, const std::string& domain, const std::string& key, const sol::table& profile);
    void unRegisterBase(EnumDefContent type, const std::string& domain, const std::string& key);
    // Регистрация модификатора предмета модом
    void registerModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key, const sol::table& profile);
    void unRegisterModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key);
    // Компилирует изменённые профили
    struct Out_buildEndProfiles {
        std::vector<ResourceId> ChangedProfiles[(int) EnumDefContent::MAX_ENUM]; 
    };

    Out_buildEndProfiles buildEndProfiles();

    std::optional<DefVoxel*> getProfile_Voxel(ResourceId id) {
        assert(id < Profiles_Voxel.size()*TableEntry<DefVoxel>::ChunkSize);
        auto& value = Profiles_Voxel[id / TableEntry<DefVoxel>::ChunkSize]->Entries[id % TableEntry<DefVoxel>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }

    std::optional<DefNode*> getProfile_Node(ResourceId id) {
        assert(id < Profiles_Node.size()*TableEntry<DefNode>::ChunkSize);
        auto& value = Profiles_Node[id / TableEntry<DefNode>::ChunkSize]->Entries[id % TableEntry<DefNode>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }

    std::optional<DefWorld*> getProfile_World(ResourceId id) {
        assert(id < Profiles_World.size()*TableEntry<DefWorld>::ChunkSize);
        auto& value = Profiles_World[id / TableEntry<DefWorld>::ChunkSize]->Entries[id % TableEntry<DefWorld>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }

    std::optional<DefPortal*> getProfile_Portal(ResourceId id) {
        assert(id < Profiles_Portal.size()*TableEntry<DefPortal>::ChunkSize);
        auto& value = Profiles_Portal[id / TableEntry<DefPortal>::ChunkSize]->Entries[id % TableEntry<DefPortal>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }

    std::optional<DefEntity*> getProfile_Entity(ResourceId id) {
        assert(id < Profiles_Entity.size()*TableEntry<DefEntity>::ChunkSize);
        auto& value = Profiles_Entity[id / TableEntry<DefEntity>::ChunkSize]->Entries[id % TableEntry<DefEntity>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }

    std::optional<DefItem*> getProfile_Item(ResourceId id) {
        assert(id < Profiles_Item.size()*TableEntry<DefItem>::ChunkSize);
        auto& value = Profiles_Item[id / TableEntry<DefItem>::ChunkSize]->Entries[id % TableEntry<DefItem>::ChunkSize];
        if(value)
            return {&*value};
        else
            return std::nullopt;
    }
};

}