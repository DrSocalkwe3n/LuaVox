#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
#include <unordered_map>
#include <unordered_set>


namespace LV::Server {

RemoteClient::~RemoteClient() {
    shutdown("~RemoteClient()");
    UseLock.wait_no_use();
}

coro<> RemoteClient::run() {
    auto useLock = UseLock.lock();

    try {
        while(!IsGoingShutdown && IsConnected) {
            co_await Socket.read<uint8_t>();
        }
    } catch(const std::exception &exc) {
        if(const auto *errc = dynamic_cast<const boost::system::system_error*>(&exc); 
            errc && errc->code() == boost::asio::error::operation_aborted)
        {
            co_return;
        }

        TOS::Logger("PlayerSocket").warn() << Username << ": " << exc.what();
    }

    IsConnected = false;

    co_return;
}

void RemoteClient::shutdown(const std::string reason) {
    if(IsGoingShutdown)
        return;

    IsGoingShutdown = true;
    // Отправить пакет о завершении работы
}

void RemoteClient::prepareDefWorld(WorldId_t worldId, void* world) {

}

void RemoteClient::prepareDefVoxel(VoxelId_t voxelId, void* voxel) {

}

void RemoteClient::prepareDefNode(NodeId_t worldId, void* node) {

}

void RemoteClient::prepareDefMediaStream(MediaStreamId_t modelId, void* mediaStream) {

}

// Может прийти событие на чанк, про который ещё ничего не знаем
void RemoteClient::prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const std::vector<VoxelCube> &voxels)
{
    WorldId_c wcId = rentWorldRemapId(worldId);
    if(wcId == WorldId_c(-1))
        return;

    // Перебиндить идентификаторы вокселей
    std::vector<VoxelId_t> NeedVoxels;
    NeedVoxels.reserve(voxels.size());

    for(const VoxelCube &cube : voxels) {
        NeedVoxels.push_back(cube.Material);
    }

    std::unordered_set<VoxelId_t> NeedVoxelsSet(NeedVoxels.begin(), NeedVoxels.end());

    // Собираем информацию о конвертации идентификаторов
    std::unordered_map<VoxelId_t, VoxelId_c> LocalRemapper;
    for(VoxelId_t vId : NeedVoxelsSet) {
        auto cvId = Remap.STC_Voxels.find(vId);
        if(cvId == Remap.STC_Voxels.end()) {
            // Нужно забронировать идентификатор
            VoxelId_c cvnId = Remap.UsedVoxelIdC._Find_first();
            if(cvnId == VoxelId_c(-1))
                // Нет свободных идентификаторов
                LocalRemapper[vId] = 0;
            else {
                NextRequest.NewVoxels.push_back(vId);
                Remap.UsedVoxelIdC.reset(cvnId);
                Remap.STC_Voxels[vId] = cvnId;
                LocalRemapper[vId] = cvnId;
            }
        } else {
            LocalRemapper[vId] = cvId->second;
        }
    }

    Net::Packet packet;
    // Packet Id
    packet << uint16_t(0);
    packet << wcId << Pos::GlobalChunk::Key(chunkPos);
    packet << uint16_t(voxels.size());

    for(const VoxelCube &cube : voxels) {
        packet << LocalRemapper[cube.Material]
            << cube.Left.X << cube.Left.Y << cube.Left.Z
            << cube.Right.X << cube.Right.Y << cube.Right.Z;
    }

    SimplePackets.push_back(std::move(packet));
}

void RemoteClient::prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const std::unordered_map<Pos::Local16_u, Node> &nodes)
{
    // Перебиндить идентификаторы нод

}

void RemoteClient::prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const LightPrism *lights)
{

}

void RemoteClient::prepareChunkRemove(WorldId_t worldId, Pos::GlobalChunk chunkPos)
{

}

void RemoteClient::prepareWorldRemove(WorldId_t worldId)
{
    
}

void RemoteClient::prepareEntitySwap(WorldId_t prevWorldId, Pos::GlobalRegion prevRegionPos, EntityId_t prevEntityId,
    WorldId_t newWorldId, Pos::GlobalRegion newRegionPos, EntityId_t newEntityId)
{

}

void RemoteClient::prepareEntityUpdate(WorldId_t worldId, Pos::GlobalRegion regionPos, 
    EntityId_t entityId, const Entity *entity)
{
    // Может прийти событие на сущность, про которую ещё ничего не знаем

    // Сопоставим с идентификатором клиента
    EntityId_c ceId = -1;

    auto pWorld = Remap.STC_Entityes.find(worldId);
    if(pWorld != Remap.STC_Entityes.end()) {
        auto pRegion = pWorld->second.find(regionPos);
        if(pRegion != pWorld->second.end()) {
            auto pId = pRegion->second.find(entityId);
            if(pId != pRegion->second.end()) {
                ceId = pId->second;
            }
        }
    }

    if(ceId == EntityId_c(-1)) {
        // Клиент ещё не знает о сущности
        // Выделяем идентификатор на стороне клиента для сущностей
        ceId = Remap.UsedEntityIdC._Find_first();
        if(ceId != EntityId_c(-1)) {
            Remap.UsedEntityIdC.reset(ceId);
            Remap.CTS_Entityes[ceId] = {worldId, regionPos, entityId};
            Remap.STC_Entityes[worldId][regionPos][entityId] = ceId;
        }

    }

    if(ceId == EntityId_c(-1))
        return; // У клиента закончились идентификаторы

    // Перебиндить ресурсы скомпилированных конвейеров
    // Отправить информацию о сущности
    // entity ceId
}

void RemoteClient::prepareEntityRemove(WorldId_t worldId, Pos::GlobalRegion regionPos, EntityId_t entityId)
{
    // Освобождаем идентификатор на стороне клиента
    auto pWorld = Remap.STC_Entityes.find(worldId);
    if(pWorld == Remap.STC_Entityes.end())
        return;

    auto pRegion = pWorld->second.find(regionPos);
    if(pRegion == pWorld->second.end())
        return;

    auto pId = pRegion->second.find(entityId);
    if(pId == pRegion->second.end())
        return;

    EntityId_c ceId = pId->second;
    Remap.UsedEntityIdC.set(ceId);

    {
        auto pceid = Remap.CTS_Entityes.find(ceId);
        if(pceid != Remap.CTS_Entityes.end())
            Remap.CTS_Entityes.erase(pceid);
    }

    pRegion->second.erase(pId);

    if(pRegion->second.empty()) {
        pWorld->second.erase(pRegion);

        if(pWorld->second.empty())
            Remap.STC_Entityes.erase(pWorld);
    }

    // Пакет об удалении сущности
    // ceId
}

void RemoteClient::preparePortalNew(PortalId_t portalId, void* portal) {}
void RemoteClient::preparePortalUpdate(PortalId_t portalId, void* portal) {}
void RemoteClient::preparePortalRemove(PortalId_t portalId) {}

void RemoteClient::prepareCameraSetEntity(WorldId_t worldId, Pos::GlobalChunk chunkPos, EntityId_t entityId) {}

ResourceRequest RemoteClient::pushPreparedPackets() {
    Socket.pushPackets(&SimplePackets);
    SimplePackets.clear();

    NextRequest.uniq();

    return std::move(NextRequest); 
}

void RemoteClient::informateDefTexture(const std::unordered_map<TextureId_t, std::shared_ptr<ResourceFile>> &textures) {

}

void RemoteClient::informateDefModel(const std::unordered_map<ModelId_t, std::shared_ptr<ResourceFile>> &models) {

}

void RemoteClient::informateDefSound(const std::unordered_map<SoundId_t, std::shared_ptr<ResourceFile>> &sounds) {

}

WorldId_c RemoteClient::rentWorldRemapId(WorldId_t wId) 
{
    WorldId_c wcId;

    auto cwId = Remap.STC_Worlds.find(wId);
    if(cwId == Remap.STC_Worlds.end()) {
        // Нужно забронировать идентификатор
        wcId = Remap.UsedWorldIdC._Find_first();
        if(wcId == WorldId_c(-1))
            // Нет свободных идентификаторов
            return wcId;
        else {
            NextRequest.NewWorlds.push_back(wId);
            Remap.UsedWorldIdC.reset(wcId);
            Remap.STC_Worlds[wId] = wcId;
        }
    } else {
        wcId = cwId->second;
    }

    return wcId;
}


}