#pragma once

#include <TOSLib.hpp>
#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include "Server/ContentEventController.hpp"
#include <Common/Abstract.hpp>
#include <bitset>
#include <initializer_list>
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
    std::vector<BinTextureId_t> NewTextures;
    std::vector<BinModelId_t> NewModels;
    std::vector<BinSoundId_t> NewSounds;

    std::vector<DefWorldId_t> NewWorlds;
    std::vector<DefVoxelId_t> NewVoxels;
    std::vector<DefNodeId_t> NewNodes;
    std::vector<DefPortalId_t> NewPortals;
    std::vector<DefEntityId_t> NewEntityes;

    void insert(const ResourceRequest &obj) {
        NewTextures.insert(NewTextures.end(), obj.NewTextures.begin(), obj.NewTextures.end());
        NewModels.insert(NewModels.end(), obj.NewModels.begin(), obj.NewModels.end());
        NewSounds.insert(NewSounds.end(), obj.NewSounds.begin(), obj.NewSounds.end());

        NewWorlds.insert(NewWorlds.end(), obj.NewWorlds.begin(), obj.NewWorlds.end());
        NewVoxels.insert(NewVoxels.end(), obj.NewVoxels.begin(), obj.NewVoxels.end());
        NewNodes.insert(NewNodes.end(), obj.NewNodes.begin(), obj.NewNodes.end());
        NewPortals.insert(NewPortals.end(), obj.NewPortals.begin(), obj.NewPortals.end());
        NewEntityes.insert(NewEntityes.end(), obj.NewEntityes.begin(), obj.NewEntityes.end());
    }

    void uniq() {
        for(std::vector<ResourceId_t> *vec : {&NewTextures, &NewModels, &NewSounds, &NewWorlds, &NewVoxels, &NewNodes, &NewPortals, &NewEntityes}) {
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
    TOS::Logger LOG;
    DestroyLock UseLock;
    Net::AsyncSocket Socket;
    bool IsConnected = true, IsGoingShutdown = false;

    struct ResUsesObj {
        // Счётчики использования базовых ресурсов высшими объектами
        std::map<BinTextureId_t, uint32_t> BinTexture;
        std::map<BinSoundId_t, uint32_t> BinSound;

        // Может использовать текстуры
        std::map<BinModelId_t, uint32_t> BinModel;

        // Будут использовать в своих определениях текстуры, звуки, модели
        std::map<DefWorldId_t, uint32_t> DefWorld;
        std::map<DefVoxelId_t, uint32_t> DefVoxel;
        std::map<DefNodeId_t, uint32_t> DefNode;
        std::map<DefPortalId_t, uint32_t> DefPortal;
        std::map<DefEntityId_t, uint32_t> DefEntity;


        // Переписываемый контент

        // Сущности используют текстуры, звуки, модели
        struct EntityResourceUse {
            DefEntityId_t DefId;

            std::unordered_set<BinTextureId_t> Textures;
            std::unordered_set<BinSoundId_t> Sounds;
            std::unordered_set<BinModelId_t> Models;
        };

        std::map<GlobalEntityId_t, EntityResourceUse> Entity;

        // Чанки используют воксели, ноды
        std::map<std::tuple<WorldId_t, Pos::GlobalChunk>, std::unordered_set<DefVoxelId_t>> ChunkVoxels;
        std::map<std::tuple<WorldId_t, Pos::GlobalChunk>, std::unordered_set<DefNodeId_t>> ChunkNodes;

        // Миры
        struct WorldResourceUse {
            DefWorldId_t DefId;

            std::unordered_set<BinTextureId_t> Textures;
            std::unordered_set<BinSoundId_t> Sounds;
            std::unordered_set<BinModelId_t> Models;
        };

        std::map<WorldId_t, WorldResourceUse> Worlds;


        // Порталы
        struct PortalResourceUse {
            DefPortalId_t DefId;

            std::unordered_set<BinTextureId_t> Textures;
            std::unordered_set<BinSoundId_t> Sounds;
            std::unordered_set<BinModelId_t> Models;
        };

        std::map<PortalId_t, PortalResourceUse> Portals;
    
    } ResUses;

    struct {
        SCSKeyRemapper<BinTextureId_t, TextureId_c> BinTextures;
        SCSKeyRemapper<BinSoundId_t, SoundId_c> BinSounds;
        SCSKeyRemapper<BinModelId_t, ModelId_c> BinModels;

        SCSKeyRemapper<DefWorldId_t, DefWorldId_c> DefWorlds;
        SCSKeyRemapper<DefVoxelId_t, VoxelId_c> DefVoxels;
        SCSKeyRemapper<DefNodeId_t, NodeId_c> DefNodes;
        SCSKeyRemapper<DefPortalId_t, DefPortalId_c> DefPortals;
        SCSKeyRemapper<DefEntityId_t, DefEntityId_c> DefEntityes;

        SCSKeyRemapper<WorldId_t, WorldId_c> Worlds;
        SCSKeyRemapper<PortalId_t, PortalId_c> Portals;
        SCSKeyRemapper<GlobalEntityId_t, EntityId_c> Entityes;
    } ResRemap;

    Net::Packet NextPacket;
    ResourceRequest NextRequest;
    std::vector<Net::Packet> SimplePackets;

public:
    const std::string Username;

public:
    RemoteClient(asio::io_context &ioc, tcp::socket socket, const std::string username)
        : LOG("RemoteClient " + username), Socket(ioc, std::move(socket)), Username(username)
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
    // Отслеживаемое игроком использование контента
    //  Maybe?
    //  Текущий список вокселей, определения нод, которые больше не используются в чанке, и определения нод, которые теперь используются
    //void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::vector<VoxelCube> &voxels, const std::vector<DefVoxelId_t> &noLongerInUseDefs, const std::vector<DefVoxelId_t> &nowUsed);
    void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::vector<VoxelCube> &voxels);
    void prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::unordered_map<Pos::Local16_u, Node> &nodes);
    void prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, const LightPrism *lights);
    void prepareChunkRemove(WorldId_t worldId, Pos::GlobalChunk chunkPos);

    void prepareEntitySwap(GlobalEntityId_t prevEntityId, GlobalEntityId_t nextEntityId);
    void prepareEntityUpdate(GlobalEntityId_t entityId, const Entity *entity);
    void prepareEntityRemove(GlobalEntityId_t entityId);

    void prepareWorldNew(WorldId_t worldId, World* world);
    void prepareWorldUpdate(WorldId_t worldId, World* world);
    void prepareWorldRemove(WorldId_t worldId);

    void preparePortalNew(PortalId_t portalId, void* portal);
    void preparePortalUpdate(PortalId_t portalId, void* portal);
    void preparePortalRemove(PortalId_t portalId);

    // Прочие моменты
    void prepareCameraSetEntity(GlobalEntityId_t entityId);

    // Отправка подготовленных пакетов
    ResourceRequest pushPreparedPackets();

    // Сообщить о ресурсах
    // Сюда приходят все обновления ресурсов движка
    // Глобально их можно запросить в выдаче pushPreparedPackets()

    // Двоичные файлы
    void informateDefTexture(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures);
    void informateDefSound(const std::unordered_map<BinSoundId_t, std::shared_ptr<ResourceFile>> &sounds);
    void informateDefModel(const std::unordered_map<BinModelId_t, std::shared_ptr<ResourceFile>> &models);

    // Игровые определения
    void informateDefWorld(const std::unordered_map<DefWorldId_t, World*> &worlds);
    void informateDefVoxel(const std::unordered_map<DefVoxelId_t, void*> &voxels);
    void informateDefNode(const std::unordered_map<DefNodeId_t, void*> &nodes);
    void informateDefEntityes(const std::unordered_map<DefEntityId_t, void*> &entityes);
    void informateDefPortals(const std::unordered_map<DefPortalId_t, void*> &portals);

private:
    void incrementBinary(std::unordered_set<BinTextureId_t> &textures, std::unordered_set<BinSoundId_t> &sounds,
        std::unordered_set<BinModelId_t> &models);
    void decrementBinary(std::unordered_set<BinTextureId_t> &textures, std::unordered_set<BinSoundId_t> &sounds,
        std::unordered_set<BinModelId_t> &models);
};


}