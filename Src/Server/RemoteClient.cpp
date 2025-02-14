#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
#include <unordered_map>
#include <unordered_set>
#include "World.hpp"
#include <Common/Packets.hpp>


namespace LV::Server {

RemoteClient::~RemoteClient() {
    shutdown(EnumDisconnect::ByInterface, "~RemoteClient()");
    if(Socket.isAlive()) {
        Socket.closeRead();
    }
    
    UseLock.wait_no_use();
}

coro<> RemoteClient::run() {
    auto useLock = UseLock.lock();

    try {
        while(!IsGoingShutdown && IsConnected) {
            co_await readPacket(Socket);
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

void RemoteClient::shutdown(EnumDisconnect type, const std::string reason) {
    if(IsGoingShutdown)
        return;

    IsGoingShutdown = true;

    NextPacket << (uint8_t) ToClient::L1::System
        << (uint8_t) ToClient::L2System::Disconnect
        << (uint8_t) type << reason;

    std::string info;
    if(type == EnumDisconnect::ByInterface)
        info = "по запросу интерфейса " + reason;
    else if(type == EnumDisconnect::CriticalError)
        info = "на сервере произошла критическая ошибка " + reason;
    else if(type == EnumDisconnect::ProtocolError)
        info = "ошибка протокола (сервер) " + reason;

    LOG.info() << "Игрок '" << Username << "' отключился " << info;
}


// Может прийти событие на чанк, про который ещё ничего не знаем
void RemoteClient::prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const std::vector<VoxelCube> &voxels)
{
    WorldId_c wcId = worldId ? ResRemap.Worlds.toClient(worldId) : 0;
    assert(wcId != WorldId_c(-1)); // Пока ожидается, что игрок не будет одновременно наблюдать 256 миров

    // Перебиндить идентификаторы вокселей
    std::vector<DefVoxelId_t> NeedVoxels;
    NeedVoxels.reserve(voxels.size());

    for(const VoxelCube &cube : voxels) {
        NeedVoxels.push_back(cube.VoxelId);
    }

    std::unordered_set<DefVoxelId_t> NeedVoxelsSet(NeedVoxels.begin(), NeedVoxels.end());

    // Собираем информацию о конвертации идентификаторов
    std::unordered_map<DefVoxelId_t, DefVoxelId_c> LocalRemapper;
    for(DefVoxelId_t vId : NeedVoxelsSet) {
        LocalRemapper[vId] = ResRemap.DefVoxels.toClient(vId);
    }

    // Проверить новые и забытые определения вокселей
    {
        std::unordered_set<DefVoxelId_t> &prevSet = ResUses.ChunkVoxels[{worldId, chunkPos}];
        std::unordered_set<DefVoxelId_t> &nextSet = NeedVoxelsSet;

        std::vector<DefVoxelId_t> newVoxels, lostVoxels;
        for(DefVoxelId_t id : prevSet) {
            if(!nextSet.contains(id)) {
                if(--ResUses.DefVoxel[id] == 0) {
                    // Определение больше не используется

                    ResUses.DefVoxel.erase(ResUses.DefVoxel.find(id));
                    DefVoxelId_c cId = ResRemap.DefVoxels.erase(id);
                    // TODO: отправить пакет потери идентификатора
                    LOG.debug() << "Определение вокселя потеряно: " << id << " -> " << cId;
                }
            }
        }

        for(DefVoxelId_t id : nextSet) {
            if(!prevSet.contains(id)) {
                if(++ResUses.DefVoxel[id] == 1) {
                    // Определение только появилось
                    NextRequest.NewVoxels.push_back(id);
                    DefVoxelId_c cId = ResRemap.DefVoxels.toClient(id);
                    LOG.debug() << "Новое определение вокселя: " << id << " -> " << cId;
                }
            }
        }

        prevSet = std::move(nextSet);
    }

    LOG.debug() << "Воксели чанка: " << worldId << " / " << chunkPos.X << ":" << chunkPos.Y << ":" << chunkPos.Z;

    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkVoxels << wcId 
        << Pos::GlobalChunk::Key(chunkPos);
    NextPacket << uint16_t(voxels.size());
    // TODO: 
    for(const VoxelCube &cube : voxels) {
        NextPacket << LocalRemapper[cube.VoxelId]
            << cube.Left.X << cube.Left.Y << cube.Left.Z
            << cube.Right.X << cube.Right.Y << cube.Right.Z;
    }
}

void RemoteClient::prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const std::unordered_map<Pos::Local16_u, Node> &nodes)
{

}

void RemoteClient::prepareChunkUpdate_LightPrism(WorldId_t worldId, Pos::GlobalChunk chunkPos, 
    const LightPrism *lights)
{

}

void RemoteClient::prepareChunkRemove(WorldId_t worldId, Pos::GlobalChunk chunkPos)
{
    // Понизим зависимости ресурсов
    std::unordered_set<DefVoxelId_t> &prevSet = ResUses.ChunkVoxels[{worldId, chunkPos}];
    for(DefVoxelId_t id : prevSet) {
        if(--ResUses.DefVoxel[id] == 0) {
            // Определение больше не используется

            ResUses.DefVoxel.erase(ResUses.DefVoxel.find(id));
            DefVoxelId_c cId = ResRemap.DefVoxels.erase(id);
            // TODO: отправить пакет потери идентификатора
            LOG.debug() << "Определение вокселя потеряно: " << id << " -> " << cId;
        }
    }

    LOG.debug() << "Чанк потерян: " << worldId << " / " << chunkPos.X << ":" << chunkPos.Y << ":" << chunkPos.Z;
    WorldId_c cwId = ResRemap.Worlds.toClient(worldId);
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::RemoveChunk
        << cwId << Pos::GlobalChunk::Key(chunkPos);
}

void RemoteClient::prepareWorldNew(WorldId_t worldId, World* world)
{
    ResUsesObj::WorldResourceUse &res = ResUses.Worlds[worldId];
    res.DefId = world->getDefId();
    if(++ResUses.DefWorld[res.DefId] == 1) {
        // Новое определение мира
        DefWorldId_c cdId = ResRemap.DefWorlds.toClient(res.DefId);
        NextRequest.NewWorlds.push_back(res.DefId);

        LOG.debug() << "Новое определение мира: " << res.DefId << " -> " << int(cdId);
        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::World
            << cdId;
    }

    incrementBinary(res.Textures, res.Sounds, res.Models);

    WorldId_c cId = ResRemap.Worlds.toClient(worldId);
    LOG.debug() << "Новый мир: " << worldId << " -> " << int(cId);
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::World
        << cId;
}

void RemoteClient::prepareWorldUpdate(WorldId_t worldId, World* world)
{
    ResUsesObj::WorldResourceUse &res = ResUses.Worlds[worldId];

    if(res.DefId != world->getDefId()) {
        DefWorldId_t newDef = world->getDefId();

        if(--ResUses.DefWorld[res.DefId] == 0) {
            // Определение больше не используется
            ResUses.DefWorld.erase(ResUses.DefWorld.find(res.DefId));
            DefWorldId_c cdId = ResRemap.DefWorlds.erase(res.DefId);

            // TODO: отправить пакет потери идентификатора
            LOG.debug() << "Определение мира потеряно: " << res.DefId << " -> " << cdId;
        }
        
        if(++ResUses.DefWorld[newDef] == 1) {
            // Новое определение мира
            DefWorldId_c cdId = ResRemap.DefWorlds.toClient(newDef);
            NextRequest.NewWorlds.push_back(newDef);

            // incrementBinary(Textures, Sounds, Models);
            // TODO: отправить пакет о новом определении мира
            LOG.debug() << "Новое определение мира: " << newDef << " -> " << cdId;
        }

        res.DefId = newDef;
    }

    // TODO: определить различия между переопределением поверх определений
    std::unordered_set<BinTextureId_t> lostTextures, newTextures;
    std::unordered_set<BinSoundId_t> lostSounds, newSounds;
    std::unordered_set<BinModelId_t> lostModels, newModels;

    decrementBinary(lostTextures, lostSounds, lostModels);
    decrementBinary(newTextures, newSounds, newModels);

    WorldId_c cId = ResRemap.Worlds.toClient(worldId);
    // TODO: отправить пакет об изменении мира
    LOG.debug() << "Изменение мира: " << worldId << " -> " << cId;
}

void RemoteClient::prepareWorldRemove(WorldId_t worldId)
{
    // Чанки уже удалены prepareChunkRemove
    // Понизим зависимости ресурсов
    ResUsesObj::WorldResourceUse &res = ResUses.Worlds[worldId];

    WorldId_c cWorld = ResUses.Worlds.erase(worldId);
    LOG.debug() << "Мир потерян: " << worldId << " -> " << cWorld;

    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::RemoveWorld
        << cWorld;

    if(--ResUses.DefWorld[res.DefId] == 0) {
        // Определение мира потеряно
        ResUses.DefWorld.erase(ResUses.DefWorld.find(res.DefId));
        DefWorldId_c cdWorld = ResRemap.DefWorlds.erase(res.DefId);

        // TODO: отправить пакет о потере определения мира
        LOG.debug() << "Определение мира потеряно: " << res.DefId << " -> " << cdWorld;
    }

    decrementBinary(res.Textures, res.Sounds, res.Models);
}

void RemoteClient::prepareEntitySwap(GlobalEntityId_t prev, GlobalEntityId_t next)
{
    ResRemap.Entityes.rebindClientKey(prev, next);
    LOG.debug() << "Ребинд сущности: " << std::get<0>(prev) << " / " << std::get<1>(prev).X << ":" 
        << std::get<1>(prev).Y << ":" << std::get<1>(prev).Z << " / " << std::get<2>(prev)
        << "  ->  " << std::get<0>(next) << " / " << std::get<1>(next).X << ":" 
        << std::get<1>(next).Y << ":" << std::get<1>(next).Z << " / " << std::get<2>(next);
}

void RemoteClient::prepareEntityUpdate(GlobalEntityId_t entityId, const Entity *entity)
{
    // Может прийти событие на сущность, про которую ещё ничего не знаем

    // Сопоставим с идентификатором клиента
    EntityId_c ceId = ResRemap.Entityes.toClient(entityId);

    auto iter = ResUses.Entity.find(entityId);
    if(iter == ResUses.Entity.end()) {
        // Новая сущность

        // WorldId_c cwId = ResRemap.Worlds.toClient(std::get<0>(entityId));

        ResUsesObj::EntityResourceUse &res = ResUses.Entity[entityId];
        res.DefId = entity->getDefId();

        if(++ResUses.DefEntity[res.DefId] == 1) {
            // Новое определение
            NextRequest.NewEntityes.push_back(res.DefId);
            DefEntityId_c cId = ResRemap.DefEntityes.toClient(res.DefId);
            LOG.debug() << "Новое определение сущности: " << res.DefId << " -> " << cId;
            // TODO: Отправить пакет о новом определении

            // incrementBinary(Textures, Sounds, Models);
        }

        incrementBinary(res.Textures, res.Sounds, res.Models);

        LOG.debug() << "Новая сущность: " << std::get<0>(entityId) << " / " << std::get<1>(entityId).X << ":" 
            << std::get<1>(entityId).Y << ":" << std::get<1>(entityId).Z << " / " << std::get<2>(entityId)
            << "  ->  " << ceId;

    } else {
        ResUsesObj::EntityResourceUse &res = iter->second;
        LOG.debug() << "Обновление сущности: " << ceId;
    }
}

void RemoteClient::prepareEntityRemove(GlobalEntityId_t entityId)
{
    EntityId_c cId = ResRemap.Entityes.erase(entityId);
    ResUsesObj::EntityResourceUse &res = ResUses.Entity[entityId];

    if(--ResUses.DefEntity[res.DefId] == 0) {
        LOG.debug() << "Потеряли определение сущности: " << res.DefId << " -> " << cId;
        ResUses.DefEntity.erase(ResUses.DefEntity.find(res.DefId));
        ResRemap.DefEntityes.erase(res.DefId);

        // decrementBinary(std::unordered_set<BinTextureId_t> &textures, std::unordered_set<BinSoundId_t> &sounds, std::unordered_set<BinModelId_t> &models)
    }

    LOG.debug() << "Сущность потеряна: " << cId;

    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::RemoveEntity
        << cId;
}

void RemoteClient::preparePortalNew(PortalId_t portalId, void* portal) {}
void RemoteClient::preparePortalUpdate(PortalId_t portalId, void* portal) {}
void RemoteClient::preparePortalRemove(PortalId_t portalId) {}

void RemoteClient::prepareCameraSetEntity(GlobalEntityId_t entityId) {

}

ResourceRequest RemoteClient::pushPreparedPackets() {
    if(NextPacket.size())
        SimplePackets.push_back(std::move(NextPacket));

    Socket.pushPackets(&SimplePackets);
    SimplePackets.clear();

    NextRequest.uniq();

    return std::move(NextRequest);
}

void RemoteClient::informateDefTexture(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t sId = pair.first;
        if(!ResUses.BinTexture.contains(sId))
            continue;

        TextureId_c cId = ResRemap.BinTextures.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
           << (uint8_t) ToClient::L2Resource::Texture << cId;
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << (uint8_t) ToClient::L1::Resource    // Принудительная отправка
            << (uint8_t) ToClient::L2Resource::InitResSend
            << uint8_t(0) << uint8_t(0) << cId
            << uint32_t(pair.second->Data.size());
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << uint8_t(0) << uint32_t(pair.second->Data.size());

        size_t pos = 0;
        while(pos < pair.second->Data.size()) {
            if(NextPacket.size() > 64000) {
                SimplePackets.push_back(std::move(NextPacket));
            }

            size_t need = std::min(pair.second->Data.size()-pos, std::min<size_t>(NextPacket.size(), 64000));
            NextPacket.write(pair.second->Data.data()+pos, need);
            pos += need;
        }
    }

    if(NextPacket.size())
        SimplePackets.push_back(std::move(NextPacket));
}

void RemoteClient::informateDefSound(const std::unordered_map<BinSoundId_t, std::shared_ptr<ResourceFile>> &sounds)
{
    for(auto pair : sounds) {
        BinSoundId_t sId = pair.first;
        if(!ResUses.BinSound.contains(sId))
            continue;

        SoundId_c cId = ResRemap.BinSounds.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
           << (uint8_t) ToClient::L2Resource::Sound << cId;
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << (uint8_t) ToClient::L1::Resource    // Принудительная отправка
            << (uint8_t) ToClient::L2Resource::InitResSend
            << uint8_t(0) << uint8_t(1) << cId
            << uint32_t(pair.second->Data.size());
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << uint8_t(0) << uint32_t(pair.second->Data.size());

        size_t pos = 0;
        while(pos < pair.second->Data.size()) {
            if(NextPacket.size() >= 64000) {
                SimplePackets.push_back(std::move(NextPacket));
            }

            size_t need = std::min(pair.second->Data.size()-pos, std::min<size_t>(NextPacket.size(), 64000));
            NextPacket.write(pair.second->Data.data()+pos, need);
            pos += need;
        }
    }
}

void RemoteClient::informateDefModel(const std::unordered_map<BinModelId_t, std::shared_ptr<ResourceFile>> &models)
{
    for(auto pair : models) {
        BinModelId_t sId = pair.first;
        if(!ResUses.BinModel.contains(sId))
            continue;

        ModelId_c cId = ResRemap.BinModels.toClient(sId);
        
        NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
           << (uint8_t) ToClient::L2Resource::Model << cId;
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << (uint8_t) ToClient::L1::Resource    // Принудительная отправка
            << (uint8_t) ToClient::L2Resource::InitResSend
            << uint8_t(0) << uint8_t(2) << cId
            << uint32_t(pair.second->Data.size());
        for(auto part : pair.second->Hash)
            NextPacket << part;

        NextPacket << uint8_t(0) << uint32_t(pair.second->Data.size());

        size_t pos = 0;
        while(pos < pair.second->Data.size()) {
            if(NextPacket.size() >= 64000) {
                SimplePackets.push_back(std::move(NextPacket));
            }

            size_t need = std::min(pair.second->Data.size()-pos, std::min<size_t>(NextPacket.size(), 64000));
            NextPacket.write(pair.second->Data.data()+pos, need);
            pos += need;
        }
    }
}

void RemoteClient::informateDefWorld(const std::unordered_map<DefWorldId_t, World*> &worlds)
{
    for(auto pair : worlds) {
        DefWorldId_t sId = pair.first;
        if(!ResUses.DefWorld.contains(sId))
            continue;

        DefWorldId_c cId = ResRemap.DefWorlds.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::World
            << cId;
    }
}

void RemoteClient::informateDefVoxel(const std::unordered_map<DefVoxelId_t, void*> &voxels)
{
    for(auto pair : voxels) {
        DefVoxelId_t sId = pair.first;
        if(!ResUses.DefWorld.contains(sId))
            continue;

        DefVoxelId_c cId = ResRemap.DefVoxels.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Voxel
            << cId;
    }
}

void RemoteClient::informateDefNode(const std::unordered_map<DefNodeId_t, void*> &nodes)
{
    for(auto pair : nodes) {
        DefNodeId_t sId = pair.first;
        if(!ResUses.DefNode.contains(sId))
            continue;

        DefNodeId_c cId = ResRemap.DefNodes.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Node
            << cId;
    }
}

void RemoteClient::informateDefEntityes(const std::unordered_map<DefEntityId_t, void*> &entityes)
{
    for(auto pair : entityes) {
        DefEntityId_t sId = pair.first;
        if(!ResUses.DefNode.contains(sId))
            continue;

        DefEntityId_c cId = ResRemap.DefEntityes.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Entity
            << cId;
    }
}

void RemoteClient::informateDefPortals(const std::unordered_map<DefPortalId_t, void*> &portals)
{
    for(auto pair : portals) {
        DefPortalId_t sId = pair.first;
        if(!ResUses.DefNode.contains(sId))
            continue;

        DefPortalId_c cId = ResRemap.DefPortals.toClient(sId);

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Portal
            << cId;
    }
}

void RemoteClient::protocolError() {
    shutdown(EnumDisconnect::ProtocolError, "Ошибка протокола");
}

coro<> RemoteClient::readPacket(Net::AsyncSocket &sock) {
    uint8_t first = co_await sock.read<uint8_t>();

    switch((ToServer::L1) first) {
    case ToServer::L1::System: co_await rP_System(sock);       co_return;
    default:
        protocolError();
    }
}

coro<> RemoteClient::rP_System(Net::AsyncSocket &sock) {
    uint8_t second = co_await sock.read<uint8_t>();

    switch((ToServer::L2System) second) {
    case ToServer::L2System::InitEnd:

        co_return;
    case ToServer::L2System::Disconnect:
    {
        EnumDisconnect type = (EnumDisconnect) co_await sock.read<uint8_t>();
        shutdown(EnumDisconnect::ByInterface, "Вы были отключены от игры");
        std::string reason;
        if(type == EnumDisconnect::CriticalError)
            reason = ": Критическая ошибка";
        else
            reason = ": Ошибка протокола (клиент)";

        LOG.info() << "Игрок '" << Username << "' отключился" << reason;
        
        co_return;
    }
    default:
        protocolError();
    }
}

void RemoteClient::incrementBinary(std::unordered_set<BinTextureId_t> &textures, std::unordered_set<BinSoundId_t> &sounds,
    std::unordered_set<BinModelId_t> &models) 
{
    for(BinTextureId_t id : textures) {
        if(++ResUses.BinTexture[id] == 1) {
            TextureId_c cId = ResRemap.BinTextures.toClient(id);
            NextRequest.NewTextures.push_back(id);
            LOG.debug() << "Новое определение текстуры: " << id << " -> " << cId;
        }
    }

    for(BinSoundId_t id : sounds) {
        if(++ResUses.BinSound[id] == 1) {
            SoundId_c cId = ResRemap.BinSounds.toClient(id);
            NextRequest.NewSounds.push_back(id);
            LOG.debug() << "Новое определение звука: " << id << " -> " << cId;
        }
    }

    for(BinModelId_t id : sounds) {
        if(++ResUses.BinModel[id] == 1) {
            ModelId_c cId = ResRemap.BinModels.toClient(id);
            NextRequest.NewModels.push_back(id);
            LOG.debug() << "Новое определение модели: " << id << " -> " << cId;
        }
    }
}

void RemoteClient::decrementBinary(std::unordered_set<BinTextureId_t> &textures, std::unordered_set<BinSoundId_t> &sounds,
    std::unordered_set<BinModelId_t> &models)
{
    for(BinTextureId_t id : textures) {
        if(--ResUses.BinTexture[id] == 0) {
            ResUses.BinTexture.erase(ResUses.BinTexture.find(id));
            TextureId_c cId = ResRemap.BinTextures.erase(id);
            LOG.debug() << "Потеряно определение текстуры: " << id << " -> " << cId;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeTexture
                << cId;
        }
    }

    for(BinSoundId_t id : sounds) {
        if(--ResUses.BinSound[id] == 0) {
            ResUses.BinSound.erase(ResUses.BinSound.find(id));
            SoundId_c cId = ResRemap.BinSounds.erase(id);
            LOG.debug() << "Потеряно определение звука: " << id << " -> " << cId;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeSound
                << cId;
        }
    }

    for(BinModelId_t id : sounds) {
        if(--ResUses.BinModel[id] == 0) {
            ResUses.BinModel.erase(ResUses.BinModel.find(id));
            ModelId_c cId = ResRemap.BinModels.erase(id);
            LOG.debug() << "Потеряно определение модели: " << id << " -> " << cId;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeModel
                << cId;
        }
    }
}


}