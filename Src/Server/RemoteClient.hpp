#pragma once

#include <TOSLib.hpp>
#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include <Common/Abstract.hpp>
#include <bitset>
#include <initializer_list>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace LV::Server {


template<typename ServerKey, typename ClientKey, std::enable_if_t<std::is_integral_v<ServerKey> && std::is_integral_v<ClientKey> && sizeof(ServerKey) >= sizeof(ClientKey), int> = 0>
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

        std::bitset<64> &bits = std::get<0>(iChunk.second);
        std::array<ServerKey, 64> &keys = std::get<1>(iChunk.second);

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

template<typename ServerKey, typename ClientKey, std::enable_if_t<std::is_integral_v<ServerKey> && std::is_integral_v<ClientKey> && sizeof(ServerKey) >= sizeof(ClientKey), int> = 0>
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
        if(skey == ServerKey(0))
            return ClientKey(0);

        auto iter = Map.find(skey);
        if(iter == Map.end()) {
            // Идентификатор отсутствует, нужно его занять
            // Ищет позицию ближайшего бита 1
            size_t pos = FreeClientKeys._Find_first();
            if(pos == FreeClientKeys.size())
                return ClientKey(0); // Свободные идентификаторы отсутствуют

            ClientKey ckey = ClientKey(pos+1);
            Map[skey] = ckey;
            CSmapper.link(ckey, ckey);
            return ClientKey(pos);
        }

        return iter.second;
    }

    // Соотнести идентификатор на стороне клиента с идентификатором на стороне сервера
    ServerKey toServer(ClientKey ckey) {
        return CSmapper.toServer(ckey);
    } 

    // Удаляет серверный идентификатор, освобождая идентификатор клиента  
    ClientKey erase(ServerKey skey) {
        auto iter = Map.find(skey);

        assert(iter != Map.end() && "Идентификатор не существует");

        ClientKey ckey = iter.second;
        CSmapper.erase(ckey);
        Map.erase(iter);

        return ckey;
    }
};

/* 
    Шаблоны игрового контента, которые необходимо поддерживать в актуальном
    состоянии для клиента и шаблоны, которые клиенту уже не нужны.
    Соответствующие менеджеры ресурсов будут следить за изменениями
    этих ресурсов и переотправлять их клиенту
*/
struct ResourceRequest {
    std::vector<DefWorldId_t> NewWorlds;
    std::vector<DefVoxelId_t> NewVoxels;
    std::vector<DefNodeId_t> NewNodes;
    std::vector<DefEntityId_t> NewEntityes;

    std::vector<TextureId_t> NewTextures;
    std::vector<ModelId_t> NewModels;
    std::vector<SoundId_t> NewSounds;

    void insert(const ResourceRequest &obj) {
        NewWorlds.insert(NewWorlds.end(), obj.NewWorlds.begin(), obj.NewWorlds.end());
        NewVoxels.insert(NewVoxels.end(), obj.NewVoxels.begin(), obj.NewVoxels.end());
        NewNodes.insert(NewNodes.end(), obj.NewNodes.begin(), obj.NewNodes.end());
        NewEntityes.insert(NewEntityes.end(), obj.NewEntityes.begin(), obj.NewEntityes.end());
        NewTextures.insert(NewTextures.end(), obj.NewTextures.begin(), obj.NewTextures.end());
        NewModels.insert(NewModels.end(), obj.NewModels.begin(), obj.NewModels.end());
        NewSounds.insert(NewSounds.end(), obj.NewSounds.begin(), obj.NewSounds.end());
    }

    void uniq() {
        for(std::vector<ResourceId_t> *vec : {&NewWorlds, &NewVoxels, &NewNodes, &NewEntityes, &NewTextures, &NewModels, &NewSounds}) {
            std::sort(vec->begin(), vec->end());
            auto last = std::unique(vec->begin(), vec->end());
            vec->erase(last, vec->end());
        }
    }
};

using EntityKey = std::tuple<WorldId_c, Pos::GlobalRegion>;

/*
    Обработчик сокета клиента.
    Подписывает клиента на отслеживание необходимых ресурсов
    на основе передаваемых клиенту данных
*/
class RemoteClient {
    DestroyLock UseLock;
    Net::AsyncSocket Socket;
    bool IsConnected = true, IsGoingShutdown = false;

    struct {
        // Счётчики использования базовых ресурсов высшими объектами
        std::map<TextureId_t, uint32_t> TextureUses;
        std::map<SoundId_t, uint32_t> SoundUses;

        // Может использовать текстуры
        std::map<ModelId_t, uint32_t> ModelUses;

        // Будут использовать в своих определениях текстуры, звуки, модели
        std::map<DefVoxelId_t, uint32_t> VoxelDefUses;
        std::map<DefNodeId_t, uint32_t> NodeDefUses;
        std::map<DefEntityId_t, uint32_t> EntityDefUses;
        std::map<DefWorldId_t, uint32_t> WorldDefUses;

        // Чанки используют воксели, ноды, миры
        // Сущности используют текстуры, модели, звуки, миры
    
    } Remap;

    struct {
    } BinaryResourceUsedIds;

    struct {

    } WorldUsedIds;

    ResourceRequest NextRequest;
    std::vector<Net::Packet> SimplePackets;

public:
    const std::string Username;

public:
    RemoteClient(asio::io_context &ioc, tcp::socket socket, const std::string username)
        : Socket(ioc, std::move(socket)), Username(username)
    {
    }

    ~RemoteClient();

    coro<> run();
    void shutdown(const std::string reason);
    bool isConnected() { return IsConnected; }

    void pushPackets(std::vector<Net::Packet> *simplePackets, std::vector<Net::SmartPacket> *smartPackets = nullptr) {
        if(IsGoingShutdown)
            return;

        Socket.pushPackets(simplePackets, smartPackets);
    }

    // Функции подготавливают пакеты к отправке

    // Необходимые определения шаблонов игрового контента
    void prepareDefWorld(WorldId_t worldId, void* world);
    void prepareDefVoxel(VoxelId_t voxelId, void* voxel);
    void prepareDefNode(NodeId_t worldId, void* node);
    void prepareDefMediaStream(MediaStreamId_t modelId, void* mediaStream);

    // Отслеживаемое игроком использование контента
    //  Maybe?
    //  Текущий список вокселей, определения нод, которые больше не используются в чанке, и определения нод, которые теперь используются
    //void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::vector<VoxelCube> &voxels, const std::vector<DefVoxelId_t> &noLongerInUseDefs, const std::vector<DefVoxelId_t> &nowUsed);
    
    void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::vector<VoxelCube> &voxels);
    void prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::unordered_map<Pos::Local16_u, Node> &nodes);
    void prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, const LightPrism *lights);
    void prepareChunkRemove(WorldId_t worldId, Pos::GlobalChunk chunkPos);

    void prepareWorldRemove(WorldId_t worldId);

    void prepareEntitySwap(WorldId_t prevWorldId, Pos::GlobalRegion prevRegionPos, EntityId_t prevEntityId,
        WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, EntityId_t newEntityId);
    void prepareEntityUpdate(WorldId_t worldId, Pos::GlobalRegion regionPos, EntityId_t entityId, const Entity *entity);
    void prepareEntityRemove(WorldId_t worldId, Pos::GlobalRegion regionPos, EntityId_t entityId);

    void prepareWorldNew(DefWorldId_t Id, void* world);
    void prepareWorldUpdate(DefWorldId_t defWorldId, void* world);
    void prepareWorldRemove(DefWorldId_t defWorldId);

    void preparePortalNew(PortalId_t portalId, void* portal);
    void preparePortalUpdate(PortalId_t portalId, void* portal);
    void preparePortalRemove(PortalId_t portalId);

    // Прочие моменты
    void prepareCameraSetEntity(WorldId_t worldId, Pos::GlobalChunk chunkPos, EntityId_t entityId);

    // Отправка подготовленных пакетов
    ResourceRequest pushPreparedPackets();

    // Сообщить о ресурсах
    // Сюда приходят все обновления ресурсов движка
    // Глобально их можно запросить в выдаче pushPreparedPackets()
    void informateDefTexture(const std::unordered_map<TextureId_t, std::shared_ptr<ResourceFile>> &textures);
    void informateDefModel(const std::unordered_map<ModelId_t, std::shared_ptr<ResourceFile>> &models);
    void informateDefSound(const std::unordered_map<SoundId_t, std::shared_ptr<ResourceFile>> &sounds);

private:
    WorldId_c rentWorldRemapId(WorldId_t wId);
};


}