#pragma once

#include <TOSLib.hpp>
#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include "Common/Packets.hpp"
#include "Server/ContentEventController.hpp"
#include <Common/Abstract.hpp>
#include <atomic>
#include <bitset>
#include <initializer_list>
#include <queue>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace LV::Server {

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
*/
struct ResourceRequest {
    std::vector<Hash_t>           Hashes;
    std::vector<ResourceId_t>     BinToHash[5 /*EnumBinResource*/];

    std::vector<DefVoxelId_t>       Voxel;
    std::vector<DefNodeId_t>        Node;
    std::vector<DefWorldId_t>       World;
    std::vector<DefPortalId_t>      Portal;
    std::vector<DefEntityId_t>      Entity;
    std::vector<DefItemId_t>        Item;

    void insert(const ResourceRequest &obj) {
        Hashes.insert(Hashes.end(), obj.Hashes.begin(), obj.Hashes.end());
        for(int iter = 0; iter < 5; iter++)
            BinToHash[iter].insert(BinToHash[iter].end(), obj.BinToHash[iter].begin(), obj.BinToHash[iter].end());

        Voxel.insert(Voxel.end(), obj.Voxel.begin(), obj.Voxel.end());
        Node.insert(Node.end(), obj.Node.begin(), obj.Node.end());
        World.insert(World.end(), obj.World.begin(), obj.World.end());
        Portal.insert(Portal.end(), obj.Portal.begin(), obj.Portal.end());
        Entity.insert(Entity.end(), obj.Entity.begin(), obj.Entity.end());
        Item.insert(Item.end(), obj.Item.begin(), obj.Item.end());
    }

    void uniq() {
        for(std::vector<ResourceId_t> *vec : {&Voxel, &Node, &World,
                &Portal, &Entity, &Item
            })
        {
            std::sort(vec->begin(), vec->end());
            auto last = std::unique(vec->begin(), vec->end());
            vec->erase(last, vec->end());
        }

        for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++)
        {
            std::sort(BinToHash[type].begin(), BinToHash[type].end());
            auto last = std::unique(BinToHash[type].begin(), BinToHash[type].end());
            BinToHash[type].erase(last, BinToHash[type].end());
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
    
    std::vector<Hash_t> ClientBinaryCache, // Хеши ресурсов которые есть у клиента
        NeedToSend; // Хеши которые нужно получить и отправить

    /*
        При обнаружении нового контента составляется запрос (ResourceRequest)
        на полное описание ресурса. Это описание отправляется клиенту и используется 
        чтобы выстроить зависимость какие базовые ресурсы использует контент.
        Если базовые ресурсы не известны, то они также запрашиваются.
    */

    struct ResUsesObj {
        // Счётчики использования двоичных кэшируемых ресурсов + хэш привязанный к идентификатору
        std::map<ResourceId_t,      std::tuple<uint32_t, Hash_t>>      BinUse[5];

        // Счётчики использование профилей контента
        std::map<DefVoxelId_t,      uint32_t>        DefVoxel; // Один чанк, одно использование
        std::map<DefNodeId_t,       uint32_t>         DefNode;
        std::map<DefWorldId_t,      uint32_t>        DefWorld;
        std::map<DefPortalId_t,     uint32_t>       DefPortal;
        std::map<DefEntityId_t,     uint32_t>       DefEntity;
        std::map<DefItemId_t,       uint32_t>         DefItem; // При передаче инвентарей?

        // Зависимость профилей контента от профилей ресурсов
        // Нужно чтобы пересчитать зависимости к профилям ресурсов
        struct RefDefBin_t {
            std::vector<ResourceId_t> Resources[5];
        };


        std::map<DefVoxelId_t, RefDefBin_t>           RefDefVoxel;
        std::map<DefNodeId_t, RefDefBin_t>            RefDefNode;
        std::map<WorldId_t, RefDefBin_t>              RefDefWorld;
        std::map<DefPortalId_t, RefDefBin_t>          RefDefPortal;
        std::map<DefEntityId_t, RefDefBin_t>          RefDefEntity;
        std::map<DefItemId_t, RefDefBin_t>            RefDefItem;

        // Модификационные зависимости экземпляров профилей контента
        struct ChunkRef {
            // Отсортированные списки уникальных вокселей
            std::vector<DefVoxelId_t> Voxel;
            std::vector<DefNodeId_t> Node;
        };
        std::atomic_bool RefChunkLock = 0;
        std::map<WorldId_t, std::map<Pos::GlobalRegion, std::array<ChunkRef, 4*4*4>>> RefChunk;
        struct RefWorld_t {
            DefWorldId_t Profile;
        };
        std::map<WorldId_t, RefWorld_t> RefWorld;
        struct RefPortal_t {
            DefPortalId_t Profile;
        };
        std::map<PortalId_t, RefPortal_t> RefPortal;
        struct RefEntity_t {
            DefEntityId_t Profile;
        };
        std::map<ServerEntityId_t, RefEntity_t> RefEntity;

    } ResUses;

    // Смена идентификаторов сервера на клиентские
    struct {
        SCSKeyRemapper<ServerEntityId_t, ClientEntityId_t> Entityes;
        SCSKeyRemapper<ServerFuncEntityId_t, ClientEntityId_t> FuncEntityes;
    } ResRemap;

    Net::Packet NextPacket;
    std::vector<Net::Packet> SimplePackets;
    ResourceRequest NextRequest;

public:
    const std::string Username;
    Pos::Object CameraPos = {0, 0, 0};
    ToServer::PacketQuat CameraQuat = {0};
    TOS::SpinlockObject<std::queue<uint8_t>> Actions;
    ResourceId_t RecievedAssets[(int) EnumAssets::MAX_ENUM] = {0};

public:
    RemoteClient(asio::io_context &ioc, tcp::socket socket, const std::string username, std::vector<ResourceFile::Hash_t> &&client_cache)
        : LOG("RemoteClient " + username), Socket(ioc, std::move(socket)), Username(username), ClientBinaryCache(std::move(client_cache))
    {
    }

    ~RemoteClient();

    coro<> run();
    void shutdown(EnumDisconnect type, const std::string reason);
    bool isConnected() { return IsConnected; }

    void pushPackets(std::vector<Net::Packet> *simplePackets, std::vector<Net::SmartPacket> *smartPackets = nullptr) {
        if(IsGoingShutdown)
            return;

        Socket.pushPackets(simplePackets, smartPackets);
    }

    /*
        Сервер собирает изменения миров, сжимает их и раздаёт на отправку игрокам
    
    */

    // Функции подготавливают пакеты к отправке
    // Отслеживаемое игроком использование контента

    // maybe созданны для использования в многопотоке, если ресурс сейчас занят вернёт false, потом нужно повторить запрос
    // В зоне видимости добавился чанк или изменились его воксели
    bool maybe_prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
        const std::vector<DefVoxelId_t>& uniq_sorted_defines);
    // В зоне видимости добавился чанк или изменились его ноды
    bool maybe_prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
        const std::vector<DefNodeId_t>& uniq_sorted_defines);
    // void prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, const LightPrism *lights);


    // Регион удалён из зоны видимости
    void prepareRegionRemove(WorldId_t worldId, Pos::GlobalRegion regionPos);

    // В зоне видимости добавилась новая сущность или она изменилась
    void prepareEntityUpdate(ServerEntityId_t entityId, const Entity *entity);
    // Наблюдаемая сущность пересекла границы региона, у неё изменился серверный идентификатор
    void prepareEntitySwap(ServerEntityId_t prevEntityId, ServerEntityId_t nextEntityId);
    // Клиент перестал наблюдать за сущностью
    void prepareEntityRemove(ServerEntityId_t entityId);

    // В зоне видимости добавился мир или он изменился
    void prepareWorldUpdate(WorldId_t worldId, World* world);
    // Клиент перестал наблюдать за миром
    void prepareWorldRemove(WorldId_t worldId);

    // В зоне видимости добавился порта или он изменился
    void preparePortalUpdate(PortalId_t portalId, void* portal);
    // Клиент перестал наблюдать за порталом
    void preparePortalRemove(PortalId_t portalId);

    // Прочие моменты
    void prepareCameraSetEntity(ServerEntityId_t entityId);

    // Отправка подготовленных пакетов
    ResourceRequest pushPreparedPackets();

    // Сообщить о ресурсах
    // Сюда приходят все обновления ресурсов движка
    // Глобально их можно запросить в выдаче pushPreparedPackets()

    // Оповещение о ресурсе для отправки клиентам
    void informateBinary(const std::vector<std::shared_ptr<ResourceFile>>& resources);

    // Привязывает локальный идентификатор с хешем. Если его нет у клиента, 
    // то делается запрос на получение ресурсы для последующей отправки клиенту
    void informateIdToHash(const std::unordered_map<ResourceId_t, ResourceFile::Hash_t>* resourcesLink);

    // Игровые определения
    void informateDefVoxel(const std::unordered_map<DefVoxelId_t, DefVoxel_t*> &voxels);
    void informateDefNode(const std::unordered_map<DefNodeId_t, DefNode_t*> &nodes);
    void informateDefWorld(const std::unordered_map<DefWorldId_t, DefWorld_t*> &worlds);
    void informateDefPortal(const std::unordered_map<DefPortalId_t, DefPortal_t*> &portals);
    void informateDefEntity(const std::unordered_map<DefEntityId_t, DefEntity_t*> &entityes);
    void informateDefItem(const std::unordered_map<DefItemId_t, DefItem_t*> &items);

private:
    void checkPacketBorder(uint16_t size);
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_System(Net::AsyncSocket &sock);

    void incrementBinary(const ResUsesObj::RefDefBin_t& bin);
    void decrementBinary(ResUsesObj::RefDefBin_t&& bin);

    // void incrementProfile(const std::vector<TextureId_t> &textures, const std::vector<ModelId_t> &model,
    //     const std::vector<SoundId_t> &sounds, const std::vector<FontId_t> &font
    // );
    // void decrementProfile(std::vector<TextureId_t> &&textures, std::vector<ModelId_t> &&model,
    //     std::vector<SoundId_t> &&sounds, std::vector<FontId_t> &&font
    // );

    
};


}