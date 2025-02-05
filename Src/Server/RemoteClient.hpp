#pragma once

#include <Common/Lockable.hpp>
#include <Common/Net.hpp>
#include "Abstract.hpp"
#include <Common/Abstract.hpp>
#include <bitset>
#include <initializer_list>
#include <unordered_map>
#include <unordered_set>

namespace LV::Server {

/* 
    Шаблоны игрового контента, которые необходимо поддерживать в актуальном
    состоянии для клиента и шаблоны, которые клиенту уже не нужны.
    Соответствующие менеджеры ресурсов будут следить за изменениями
    этих ресурсов и переотправлять их клиенту
*/
struct ResourceRequest {
    std::vector<WorldId_t> NewWorlds;
    std::vector<VoxelId_t> NewVoxels;
    std::vector<NodeId_t> NewNodes;
    std::vector<TextureId_t> NewTextures;
    std::vector<ModelId_t> NewModels;
    std::vector<SoundId_t> NewSounds;

    void insert(const ResourceRequest &obj) {
        NewWorlds.insert(NewWorlds.end(), obj.NewWorlds.begin(), obj.NewWorlds.end());
        NewVoxels.insert(NewVoxels.end(), obj.NewVoxels.begin(), obj.NewVoxels.end());
        NewNodes.insert(NewNodes.end(), obj.NewNodes.begin(), obj.NewNodes.end());
        NewTextures.insert(NewTextures.end(), obj.NewTextures.begin(), obj.NewTextures.end());
        NewModels.insert(NewModels.end(), obj.NewModels.begin(), obj.NewModels.end());
        NewSounds.insert(NewSounds.end(), obj.NewSounds.begin(), obj.NewSounds.end());
    }

    void uniq() {
        for(std::vector<ResourceId_t> *vec : {&NewWorlds, &NewVoxels, &NewNodes, &NewTextures, &NewModels, &NewSounds}) {
            std::sort(vec->begin(), vec->end());
            auto last = std::unique(vec->begin(), vec->end());
            vec->erase(last, vec->end());
        }
    }
};

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
        std::bitset<(1 << sizeof(EntityId_c)*8) - 1> UsedEntityIdC; // 1 - идентификатор свободен, 0 - занят
        std::unordered_map<EntityId_c, std::tuple<WorldId_t, Pos::GlobalRegion, EntityId_t>> CTS_Entityes;
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalRegion, std::unordered_map<EntityId_t, EntityId_c>>> STC_Entityes;
    
        std::unordered_map<TextureId_t, TextureId_c> STC_Textures;
        std::unordered_map<ModelId_t, ModelId_c> STC_Models;
        //std::unordered_map<SoundId_t, SoundId_c> STC_Sounds;
        std::bitset<(1 << sizeof(VoxelId_c)*8) - 1> UsedVoxelIdC; // 1 - идентификатор свободен, 0 - занят
        std::unordered_map<VoxelId_t, VoxelId_c> STC_Voxels;
        std::bitset<(1 << sizeof(NodeId_c)*8) - 1> UsedNodeIdC; // 1 - идентификатор свободен, 0 - занят
        std::unordered_map<NodeId_t, NodeId_c> STC_Nodes;
        std::bitset<(1 << sizeof(VoxelId_c)*8) - 1> UsedWorldIdC; // 1 - идентификатор свободен, 0 - занят
        std::unordered_map<WorldId_t, WorldId_c> STC_Worlds;
    } Remap;

    struct {
        //std::unordered_map<EntityId_c, > EntityTextures;
    } BinaryResourceUsedIds;

    // Вести учёт использования идентификаторов
    struct {
        // Использованные идентификаторы вокселей чанками
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::unordered_set<VoxelId_t>>> Voxels;
        // Количество зависимостей к ресурсу
        std::unordered_map<VoxelId_t, uint32_t> VoxelsUsedCount;
        // Использованные идентификаторы нод чанками
        std::unordered_map<WorldId_t, std::unordered_map<Pos::GlobalChunk, std::unordered_set<NodeId_c>>> Nodes;
        // Количество зависимостей к ресурсу
        std::unordered_map<NodeId_c, uint32_t> NodesUsedCount;
    } ChunkUsedIds;

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
    void prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::vector<VoxelCube> &voxels);
    void prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::unordered_map<Pos::Local16_u, Node> &nodes);
    void prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, const LightPrism *lights);
    void prepareChunkRemove(WorldId_t worldId, Pos::GlobalChunk chunkPos);

    void prepareWorldRemove(WorldId_t worldId);

    void prepareEntitySwap(WorldId_t prevWorldId, Pos::GlobalRegion prevRegionPos, EntityId_t prevEntityId,
        WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, EntityId_t newEntityId);
    void prepareEntityUpdate(WorldId_t worldId, Pos::GlobalRegion regionPos, EntityId_t entityId, const Entity *entity);
    void prepareEntityRemove(WorldId_t worldId, Pos::GlobalRegion regionPos, EntityId_t entityId);

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