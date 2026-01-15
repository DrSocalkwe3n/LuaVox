#pragma once

#include "Common/Abstract.hpp"
#include "AssetsManager.hpp"
#include "Common/IdProvider.hpp"
#include <array>
#include <mutex>
#include <sol/table.hpp>
#include <unordered_map>


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
struct DefNode : public ResourceBase {
    AssetsNodestate NodestateId;
    std::vector<AssetsModel> ModelDeps;
    std::vector<AssetsTexture> TextureDeps;
};
struct DefWorld : public ResourceBase { };
struct DefPortal : public ResourceBase { };
struct DefEntity : public ResourceBase { };
struct DefItem : public ResourceBase { };

/*
    DK to id
    id to profile
*/

class ContentManager : public IdProvider<EnumDefContent> {
public:
    class LRU {
    public:
        LRU(ContentManager& cm)
        : CM(&cm)
        {
        }

        LRU(const LRU&) = default;
        LRU(LRU&&) = default;
        LRU& operator=(const LRU&) = default;
        LRU& operator=(LRU&&) = default;

        ResourceId getId(EnumDefContent type, const std::string_view domain, const std::string_view key) {
            auto iter = DKToId[static_cast<size_t>(type)].find(BindDomainKeyViewInfo(domain, key));
            if(iter == DKToId[static_cast<size_t>(type)].end()) {
                ResourceId id = CM->getId(type, domain, key);
                DKToId[static_cast<size_t>(type)].emplace_hint(iter, BindDomainKeyInfo((std::string) domain, (std::string) key), id);
                return id;
            }

            return iter->second;

            // switch(type) {
            // case EnumDefContent::Voxel:

            // case EnumDefContent::Node:
            // case EnumDefContent::World:
            // case EnumDefContent::Portal:
            // case EnumDefContent::Entity:
            // case EnumDefContent::Item:
            // default:
            //     std::unreachable();
            // }
        }

        ResourceId getIdVoxel(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::Voxel, domain, key);
        }

        ResourceId getIdNode(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::Node, domain, key);
        }

        ResourceId getIdWorld(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::World, domain, key);
        }

        ResourceId getIdPortal(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::Portal, domain, key);
        }

        ResourceId getIdEntity(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::Entity, domain, key);
        }

        ResourceId getIdItem(const std::string_view domain, const std::string_view key) {
            return getId(EnumDefContent::Item, domain, key);
        }

    private:
        ContentManager* CM;

        std::array<
            ankerl::unordered_dense::map<BindDomainKeyInfo, ResourceId, KeyHash, KeyEq>,
            MAX_ENUM
        > DKToId;

        std::unordered_map<DefVoxelId, std::optional<DefItem>*> Profiles_Voxel;
        std::unordered_map<DefNodeId, std::optional<DefNode>*> Profiles_Node;
        std::unordered_map<DefWorldId, std::optional<DefWorld>*> Profiles_World;
        std::unordered_map<DefPortalId, std::optional<DefPortal>*> Profiles_Portal;
        std::unordered_map<DefEntityId, std::optional<DefEntity>*> Profiles_Entity;
        std::unordered_map<DefItemId, std::optional<DefItem>*> Profiles_Item;
    };

public:
    ContentManager(AssetsManager &am);
    ~ContentManager();

    // Регистрирует определение контента
    void registerBase(EnumDefContent type, const std::string& domain, const std::string& key, const sol::table& profile);
    void unRegisterBase(EnumDefContent type, const std::string& domain, const std::string& key);
    // Регистрация модификатора предмета модом
    void registerModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key, const sol::table& profile);
    void unRegisterModifier(EnumDefContent type, const std::string& mod, const std::string& domain, const std::string& key);
    // Пометить все профили типа как изменённые (например, после перезагрузки ассетов)
    void markAllProfilesDirty(EnumDefContent type);
    // Список всех зарегистрированных профилей выбранного типа
    std::vector<ResourceId> collectProfileIds(EnumDefContent type) const;
    // Компилирует изменённые профили
    struct Out_buildEndProfiles {
        std::vector<ResourceId> ChangedProfiles[MAX_ENUM]; 
    };

    Out_buildEndProfiles buildEndProfiles();
    

    std::optional<DefVoxel>& getEntry_Voxel(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Voxel)]);

        assert(resId / TableEntry<DefVoxel>::ChunkSize <= Profiles_Voxel.size());
        return Profiles_Voxel[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefVoxel>& getEntry_Voxel(const std::string_view domain, const std::string_view key) {
        return getEntry_Voxel(getId(EnumDefContent::Voxel, domain, key));
    }

    std::optional<DefNode>& getEntry_Node(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Node)]);

        assert(resId / TableEntry<DefNode>::ChunkSize < Profiles_Node.size());
        return Profiles_Node[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefNode>& getEntry_Node(const std::string_view domain, const std::string_view key) {
        return getEntry_Node(getId(EnumDefContent::Node, domain, key));
    }

    std::optional<DefWorld>& getEntry_World(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::World)]);

        assert(resId / TableEntry<DefWorld>::ChunkSize < Profiles_World.size());
        return Profiles_World[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefWorld>& getEntry_World(const std::string_view domain, const std::string_view key) {
        return getEntry_World(getId(EnumDefContent::World, domain, key));
    }

    std::optional<DefPortal>& getEntry_Portal(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Portal)]);

        assert(resId / TableEntry<DefPortal>::ChunkSize < Profiles_Portal.size());
        return Profiles_Portal[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefPortal>& getEntry_Portal(const std::string_view domain, const std::string_view key) {
        return getEntry_Portal(getId(EnumDefContent::Portal, domain, key));
    }

    std::optional<DefEntity>& getEntry_Entity(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Entity)]);

        assert(resId / TableEntry<DefEntity>::ChunkSize < Profiles_Entity.size());
        return Profiles_Entity[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefEntity>& getEntry_Entity(const std::string_view domain, const std::string_view key) {
        return getEntry_Entity(getId(EnumDefContent::Entity, domain, key));
    }

    std::optional<DefItem>& getEntry_Item(ResourceId resId) {
        std::shared_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Item)]);

        assert(resId / TableEntry<DefItem>::ChunkSize < Profiles_Item.size());
        return Profiles_Item[resId / TableEntry<DefVoxel>::ChunkSize]->Entries[resId % TableEntry<DefVoxel>::ChunkSize];
    }

    std::optional<DefItem>& getEntry_Item(const std::string_view domain, const std::string_view key) {
        return getEntry_Item(getId(EnumDefContent::Item, domain, key));
    }
    
    ResourceId getId(EnumDefContent type, const std::string_view domain, const std::string_view key) {
        ResourceId resId = IdProvider::getId(type, domain, key);

        switch(type) {
        case EnumDefContent::Voxel:
            if(resId >= Profiles_Voxel.size()*TableEntry<DefVoxel>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Voxel)]);
                if(resId >= Profiles_Voxel.size()*TableEntry<DefVoxel>::ChunkSize)
                    Profiles_Voxel.push_back(std::make_unique<TableEntry<DefVoxel>>());
            }
            break;
        case EnumDefContent::Node:
            if(resId >= Profiles_Node.size()*TableEntry<DefNode>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Node)]);
                if(resId >= Profiles_Node.size()*TableEntry<DefNode>::ChunkSize)
                    Profiles_Node.push_back(std::make_unique<TableEntry<DefNode>>());
            }
            break;
        case EnumDefContent::World:
            if(resId >= Profiles_World.size()*TableEntry<DefWorld>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::World)]);
                if(resId >= Profiles_World.size()*TableEntry<DefWorld>::ChunkSize)
                    Profiles_World.push_back(std::make_unique<TableEntry<DefWorld>>());
            }
            break;
        case EnumDefContent::Portal:
            if(resId >= Profiles_Portal.size()*TableEntry<DefPortal>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Portal)]);
                if(resId >= Profiles_Portal.size()*TableEntry<DefPortal>::ChunkSize)
                    Profiles_Portal.push_back(std::make_unique<TableEntry<DefPortal>>());
            }
            break;
        case EnumDefContent::Entity:
            if(resId >= Profiles_Entity.size()*TableEntry<DefEntity>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Entity)]);
                if(resId >= Profiles_Entity.size()*TableEntry<DefEntity>::ChunkSize)
                    Profiles_Entity.push_back(std::make_unique<TableEntry<DefEntity>>());
            }
            break;
        case EnumDefContent::Item:
            if(resId >= Profiles_Item.size()*TableEntry<DefItem>::ChunkSize) {
                std::unique_lock mtx(Profiles_Mtx[static_cast<size_t>(EnumDefContent::Item)]);
                if(resId >= Profiles_Item.size()*TableEntry<DefItem>::ChunkSize)
                    Profiles_Item.push_back(std::make_unique<TableEntry<DefItem>>());
            }
            break;
        default:
            std::unreachable();
        }

        return resId;
    }

    LRU createLRU() {
        return {*this};
    }

private:
    template<typename T>
    struct TableEntry {
        static constexpr size_t ChunkSize = 4096;
        std::array<std::optional<T>, ChunkSize> Entries;
    };

    void registerBase_Node(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile);
    void registerBase_World(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile);
    void registerBase_Entity(ResourceId id, const std::string& domain, const std::string& key, const sol::table& profile);


    TOS::Logger LOG = "Server>ContentManager";
    AssetsManager& AM;

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
    std::vector<ResourceId> ProfileChanges[MAX_ENUM];

    // Конечные профили контента
    std::array<std::shared_mutex, MAX_ENUM> Profiles_Mtx;
    std::vector<std::unique_ptr<TableEntry<DefVoxel>>>     Profiles_Voxel;
    std::vector<std::unique_ptr<TableEntry<DefNode>>>       Profiles_Node;
    std::vector<std::unique_ptr<TableEntry<DefWorld>>>     Profiles_World;
    std::vector<std::unique_ptr<TableEntry<DefPortal>>>   Profiles_Portal;
    std::vector<std::unique_ptr<TableEntry<DefEntity>>>   Profiles_Entity;
    std::vector<std::unique_ptr<TableEntry<DefItem>>>       Profiles_Item;
};

}
