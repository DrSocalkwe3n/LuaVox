#pragma once

#include <TOSLib.hpp>
#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include "Common/Packets.hpp"
#include "Server/ContentManager.hpp"
#include <Common/Abstract.hpp>
#include <bitset>
#include <initializer_list>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace LV::Server {

class World;
class GameServer;

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
    std::vector<Hash_t> Hashes;

    void merge(const ResourceRequest &obj) {
        Hashes.insert(Hashes.end(), obj.Hashes.begin(), obj.Hashes.end());
    }

    void uniq() {
        std::sort(Hashes.begin(), Hashes.end());
        auto last = std::unique(Hashes.begin(), Hashes.end());
        Hashes.erase(last, Hashes.end());
    }
};

struct AssetBinaryInfo {
    Resource Data;
    Hash_t Hash;
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
        // Смена идентификаторов сервера на клиентские
        SCSKeyRemapper<ServerEntityId_t, ClientEntityId_t> ReMapEntities;
        // Накопленные чанки для отправки
        std::unordered_map<
            WorldId_t,                  // Миры
            std::unordered_map<
                Pos::GlobalRegion,      // Регионы
                std::pair<
                    std::unordered_map< // Воксели
                        Pos::bvec4u,    // Чанки
                        std::u8string
                    >,
                    std::unordered_map< // Ноды
                        Pos::bvec4u,    // Чанки
                        std::u8string
                    >
                >
            >
        > ChunksToSend;

        // Запрос информации об ассетах и профилях контента
        ResourceRequest NextRequest;
        // Запрошенные клиентом ресурсы
        /// TODO: здесь может быть засор
        std::vector<Hash_t> ClientRequested;

        Net::Packet NextPacket;
        std::vector<Net::Packet> SimplePackets;
        void checkPacketBorder(uint16_t size) {
            if(64000-NextPacket.size() < size || (NextPacket.size() != 0 && size == 0)) {
                SimplePackets.push_back(std::move(NextPacket));
            }
        }

        void prepareChunkUpdate_Voxels(
            WorldId_t worldId,
            Pos::GlobalRegion regionPos,
            Pos::bvec4u chunkPos,
            const std::u8string& compressed_voxels
        ) {
            ChunksToSend[worldId][regionPos].first[chunkPos] = compressed_voxels;
        }

        void prepareChunkUpdate_Nodes(
            WorldId_t worldId,
            Pos::GlobalRegion regionPos,
            Pos::bvec4u chunkPos,
            const std::u8string& compressed_nodes
        ) {
            ChunksToSend[worldId][regionPos].second[chunkPos] = compressed_nodes;
        }

        void flushChunksToPackets();

        void prepareEntitiesRemove(const std::vector<ServerEntityId_t>& entityId);
        void prepareRegionsRemove(WorldId_t worldId, std::vector<Pos::GlobalRegion> regionPoses);
        void prepareWorldRemove(WorldId_t worldId);
        void prepareEntitiesUpdate(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities);
        void prepareEntitiesUpdate_Dynamic(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities);
        void prepareEntitySwap(ServerEntityId_t prevEntityId, ServerEntityId_t nextEntityId);
        void prepareWorldUpdate(WorldId_t worldId, World* world);
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
        // Ресурс, количество отправленных байт
        std::vector<std::tuple<Resource, size_t>> ToSend;
        // Пакет с ресурсами
        std::vector<Net::Packet> AssetsPackets;
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
    std::optional<ServerEntityId_t> PlayerEntity;

public:
    RemoteClient(asio::io_context &ioc, tcp::socket socket, const std::string username, GameServer* server)
        : LOG("RemoteClient " + username), Socket(ioc, std::move(socket)), Username(username), Server(server)
    {}

    ~RemoteClient();

    coro<> run();
    void shutdown(EnumDisconnect type, const std::string reason);
    bool isConnected() { return IsConnected; }
    void setPlayerEntity(ServerEntityId_t id) { PlayerEntity = id; }
    std::optional<ServerEntityId_t> getPlayerEntity() const { return PlayerEntity; }
    void clearPlayerEntity() { PlayerEntity.reset(); }

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

    // Создаёт пакет отправки вокселей чанка
    void prepareChunkUpdate_Voxels(
        WorldId_t worldId,
        Pos::GlobalRegion regionPos,
        Pos::bvec4u chunkPos,
        const std::u8string& compressed_voxels
    ) {
        NetworkAndResource.lock()->prepareChunkUpdate_Voxels(worldId, regionPos, chunkPos, compressed_voxels);
    }

    // Создаёт пакет отправки нод чанка
    void prepareChunkUpdate_Nodes(
        WorldId_t worldId,
        Pos::GlobalRegion regionPos,
        Pos::bvec4u chunkPos,
        const std::u8string& compressed_nodes
    ) {
        NetworkAndResource.lock()->prepareChunkUpdate_Nodes(worldId, regionPos, chunkPos, compressed_nodes);
    }

    // Клиент перестал наблюдать за сущностями
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

    // Создаёт пакет для всех игроков с оповещением о новых идентификаторах (id -> domain+key)
    static Net::Packet makePacket_informateAssets_DK(
        const std::array<
            std::vector<AssetsPreloader::BindDomainKeyInfo>, 
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        >& dkVector
    );

    // Создаёт пакет для всех игроков с оповещением об изменении файлов ресурсов (id -> hash+header)
    static Net::Packet makePacket_informateAssets_HH(
        const std::array< 
            std::vector<AssetsPreloader::BindHashHeaderInfo>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        >& hhVector,
        const std::array<
            std::vector<ResourceId>,
            static_cast<size_t>(EnumAssets::MAX_ENUM)
        >& lost
    );

    // Оповещение о двоичных ресурсах (стриминг по запросу)
    void informateBinaryAssets(
        const std::vector<AssetBinaryInfo>& resources
    );

    // Создаёт пакет об обновлении игровых профилей
    static std::vector<Net::Packet> makePackets_sendDefContentUpdate(
        std::array<
            std::vector<
                std::pair<
                    ResourceId,     // Идентификатор профиля
                    std::u8string   // Двоичный формат профиля
                >
            >,
            static_cast<size_t>(EnumDefContent::MAX_ENUM)
        > newOrUpdate,  // Новые или изменённые
        std::array<
            std::vector<ResourceId>,
            static_cast<size_t>(EnumDefContent::MAX_ENUM)
        > lost,         // Потерянные профили
        std::array<
            std::vector<std::pair<std::string, std::string>>,
            static_cast<size_t>(EnumDefContent::MAX_ENUM)
        > idToDK        // Новые привязки
    );
    
    void onUpdate();

private:
    GameServer* Server = nullptr;
    void protocolError();
    coro<> readPacket(Net::AsyncSocket &sock);
    coro<> rP_System(Net::AsyncSocket &sock);
};


}
