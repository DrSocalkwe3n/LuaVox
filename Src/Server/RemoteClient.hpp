#pragma once

#include <TOSLib.hpp>
#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include "Common/Packets.hpp"
#include "Server/AssetsManager.hpp"
#include "Server/ContentManager.hpp"
#include <Common/Abstract.hpp>
#include <bitset>
#include <initializer_list>
#include <queue>
#include <type_traits>
#include <unordered_map>

namespace LV::Server {

class World;

template<typename ServerKey, typename ClientKey, std::enable_if_t<sizeof(ServerKey) >= sizeof(ClientKey), int> = 0>
class CSChunkedMapper {
    std::unordered_map<uint32_t, std::tuple<std::bitset<64>, std::array<ServerKey, 64>>> Chunks;

public:
    ServerKey toServer(ClientKey cKey) {
        int chunkIndex = cKey >> 6;
        int subIndex = cKey & 0x3f;

        auto iChunk = Chunks.find(chunkIndex);
        assert(iChunk != Chunks.end() && "Идентификатор уже занят");

        std::bitset<64> &bits = std::get<0>(iChunk.second);
        std::array<ServerKey, 64> &keys = std::get<1>(iChunk.second);

        assert(bits.test(subIndex) && "Идентификатор уже занят");

        return keys[subIndex];
    }

    void erase(ClientKey cKey) {
        int chunkIndex = cKey >> 6;
        int subIndex = cKey & 0x3f;

        auto iChunk = Chunks.find(chunkIndex);
        if(iChunk == Chunks.end())
            MAKE_ERROR("Идентификатор не привязан");

        std::bitset<64> &bits = std::get<0>(iChunk->second);
        std::array<ServerKey, 64> &keys = std::get<1>(iChunk->second);

        assert(bits.test(subIndex) && "Идентификатор уже занят");

        bits.reset(subIndex);
    }

    void link(ClientKey cKey, ServerKey sKey) {
        int chunkIndex = cKey >> 6;
        int subIndex = cKey & 0x3f;

        std::tuple<std::bitset<64>, std::array<ServerKey, 64>> &chunk = Chunks[chunkIndex];
        std::bitset<64> &bits = std::get<0>(chunk);
        std::array<ServerKey, 64> &keys = std::get<1>(chunk);

        assert(!bits.test(subIndex) && "Идентификатор уже занят");

        bits.set(subIndex);
        keys[subIndex] = sKey;
    }
};

template<typename ServerKey, typename ClientKey, std::enable_if_t<sizeof(ServerKey) >= sizeof(ClientKey), int> = 0>
class SCSKeyRemapper {
    std::bitset<sizeof(ClientKey)*8-1> FreeClientKeys;
    std::map<ServerKey, ClientKey> Map;
    CSChunkedMapper<ServerKey, ClientKey> CSmapper;

public:
    SCSKeyRemapper() {
        FreeClientKeys.set();
    }

    // Соотнести идентификатор на стороне сервера с идентификатором на стороне клиента 
    ClientKey toClient(ServerKey skey) {
        auto iter = Map.find(skey);
        if(iter == Map.end()) {
            // Идентификатор отсутствует, нужно его занять
            // Ищет позицию ближайшего бита 1
            size_t pos = FreeClientKeys._Find_first();
            if(pos == FreeClientKeys.size())
                return ClientKey(0); // Свободные идентификаторы отсутствуют

            ClientKey ckey = ClientKey(pos+1);
            Map[skey] = ckey;
            CSmapper.link(ckey, skey);
            FreeClientKeys.reset(pos);
            return ClientKey(pos);
        }

        return iter->second;
    }

    // Соотнести идентификатор на стороне клиента с идентификатором на стороне сервера
    ServerKey toServer(ClientKey ckey) {
        return CSmapper.toServer(ckey);
    }

    // Удаляет серверный идентификатор, освобождая идентификатор клиента  
    ClientKey erase(ServerKey skey) {
        auto iter = Map.find(skey);

        if(iter == Map.end())
            return 0;

        ClientKey ckey = iter->second;
        CSmapper.erase(ckey);
        Map.erase(iter);
        FreeClientKeys.set(ckey-1);

        return ckey;
    }

    void rebindClientKey(ServerKey prev, ServerKey next) {
        auto iter = Map.find(prev);

        assert(iter != Map.end() && "Идентификатор не найден");
        ClientKey ckey = iter->second;
        CSmapper.erase(ckey);
        CSmapper.link(ckey, next);
        Map.erase(iter);
        Map[next] = ckey;
    }
};

/* 
    Шаблоны игрового контента, которые необходимо поддерживать в актуальном
    состоянии для клиента и шаблоны, которые клиенту уже не нужны.
    Соответствующие менеджеры ресурсов будут следить за изменениями
    этих ресурсов и переотправлять их клиенту

    Информация о двоичных ресурсах будет получена сразу же при их запросе.
    Действительная отправка ресурсов будет только по запросу клиента.

*/
struct ResourceRequest {
    std::vector<Hash_t>           Hashes;
    std::vector<ResourceId>     AssetsInfo[(int) EnumAssets::MAX_ENUM];

    std::vector<DefVoxelId>        Voxel;
    std::vector<DefNodeId>          Node;
    std::vector<DefWorldId>        World;
    std::vector<DefPortalId>      Portal;
    std::vector<DefEntityId>      Entity;
    std::vector<DefItemId>          Item;

    void insert(const ResourceRequest &obj) {
        Hashes.insert(Hashes.end(), obj.Hashes.begin(), obj.Hashes.end());
        for(int iter = 0; iter < (int) EnumAssets::MAX_ENUM; iter++)
            AssetsInfo[iter].insert(AssetsInfo[iter].end(), obj.AssetsInfo[iter].begin(), obj.AssetsInfo[iter].end());

        Voxel.insert(Voxel.end(), obj.Voxel.begin(), obj.Voxel.end());
        Node.insert(Node.end(), obj.Node.begin(), obj.Node.end());
        World.insert(World.end(), obj.World.begin(), obj.World.end());
        Portal.insert(Portal.end(), obj.Portal.begin(), obj.Portal.end());
        Entity.insert(Entity.end(), obj.Entity.begin(), obj.Entity.end());
        Item.insert(Item.end(), obj.Item.begin(), obj.Item.end());
    }

    void uniq() {
        for(std::vector<ResourceId> *vec : {&Voxel, &Node, &World,
                &Portal, &Entity, &Item
            })
        {
            std::sort(vec->begin(), vec->end());
            auto last = std::unique(vec->begin(), vec->end());
            vec->erase(last, vec->end());
        }

        for(int type = 0; type < (int) EnumAssets::MAX_ENUM; type++)
        {
            std::sort(AssetsInfo[type].begin(), AssetsInfo[type].end());
            auto last = std::unique(AssetsInfo[type].begin(), AssetsInfo[type].end());
            AssetsInfo[type].erase(last, AssetsInfo[type].end());
        }

        std::sort(Hashes.begin(), Hashes.end());
        auto last = std::unique(Hashes.begin(), Hashes.end());
        Hashes.erase(last, Hashes.end());
    }
};

// using EntityKey = std::tuple<WorldId_c, Pos::GlobalRegion>;






/*
    Обработчик сокета клиента.
    Подписывает клиента на отслеживание необходимых ресурсов
    на основе передаваемых клиенту данных
*/
class RemoteClient {
    TOS::Logger LOG;
    DestroyLock UseLock;
    Net::AsyncSocket Socket;
    bool IsConnected = true, IsGoingShutdown = false;

    struct NetworkAndResource_t {
        struct ResUses_t {
            // Счётчики использования двоичных кэшируемых ресурсов + хэш привязанный к идентификатору
            // Хэш используется для того, чтобы исключить повторные объявления неизменившихся ресурсов
            std::map<ResourceId,      std::pair<uint32_t, Hash_t>>      AssetsUse[(int) EnumAssets::MAX_ENUM];

            // Зависимость профилей контента от профилей ресурсов
            // Нужно чтобы пересчитать зависимости к профилям ресурсов
            struct RefAssets_t {
                std::vector<ResourceId> Resources[(int) EnumAssets::MAX_ENUM];
            };

            std::map<DefVoxelId, RefAssets_t>           RefDefVoxel;
            std::map<DefNodeId, RefAssets_t>            RefDefNode;
            std::map<WorldId_t, RefAssets_t>            RefDefWorld;
            std::map<DefPortalId, RefAssets_t>          RefDefPortal;
            std::map<DefEntityId, RefAssets_t>          RefDefEntity;
            std::map<DefItemId, RefAssets_t>            RefDefItem;

            // Счётчики использование профилей контента
            std::map<DefVoxelId,      uint32_t>        DefVoxel; // Один чанк, одно использование
            std::map<DefNodeId,       uint32_t>         DefNode;
            std::map<DefWorldId,      uint32_t>        DefWorld;
            std::map<DefPortalId,     uint32_t>       DefPortal;
            std::map<DefEntityId,     uint32_t>       DefEntity;
            std::map<DefItemId,       uint32_t>         DefItem; // При передаче инвентарей?


            // Зависимость наблюдаемых чанков от профилей нод и вокселей
            struct ChunkRef {
                // Отсортированные списки уникальных вокселей
                std::vector<DefVoxelId> Voxel;
                std::vector<DefNodeId> Node;
            };
            
            std::map<WorldId_t, std::map<Pos::GlobalRegion, std::array<ChunkRef, 4*4*4>>> RefChunk;

            // Модификационные зависимости экземпляров профилей контента
            // У сущностей в мире могут дополнительно изменятся свойства, переписывая их профиль
            struct RefWorld_t {
                DefWorldId Profile;
                RefAssets_t Assets;
            };
            std::map<WorldId_t, RefWorld_t> RefWorld;
            struct RefPortal_t {
                DefPortalId Profile;
                RefAssets_t Assets;
            };
            // std::map<PortalId, RefPortal_t> RefPortal;
            struct RefEntity_t {
                DefEntityId Profile;
                RefAssets_t Assets;
            };
            std::map<ServerEntityId_t, RefEntity_t> RefEntity;
        } ResUses;

        // Смена идентификаторов сервера на клиентские
        SCSKeyRemapper<ServerEntityId_t, ClientEntityId_t> ReMapEntities;

        // Запрос информации об ассетах и профилях контента
        ResourceRequest NextRequest;
        // Запрошенные клиентом ресурсы
        /// TODO: здесь может быть засор
        std::vector<Hash_t> ClientRequested;

        void incrementAssets(const ResUses_t::RefAssets_t& bin);
        void decrementAssets(ResUses_t::RefAssets_t&& bin);

        Net::Packet NextPacket;
        std::vector<Net::Packet> SimplePackets;
        void checkPacketBorder(uint16_t size) {
            if(64000-NextPacket.size() < size || (NextPacket.size() != 0 && size == 0)) {
                SimplePackets.push_back(std::move(NextPacket));
            }
        }

        void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
            const std::vector<DefVoxelId>& uniq_sorted_defines);
        void prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
            const std::vector<DefNodeId>& uniq_sorted_defines);
        void prepareEntitiesRemove(const std::vector<ServerEntityId_t>& entityId);
        void prepareRegionsRemove(WorldId_t worldId, std::vector<Pos::GlobalRegion> regionPoses);
        void prepareWorldRemove(WorldId_t worldId);
        void prepareEntitiesUpdate(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities);
        void prepareEntitiesUpdate_Dynamic(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities);
        void prepareEntitySwap(ServerEntityId_t prevEntityId, ServerEntityId_t nextEntityId);
        void prepareWorldUpdate(WorldId_t worldId, World* world);
        void informateDefVoxel(const std::vector<std::pair<DefVoxelId, DefVoxel*>>& voxels);
        void informateDefNode(const std::vector<std::pair<DefNodeId, DefNode*>>& nodes);
        void informateDefWorld(const std::vector<std::pair<DefWorldId, DefWorld*>>& worlds);
        void informateDefPortal(const std::vector<std::pair<DefPortalId, DefPortal*>>& portals);
        void informateDefEntity(const std::vector<std::pair<DefEntityId, DefEntity*>>& entityes);
        void informateDefItem(const std::vector<std::pair<DefItemId, DefItem*>>& items);
    };

    struct {
        /*
            К концу такта собираются необходимые идентификаторы ресурсов
            В конце такта сервер забирает запросы и возвращает информацию
            о ресурсах. Отправляем связку Идентификатор + домен:ключ 
            + хеш. Если у клиента не окажется этого ресурса, он может его запросить
        */

        // Ресурсы, отправленные на клиент в этой сессии
        std::vector<Hash_t> OnClient;
        // Отправляемые на клиент ресурсы
        // Тип, домен, ключ, идентификатор, ресурс, количество отправленных байт
        std::vector<std::tuple<EnumAssets, std::string, std::string, ResourceId, Resource, size_t>> ToSend;
        // Пакет с ресурсами
        Net::Packet AssetsPacket;
    } AssetsInWork;

    TOS::SpinlockObject<NetworkAndResource_t> NetworkAndResource;

public:
    const std::string Username;
    Pos::Object CameraPos = {0, 0, 0};
    Pos::Object LastPos = CameraPos;
    ToServer::PacketQuat CameraQuat = {0};
    TOS::SpinlockObject<std::queue<uint8_t>> Actions;
    ResourceId RecievedAssets[(int) EnumAssets::MAX_ENUM] = {0};

    // Регионы, наблюдаемые клиентом
    ContentViewInfo ContentViewState;
    // Если игрок пересекал границы региона (для перерасчёта ContentViewState)
    bool CrossedRegion = true;
    std::queue<Pos::GlobalNode> Build, Break;

public:
    RemoteClient(asio::io_context &ioc, tcp::socket socket, const std::string username, std::vector<ResourceFile::Hash_t> &&client_cache)
        : LOG("RemoteClient " + username), Socket(ioc, std::move(socket)), Username(username)
    {}

    ~RemoteClient();

    coro<> run();
    void shutdown(EnumDisconnect type, const std::string reason);
    bool isConnected() { return IsConnected; }

    void pushPackets(std::vector<Net::Packet> *simplePackets, std::vector<Net::SmartPacket> *smartPackets = nullptr) {
        if(IsGoingShutdown)
            return;

        Socket.pushPackets(simplePackets, smartPackets);
    }

    // Возвращает список точек наблюдений клиентом с радиусом в регионах
    std::vector<std::tuple<WorldId_t, Pos::Object, uint8_t>> getViewPoints();

    /*
        Сервер собирает изменения миров, сжимает их и раздаёт на отправку игрокам
    */

    // Все функции prepare потокобезопасные
    // maybe используются в BackingChunkPressure_t в GameServer в пуле потоков.
    // если возвращает false, то блокировка сейчас находится у другого потока
    // и запрос не был обработан.

    // В зоне видимости добавился чанк или изменились его воксели
    bool maybe_prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
        const std::vector<DefVoxelId>& uniq_sorted_defines)
    {
        auto lock = NetworkAndResource.tryLock();
        if(!lock)
            return false;

        lock->prepareChunkUpdate_Voxels(worldId, chunkPos, compressed_voxels, uniq_sorted_defines);
        return true;
    }

    // В зоне видимости добавился чанк или изменились его ноды
    bool maybe_prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
        const std::vector<DefNodeId>& uniq_sorted_defines)
    {
        auto lock = NetworkAndResource.tryLock();
        if(!lock)
            return false;

        lock->prepareChunkUpdate_Nodes(worldId, chunkPos, compressed_nodes, uniq_sorted_defines);
        return true;
    }
    // void prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, const LightPrism *lights);
    
    // Клиент перестал наблюдать за сущностью
    void prepareEntitiesRemove(const std::vector<ServerEntityId_t>& entityId) { NetworkAndResource.lock()->prepareEntitiesRemove(entityId); }
    // Регион удалён из зоны видимости
    void prepareRegionsRemove(WorldId_t worldId, std::vector<Pos::GlobalRegion> regionPoses)  { NetworkAndResource.lock()->prepareRegionsRemove(worldId, regionPoses); }
    // Мир удалён из зоны видимости
    void prepareWorldRemove(WorldId_t worldId)  { NetworkAndResource.lock()->prepareWorldRemove(worldId); }

    // В зоне видимости добавилась новая сущность или она изменилась
    void prepareEntitiesUpdate(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities)  { NetworkAndResource.lock()->prepareEntitiesUpdate(entities); }
    void prepareEntitiesUpdate_Dynamic(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities)  { NetworkAndResource.lock()->prepareEntitiesUpdate_Dynamic(entities); }
    // Наблюдаемая сущность пересекла границы региона, у неё изменился серверный идентификатор
    void prepareEntitySwap(ServerEntityId_t prevEntityId, ServerEntityId_t nextEntityId)  { NetworkAndResource.lock()->prepareEntitySwap(prevEntityId, nextEntityId); }
    // Мир появился в зоне видимости или изменился
    void prepareWorldUpdate(WorldId_t worldId, World* world)  { NetworkAndResource.lock()->prepareWorldUpdate(worldId, world); }

    // В зоне видимости добавился порта или он изменился
    // void preparePortalUpdate(PortalId_t portalId, void* portal);
    // Клиент перестал наблюдать за порталом
    // void preparePortalRemove(PortalId_t portalId);

    // Прочие моменты
    void prepareCameraSetEntity(ServerEntityId_t entityId);

    // Отправка подготовленных пакетов
    ResourceRequest pushPreparedPackets();

    // Сообщить о ресурсах
    // Сюда приходят все обновления ресурсов движка
    // Глобально их можно запросить в выдаче pushPreparedPackets()

    // Оповещение о запрошенных (и не только) ассетах
    void informateAssets(const std::vector<std::tuple<EnumAssets, ResourceId, const std::string, const std::string, Resource>>& resources);

    // Игровые определения
    void informateDefVoxel(const std::vector<std::pair<DefVoxelId, DefVoxel*>>& voxels)         { NetworkAndResource.lock()->informateDefVoxel(voxels); }
    void informateDefNode(const std::vector<std::pair<DefNodeId, DefNode*>>& nodes)             { NetworkAndResource.lock()->informateDefNode(nodes); }
    void informateDefWorld(const std::vector<std::pair<DefWorldId, DefWorld*>>& worlds)         { NetworkAndResource.lock()->informateDefWorld(worlds); }
    void informateDefPortal(const std::vector<std::pair<DefPortalId, DefPortal*>>& portals)     { NetworkAndResource.lock()->informateDefPortal(portals); }
    void informateDefEntity(const std::vector<std::pair<DefEntityId, DefEntity*>>& entityes)    { NetworkAndResource.lock()->informateDefEntity(entityes); }
    void informateDefItem(const std::vector<std::pair<DefItemId, DefItem*>>& items)             { NetworkAndResource.lock()->informateDefItem(items); }

    void onUpdate();

private:
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_System(Net::AsyncSocket &sock);

    // void incrementProfile(const std::vector<TextureId_t> &textures, const std::vector<ModelId_t> &model,
    //     const std::vector<SoundId_t> &sounds, const std::vector<FontId_t> &font
    // );
    // void decrementProfile(std::vector<TextureId_t> &&textures, std::vector<ModelId_t> &&model,
    //     std::vector<SoundId_t> &&sounds, std::vector<FontId_t> &&font
    // );
};


}