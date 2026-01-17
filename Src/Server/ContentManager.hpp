#pragma once

#include "Common/Abstract.hpp"
#include "AssetsManager.hpp"
#include "Common/IdProvider.hpp"
#include "Common/Net.hpp"
#include "TOSLib.hpp"
#include <array>
#include <mutex>
#include <sol/table.hpp>
#include <string_view>
#include <unordered_map>


namespace LV::Server {

struct ResourceBase {
    std::string Domain, Key;
};

class ContentManager;

struct DefVoxel : public ResourceBase {
    std::u8string dumpToClient() const {
        return {};
    }
};

struct DefNode : public ResourceBase {
    AssetsNodestate NodestateId;

    std::u8string dumpToClient() const {
        auto wr = TOS::ByteBuffer::Writer();
        wr << uint32_t(NodestateId);
        auto buff = wr.complite();
        return (std::u8string) std::u8string_view((const char8_t*) buff.data(), buff.size());
    }
};
struct DefWorld : public ResourceBase {
    std::u8string dumpToClient() const {
        return {};
    }
};

struct DefPortal : public ResourceBase {
    std::u8string dumpToClient() const {
        return {};
    }
};

struct DefEntity : public ResourceBase {
    std::u8string dumpToClient() const {
        return {};
    }
};

struct DefItem : public ResourceBase {
    std::u8string dumpToClient() const {
        return {};
    }
};

struct DefVoxel_Mod { };
struct DefNode_Mod { };
struct DefWorld_Mod { };
struct DefPortal_Mod { };
struct DefEntity_Mod { };
struct DefItem_Mod { };

struct DefVoxel_Base {
private:
    friend ContentManager;
    DefVoxel compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefVoxel_Mod>>& mods) const {
        return DefVoxel();
    }
};

struct DefNode_Base {
private:
    friend ContentManager;
    DefNode compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefNode_Mod>>& mods) const {
        DefNode profile;
        std::string jsonKey = std::string(key)+".json";
        profile.NodestateId = am.getId(EnumAssets::Nodestate, domain, jsonKey);
        TOS::Logger("Compile").info() << domain << ' ' << key << " -> " << profile.NodestateId;
        return profile;
    }
};

struct DefWorld_Base {
private:
    friend ContentManager;
    DefWorld compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefWorld_Mod>>& mods) const {
        return DefWorld();
    }
};

struct DefPortal_Base {
private:
    friend ContentManager;
    DefPortal compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefPortal_Mod>>& mods) const {
        return DefPortal();
    }
};

struct DefEntity_Base {
private:
    friend ContentManager;
    DefEntity compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefEntity_Mod>>& mods) const {
        return DefEntity();
    }
};

struct DefItem_Base {
private:
    friend ContentManager;
    DefItem compile(AssetsManager& am, ContentManager& cm, const std::string_view domain, const std::string_view key, const std::vector<std::tuple<std::string, DefItem_Mod>>& mods) const {
        return DefItem();
    }
};

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

        std::unordered_map<DefVoxelId, std::optional<DefVoxel>*> Profiles_Voxel;
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
    // void markAllProfilesDirty(EnumDefContent type);
    // Список всех зарегистрированных профилей выбранного типа
    std::vector<ResourceId> collectProfileIds(EnumDefContent type) const;
    // Компилирует изменённые профили
    struct Out_buildEndProfiles {
        std::vector<
            std::tuple<DefVoxelId, const DefVoxel*>
        > ChangedProfiles_Voxel;

        std::vector<
            std::tuple<DefNodeId, const DefNode*>
        > ChangedProfiles_Node;

        std::vector<
            std::tuple<DefWorldId, const DefWorld*>
        > ChangedProfiles_World;

        std::vector<
            std::tuple<DefPortalId, const DefPortal*>
        > ChangedProfiles_Portal;

        std::vector<
            std::tuple<DefEntityId, const DefEntity*>
        > ChangedProfiles_Entity;

        std::vector<
            std::tuple<DefItemId, const DefItem*>
        > ChangedProfiles_Item;
        
        std::array<
            std::vector<ResourceId>,
            MAX_ENUM
        > LostProfiles;

        std::array<
            std::vector<BindDomainKeyInfo>,
            static_cast<size_t>(EnumDefContent::MAX_ENUM)
        > IdToDK;
    };

    // Компилирует конечные профили по базе и модификаторам (предоставляет клиентам изменённые и потерянные)
    Out_buildEndProfiles buildEndProfiles();

    struct Out_getAllProfiles {
        std::vector<DefVoxelId> ProfilesIds_Voxel;
        std::vector<const DefVoxel*> Profiles_Voxel;

        std::vector<DefNodeId> ProfilesIds_Node;
        std::vector<const DefNode*> Profiles_Node;

        std::vector<DefWorldId> ProfilesIds_World;
        std::vector<const DefWorld*> Profiles_World;

        std::vector<DefPortalId> ProfilesIds_Portal;
        std::vector<const DefPortal*> Profiles_Portal;

        std::vector<DefEntityId> ProfilesIds_Entity;
        std::vector<const DefEntity*> Profiles_Entity;

        std::vector<DefItemId> ProfilesIds_Item;
        std::vector<const DefItem*> Profiles_Item;

        std::array<
            std::vector<BindDomainKeyInfo>,
            static_cast<size_t>(EnumDefContent::MAX_ENUM)
        > IdToDK;
    };

    // Выдаёт все профили (для новых клиентов)
    Out_getAllProfiles getAllProfiles();
    

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

    template<class type, class modType>
    void buildEndProfilesByType(auto& profiles, auto enumType, auto& base, auto& keys, auto& result, auto& mods);

    TOS::Logger LOG = "Server>ContentManager";
    AssetsManager& AM;

    // Профили зарегистрированные модами
    std::vector<std::unique_ptr<TableEntry<DefVoxel_Base>>>     Profiles_Base_Voxel;
    std::vector<std::unique_ptr<TableEntry<DefNode_Base>>>       Profiles_Base_Node;
    std::vector<std::unique_ptr<TableEntry<DefWorld_Base>>>     Profiles_Base_World;
    std::vector<std::unique_ptr<TableEntry<DefPortal_Base>>>   Profiles_Base_Portal;
    std::vector<std::unique_ptr<TableEntry<DefEntity_Base>>>   Profiles_Base_Entity;
    std::vector<std::unique_ptr<TableEntry<DefItem_Base>>>       Profiles_Base_Item;

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
