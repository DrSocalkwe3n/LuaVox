#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include "Server/World.hpp"
#include <algorithm>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
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

    Net::Packet packet;
    packet << (uint8_t) ToClient::L1::System
        << (uint8_t) ToClient::L2System::Disconnect
        << (uint8_t) type << reason;

    std::string info;
    if(type == EnumDisconnect::ByInterface)
        info = "по запросу интерфейса " + reason;
    else if(type == EnumDisconnect::CriticalError)
        info = "на сервере произошла критическая ошибка " + reason;
    else if(type == EnumDisconnect::ProtocolError)
        info = "ошибка протокола (сервер) " + reason;

    Socket.pushPacket(std::move(packet));

    LOG.info() << "Игрок '" << Username << "' отключился " << info;
}

void RemoteClient::NetworkAndResource_t::prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
    const std::vector<DefVoxelId>& uniq_sorted_defines)
{
    Pos::bvec4u localChunk = chunkPos & 0x3;
    Pos::GlobalRegion regionPos = chunkPos >> 2;

    /*
        Обновить зависимости
        Запросить недостающие
        Отправить всё клиенту
    */

    std::vector<DefVoxelId>
        newTypes, /* Новые типы вокселей */
        lostTypes /* Потерянные типы вокселей */;

    // Отметим использование этих вокселей
    for(const DefVoxelId& id : uniq_sorted_defines) {
        auto iter = ResUses.DefVoxel.find(id);
        if(iter == ResUses.DefVoxel.end()) {
            // Новый тип
            newTypes.push_back(id);
            ResUses.DefVoxel[id] = 1;
        } else {
            // Увеличиваем счётчик
            iter->second++;
        }
    }

    auto iterWorld = ResUses.RefChunk.find(worldId);

    if(iterWorld != ResUses.RefChunk.end())
    // Исключим зависимости предыдущей версии чанка
    {
        auto iterRegion = iterWorld->second.find(regionPos);
        if(iterRegion != iterWorld->second.end()) {
            // Уменьшим счётчик зависимостей
            for(const DefVoxelId& id : iterRegion->second[localChunk.pack()].Voxel) {
                auto iter = ResUses.DefVoxel.find(id);
                assert(iter != ResUses.DefVoxel.end()); // Воксель должен быть в зависимостях
                if(--iter->second == 0) {
                    // Вокселя больше нет в зависимостях
                    lostTypes.push_back(id);
                    ResUses.DefVoxel.erase(iter);
                }
            }
        }
    } else {
        ResUses.RefChunk[worldId] = {};
        iterWorld = ResUses.RefChunk.find(worldId);
    }

    iterWorld->second[regionPos][localChunk.pack()].Voxel = uniq_sorted_defines;

    if(!newTypes.empty()) {
        // Добавляем новые типы в запрос
        NextRequest.Voxel.insert(NextRequest.Voxel.end(), newTypes.begin(), newTypes.end());
        for(DefVoxelId voxel : newTypes)
            ResUses.RefDefVoxel[voxel] = {};
    }

    if(!lostTypes.empty()) {
        for(const DefVoxelId& id : lostTypes) {
            auto iter = ResUses.RefDefVoxel.find(id);
            assert(iter != ResUses.RefDefVoxel.end()); // Должны быть описаны зависимости вокселя
            decrementAssets(std::move(iter->second));
            ResUses.RefDefVoxel.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeVoxel
                << id;
        }
    }

    checkPacketBorder(4+4+8+2+4+compressed_voxels.size());
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkVoxels
        << worldId << chunkPos.pack() << uint32_t(compressed_voxels.size());
    NextPacket.write((const std::byte*) compressed_voxels.data(), compressed_voxels.size());
}

void RemoteClient::NetworkAndResource_t::prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
    const std::vector<DefNodeId>& uniq_sorted_defines)
{
    Pos::bvec4u localChunk = chunkPos & 0x3;
    Pos::GlobalRegion regionPos = chunkPos >> 2;

    std::vector<DefNodeId>
        newTypes, /* Новые типы нод */
        lostTypes /* Потерянные типы нод */;

    // Отметим использование этих нод
    for(const DefNodeId& id : uniq_sorted_defines) {
        auto iter = ResUses.DefNode.find(id);
        if(iter == ResUses.DefNode.end()) {
            // Новый тип
            newTypes.push_back(id);
            ResUses.DefNode[id] = 1;
        } else {
            // Увеличиваем счётчик
            iter->second++;
        }
    }

    auto iterWorld = ResUses.RefChunk.find(worldId);

    if(iterWorld != ResUses.RefChunk.end())
    // Исключим зависимости предыдущей версии чанка
    {
        auto iterRegion = iterWorld->second.find(regionPos);
        if(iterRegion != iterWorld->second.end()) {
            // Уменьшим счётчик зависимостей
            for(const DefNodeId& id : iterRegion->second[localChunk.pack()].Node) {
                auto iter = ResUses.DefNode.find(id);
                assert(iter != ResUses.DefNode.end()); // Нода должна быть в зависимостях
                if(--iter->second == 0) {
                    // Ноды больше нет в зависимостях
                    lostTypes.push_back(id);
                    ResUses.DefNode.erase(iter);
                }
            }
        }
    } else {
        ResUses.RefChunk[worldId] = {};
        iterWorld = ResUses.RefChunk.find(worldId);
    }

    iterWorld->second[regionPos][localChunk.pack()].Node = uniq_sorted_defines;

    if(!newTypes.empty()) {
        // Добавляем новые типы в запрос
        NextRequest.Node.insert(NextRequest.Node.end(), newTypes.begin(), newTypes.end());
        for(DefNodeId node : newTypes)
            ResUses.RefDefNode[node] = {};
    }

    if(!lostTypes.empty()) {
        for(const DefNodeId& id : lostTypes) {
            auto iter = ResUses.RefDefNode.find(id);
            assert(iter != ResUses.RefDefNode.end()); // Должны быть описаны зависимости ноды
            decrementAssets(std::move(iter->second));
            ResUses.RefDefNode.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeNode
                << id;
        }
    }

    checkPacketBorder(4+4+8+4+compressed_nodes.size());
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkNodes
        << worldId << chunkPos.pack() << uint32_t(compressed_nodes.size());
    NextPacket.write((const std::byte*) compressed_nodes.data(), compressed_nodes.size());
}

void RemoteClient::NetworkAndResource_t::prepareRegionsRemove(WorldId_t worldId, std::vector<Pos::GlobalRegion> regionPoses)
{
    std::vector<DefVoxelId>
        lostTypesV /* Потерянные типы вокселей */;
    std::vector<DefNodeId>
        lostTypesN /* Потерянные типы нод */;

    for(Pos::GlobalRegion regionPos : regionPoses)
    // Уменьшаем зависимости вокселей и нод
    {
        auto iterWorld = ResUses.RefChunk.find(worldId);
        if(iterWorld == ResUses.RefChunk.end())
            return;

        auto iterRegion = iterWorld->second.find(regionPos);
        if(iterRegion == iterWorld->second.end())
            return;
         
        for(const auto &iterChunk : iterRegion->second) {
            for(const DefVoxelId& id : iterChunk.Voxel) {
                auto iter = ResUses.DefVoxel.find(id);
                assert(iter != ResUses.DefVoxel.end()); // Воксель должен быть в зависимостях
                if(--iter->second == 0) {
                    // Вокселя больше нет в зависимостях
                    lostTypesV.push_back(id);
                    ResUses.DefVoxel.erase(iter);
                }
            }

            for(const DefNodeId& id : iterChunk.Node) {
                auto iter = ResUses.DefNode.find(id);
                assert(iter != ResUses.DefNode.end()); // Нода должна быть в зависимостях
                if(--iter->second == 0) {
                    // Ноды больше нет в зависимостях
                    lostTypesN.push_back(id);
                    ResUses.DefNode.erase(iter);
                }
            }
        }

        iterWorld->second.erase(iterRegion);
    }

    if(!lostTypesV.empty()) {
        for(const DefVoxelId& id : lostTypesV) {
            auto iter = ResUses.RefDefVoxel.find(id);
            assert(iter != ResUses.RefDefVoxel.end()); // Должны быть описаны зависимости вокселя
            decrementAssets(std::move(iter->second));
            ResUses.RefDefVoxel.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeVoxel
                << id;
        }
    }

    if(!lostTypesN.empty()) {
        for(const DefNodeId& id : lostTypesN) {
            auto iter = ResUses.RefDefNode.find(id);
            assert(iter != ResUses.RefDefNode.end()); // Должны быть описаны зависимости ноды
            decrementAssets(std::move(iter->second));
            ResUses.RefDefNode.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeNode
                << id;
        }
    }


    for(Pos::GlobalRegion regionPos : regionPoses) {
        checkPacketBorder(16);
        NextPacket << (uint8_t) ToClient::L1::Content
            << (uint8_t) ToClient::L2Content::RemoveRegion
            << worldId << regionPos.pack();
    }
}

void RemoteClient::NetworkAndResource_t::prepareEntitiesUpdate(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities)
{
    for(auto& [entityId, entity] : entities) {
        // Сопоставим с идентификатором клиента
        ClientEntityId_t ceId = ReMapEntities.toClient(entityId);

        // Профиль новый
        {
            DefEntityId profile = entity->getDefId();
            auto iter = ResUses.DefEntity.find(profile);
            if(iter == ResUses.DefEntity.end()) {
                // Клиенту неизвестен профиль
                NextRequest.Entity.push_back(profile);
                ResUses.DefEntity[profile] = 1;
            } else
                iter->second++;
        }

        // Добавление модификационных зависимостей
        // incrementBinary({}, {}, {}, {}, {});

        // Старые данные
        {
            auto iterEntity = ResUses.RefEntity.find(entityId);
            if(iterEntity != ResUses.RefEntity.end()) {
                // Убавляем зависимость к старому профилю
                auto iterProfile = ResUses.DefEntity.find(iterEntity->second.Profile);
                assert(iterProfile != ResUses.DefEntity.end()); // Старый профиль должен быть
                if(--iterProfile->second == 0) {
                    // Старый профиль больше не нужен
                    auto iterProfileRef = ResUses.RefDefEntity.find(iterEntity->second.Profile);
                    decrementAssets(std::move(iterProfileRef->second));
                    ResUses.DefEntity.erase(iterProfile);
                }

                // Убавляем зависимость к модификационным данным
                // iterEntity->second.
                // decrementBinary({}, {}, {}, {}, {});
            }
        }

        // TODO: отправить клиенту
    }
}

void RemoteClient::NetworkAndResource_t::prepareEntitySwap(ServerEntityId_t prev, ServerEntityId_t next)
{
    ReMapEntities.rebindClientKey(prev, next);
}

void RemoteClient::NetworkAndResource_t::prepareEntitiesRemove(const std::vector<ServerEntityId_t>& entityIds)
{
    for(ServerEntityId_t entityId : entityIds) {
        ClientEntityId_t cId = ReMapEntities.erase(entityId);

        // Убавляем старые данные
        {
            auto iterEntity = ResUses.RefEntity.find(entityId);
            assert(iterEntity != ResUses.RefEntity.end()); // Зависимости должны быть

            // Убавляем модификационные заависимости
            //decrementBinary(std::vector<BinTextureId_t> &&textures, std::vector<BinAnimationId_t> &&animation, std::vector<BinSoundId_t> &&sounds, std::vector<BinModelId_t> &&models, std::vector<BinFontId_t> &&fonts)
            
            // Убавляем зависимость к профилю
            auto iterProfile = ResUses.DefEntity.find(iterEntity->second.Profile);
            assert(iterProfile != ResUses.DefEntity.end()); // Профиль должен быть
            if(--iterProfile->second == 0) {
                // Профиль больше не используется
                auto iterProfileRef = ResUses.RefDefEntity.find(iterEntity->second.Profile);

                decrementAssets(std::move(iterProfileRef->second));
            
                ResUses.RefDefEntity.erase(iterProfileRef);
                ResUses.DefEntity.erase(iterProfile);
            }
        }

        checkPacketBorder(16);
        NextPacket << (uint8_t) ToClient::L1::Content
            << (uint8_t) ToClient::L2Content::RemoveEntity
            << cId;
    }
}

void RemoteClient::NetworkAndResource_t::prepareWorldUpdate(WorldId_t worldId, World* world)
{
    // Добавление зависимостей
    ResUses.RefChunk[worldId];

    // Профиль
    {
        DefWorldId defWorld = world->getDefId();
        auto iterWorldProf = ResUses.DefWorld.find(defWorld);
        if(iterWorldProf == ResUses.DefWorld.end()) {
            // Профиль мира неизвестен клиенту
            ResUses.DefWorld[defWorld] = 1;
            NextRequest.World.push_back(defWorld);
        } else {
            iterWorldProf->second++;
        }
    }
    
    // Если есть предыдущая версия мира
    {
        auto iterWorld = ResUses.RefWorld.find(worldId);
        if(iterWorld != ResUses.RefWorld.end()) {
            // Мир известен клиенту

            // Убавляем модицикационные зависимости предыдущей версии мира
            // iterWorld->second.

            // Убавляем зависимости старого профиля
            auto iterWorldProf = ResUses.DefWorld.find(iterWorld->second.Profile);
            assert(iterWorldProf != ResUses.DefWorld.end()); // Старый профиль должен быть известен
            if(--iterWorldProf->second == 0) {
                // Старый профиль более ни кем не используется
                ResUses.DefWorld.erase(iterWorldProf);
                auto iterWorldProfRef = ResUses.RefDefWorld.find(iterWorld->second.Profile);
                assert(iterWorldProfRef != ResUses.RefDefWorld.end()); // Зависимости предыдущего профиля также должны быть
                decrementAssets(std::move(iterWorldProfRef->second));
                ResUses.RefDefWorld.erase(iterWorldProfRef);
            }
        }
    }

    // Указываем модификационные зависимости текущей версии мира
    ResUses.RefWorld[worldId] = {world->getDefId()};
    
    // TODO: отправить мир
}

void RemoteClient::NetworkAndResource_t::prepareWorldRemove(WorldId_t worldId)
{
    // Чанки уже удалены prepareChunkRemove
    // Обновление зависимостей
    auto iterWorld = ResUses.RefWorld.find(worldId);
    assert(iterWorld != ResUses.RefWorld.end());

    // Убавляем модификационные зависимости
    // decrementBinary(std::move(iterWorld->second.Texture), std::move(iterWorld->second.Model), {}, {});

    auto iterWorldProf = ResUses.DefWorld.find(iterWorld->second.Profile);
    assert(iterWorldProf != ResUses.DefWorld.end()); // Профиль мира должен быть
    if(--iterWorldProf->second == 0) {
        // Профиль мира более не используется
        ResUses.DefWorld.erase(iterWorldProf);
        // Убавляем зависимости профиля
        auto iterWorldProfDef = ResUses.RefDefWorld.find(iterWorld->second.Profile);
        assert(iterWorldProfDef != ResUses.RefDefWorld.end()); // Зависимости профиля должны быть
        decrementAssets(std::move(iterWorldProfDef->second));
        ResUses.RefDefWorld.erase(iterWorldProfDef);
    }

    ResUses.RefWorld.erase(iterWorld);

    auto iter = ResUses.RefChunk.find(worldId);
    assert(iter->second.empty());
    ResUses.RefChunk.erase(iter);
}

// void RemoteClient::NetworkAndResource_t::preparePortalUpdate(PortalId portalId, void* portal) {}
// void RemoteClient::NetworkAndResource_t::preparePortalRemove(PortalId portalId) {}

void RemoteClient::prepareCameraSetEntity(ServerEntityId_t entityId) {

}

ResourceRequest RemoteClient::pushPreparedPackets() {
    std::vector<Net::Packet> toSend;
    ResourceRequest nextRequest;

    {
        auto lock = NetworkAndResource.lock();

        if(lock->NextPacket.size())
            lock->SimplePackets.push_back(std::move(lock->NextPacket));

        toSend = std::move(lock->SimplePackets);
        nextRequest = std::move(lock->NextRequest);
    }

    if(AssetsInWork.AssetsPacket.size()) {
        toSend.push_back(std::move(AssetsInWork.AssetsPacket));
    }

    Socket.pushPackets(&toSend);
    toSend.clear();

    nextRequest.uniq();

    return std::move(nextRequest);
}

void RemoteClient::informateAssets(const std::vector<std::tuple<EnumAssets, ResourceId, const std::string, const std::string, Resource>>& resources)
{
    std::vector<std::tuple<EnumAssets, ResourceId, const std::string, const std::string, Hash_t, size_t>> newForClient;

    for(auto& [type, resId, domain, key, resource] : resources) {
        auto hash = resource.hash();

        // Проверка запрашиваемых клиентом ресурсов
        {
            auto iter = std::find(AssetsInWork.ClientRequested.begin(), AssetsInWork.ClientRequested.end(), hash);
            if(iter != AssetsInWork.ClientRequested.end())
            {
                auto it = std::lower_bound(AssetsInWork.OnClient.begin(), AssetsInWork.OnClient.end(), hash);

                if(it == AssetsInWork.OnClient.end() || *it != hash)
                    AssetsInWork.OnClient.insert(it, hash);

                AssetsInWork.ToSend.emplace_back(type, domain, key, resId, resource, 0);
            }
        }

        auto lock = NetworkAndResource.lock();
        // Информирование клиента о привязках ресурсов к идентификатору
        {
            // Посмотрим что известно клиенту
            if(auto iter = lock->ResUses.AssetsUse[(int) type].find(resId); 
                iter != lock->ResUses.AssetsUse[(int) type].end() 
                && std::get<Hash_t>(iter->second) != hash
            ) {
                // Требуется перепривязать идентификатор к новому хешу
                newForClient.push_back({(EnumAssets) type, resId, domain, key, hash, resource.size()});
                std::get<Hash_t>(iter->second) = hash;
            }
        }
    }

    // Отправляем новые привязки ресурсов
    if(!newForClient.empty()) {
        assert(newForClient.size() < 65535*4);
        auto lock = NetworkAndResource.lock();

        lock->checkPacketBorder(2+4+newForClient.size()*(1+4+32));
        lock->NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
            << ((uint8_t) ToClient::L2Resource::Bind) << uint32_t(newForClient.size());

        for(auto& [type, resId, domain, key, hash, size] : newForClient) {
            // TODO: может внести ограничение на длину домена и ключа?
            lock->NextPacket << uint8_t(type) << uint32_t(resId) << domain << key << size;
            lock->NextPacket.write((const std::byte*) hash.data(), hash.size());
        }
    }
}

void RemoteClient::NetworkAndResource_t::informateDefVoxel(const std::vector<std::pair<DefVoxelId, DefVoxel*>>& voxels)
{
    for(auto pair : voxels) {
        DefVoxelId id = pair.first;
        if(!ResUses.DefVoxel.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Voxel
            << id;
    }
}

void RemoteClient::NetworkAndResource_t::informateDefNode(const std::vector<std::pair<DefNodeId, DefNode*>>& nodes)
{
    // for(auto& [id, def] : nodes) {
    //     if(!ResUses.DefNode.contains(id))
    //         continue;
        
    //     size_t reserve = 0;
    //     for(int iter = 0; iter < 6; iter++)
    //         reserve += def->Texs[iter].Pipeline.size();

    //     checkPacketBorder(1+1+4+1+2*6+reserve);
    //     NextPacket << (uint8_t) ToClient::L1::Definition
    //         << (uint8_t) ToClient::L2Definition::Node
    //         << id << (uint8_t) def->DrawType;

    //     for(int iter = 0; iter < 6; iter++) {
    //         NextPacket << (uint16_t) def->Texs[iter].Pipeline.size();
    //         NextPacket.write((const std::byte*) def->Texs[iter].Pipeline.data(), def->Texs[iter].Pipeline.size());
    //     }

    //     ResUsesObj::RefDefBin_t refs;
    //     {
    //         auto &array = refs.Resources[(uint8_t) EnumBinResource::Texture];
    //         for(int iter = 0; iter < 6; iter++) {
    //             array.insert(array.end(), def->Texs[iter].BinTextures.begin(), def->Texs[iter].BinTextures.end());
    //         }

    //         std::sort(array.begin(), array.end());
    //         auto eraseLast = std::unique(array.begin(), array.end());
    //         array.erase(eraseLast, array.end());

    //         incrementBinary(refs);
    //     }

        
    //     {
    //         auto iterDefRef = ResUses.RefDefNode.find(id);
    //         if(iterDefRef != ResUses.RefDefNode.end()) {
    //             decrementBinary(std::move(iterDefRef->second));
    //             iterDefRef->second = std::move(refs);
    //         } else {
    //             ResUses.RefDefNode[id] = std::move(refs);
    //         }
    //     }

    // }
}

void RemoteClient::NetworkAndResource_t::informateDefWorld(const std::vector<std::pair<DefWorldId, DefWorld*>>& worlds)
{
    // for(auto pair : worlds) {
    //     DefWorldId_t id = pair.first;
    //     if(!ResUses.DefWorld.contains(id))
    //         continue;

    //     NextPacket << (uint8_t) ToClient::L1::Definition
    //         << (uint8_t) ToClient::L2Definition::World
    //         << id;
    // }
}

void RemoteClient::NetworkAndResource_t::informateDefPortal(const std::vector<std::pair<DefPortalId, DefPortal*>>& portals)
{
    // for(auto pair : portals) {
    //     DefPortalId_t id = pair.first;
    //     if(!ResUses.DefPortal.contains(id))
    //         continue;

    //     NextPacket << (uint8_t) ToClient::L1::Definition
    //         << (uint8_t) ToClient::L2Definition::Portal
    //         << id;
    // }
}

void RemoteClient::NetworkAndResource_t::informateDefEntity(const std::vector<std::pair<DefEntityId, DefEntity*>>& entityes)
{
    // for(auto pair : entityes) {
    //     DefEntityId_t id = pair.first;
    //     if(!ResUses.DefEntity.contains(id))
    //         continue;

    //     NextPacket << (uint8_t) ToClient::L1::Definition
    //         << (uint8_t) ToClient::L2Definition::Entity
    //         << id;
    // }
}

void RemoteClient::NetworkAndResource_t::informateDefItem(const std::vector<std::pair<DefItemId, DefItem*>>& items)
{
    // for(auto pair : items) {
    //     DefItemId_t id = pair.first;
    //     if(!ResUses.DefNode.contains(id))
    //         continue;

    //     NextPacket << (uint8_t) ToClient::L1::Definition
    //         << (uint8_t) ToClient::L2Definition::FuncEntity
    //         << id;
    // }
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
    case ToServer::L2System::Test_CAM_PYR_POS:
    {
        Pos::Object newPos;
        newPos.x = co_await sock.read<decltype(CameraPos.x)>();
        newPos.y = co_await sock.read<decltype(CameraPos.y)>();
        newPos.z = co_await sock.read<decltype(CameraPos.z)>();

        CameraPos = newPos;

        for(int iter = 0; iter < 5; iter++)
            CameraQuat.Data[iter] = co_await sock.read<uint8_t>();

        co_return;
    }
    case ToServer::L2System::BlockChange:
    {
        uint8_t action = co_await sock.read<uint8_t>();
        Actions.lock()->push(action);
        co_return;
    }
    default:
        protocolError();
    }
}

void RemoteClient::NetworkAndResource_t::incrementAssets(const ResUses_t::RefAssets_t& bin) {
    for(int iter = 0; iter < 5; iter++) {
        auto &use = ResUses.AssetsUse[iter];

        for(ResourceId id : bin.Resources[iter]) {
            if(++std::get<0>(use[id]) == 1) {
                NextRequest.AssetsInfo[iter].push_back(id);
                // LOG.debug() << "Новое определение (тип " << iter << ") -> " << id;
            }
        }
    }
}

void RemoteClient::NetworkAndResource_t::decrementAssets(ResUses_t::RefAssets_t&& bin) {
    std::vector<std::tuple<EnumAssets, ResourceId>> lost;

    for(int iter = 0; iter < (int) EnumAssets::MAX_ENUM; iter++) {
        auto &use = ResUses.AssetsUse[iter];

        for(ResourceId id : bin.Resources[iter]) {
            if(--std::get<0>(use[id]) == 0) {
                use.erase(use.find(id));

                lost.push_back({(EnumAssets) iter, id});
                // LOG.debug() << "Потеряно определение (тип " << iter << ") -> " << id;
            }
        }
    }

    if(!lost.empty()) {
        assert(lost.size() < 65535*4);

        checkPacketBorder(1+1+4+lost.size()*(1+4));
        NextPacket << (uint8_t) ToClient::L1::Resource
            << (uint8_t) ToClient::L2Resource::Lost
            << uint32_t(lost.size());

        for(auto& [type, id] : lost)
            NextPacket /* << uint8_t(type)*/  << uint32_t(id);
    }
}

void RemoteClient::onUpdate() {
    Pos::Object cameraPos = CameraPos;

    Pos::GlobalRegion r1 = LastPos >> 12 >> 4 >> 2;
    Pos::GlobalRegion r2 = cameraPos >> 12 >> 4 >> 2;
    if(r1 != r2) {
        CrossedRegion = true;
    }

    if(!Actions.get_read().empty()) {
        auto lock = Actions.lock();
        while(!lock->empty()) {
            uint8_t action = lock->front();
            lock->pop();

            glm::quat q = CameraQuat.toQuat();
            glm::vec4 v = glm::mat4(q)*glm::vec4(0, 0, -6, 1);
            Pos::GlobalNode pos = (Pos::GlobalNode) (glm::vec3) v;
            pos += cameraPos >> Pos::Object_t::BS_Bit;

            if(action == 0) {
                // Break
                Break.push(pos);

            } else if(action == 1) {
                // Build
                Build.push(pos);
            }
        }
    }

    LastPos = cameraPos;

    // Отправка ресурсов
    if(!AssetsInWork.ToSend.empty()) {
        auto& toSend = AssetsInWork.ToSend;
        size_t chunkSize = std::max<size_t>(1'024'000 / toSend.size(), 4096);

        Net::Packet& p = AssetsInWork.AssetsPacket;

        bool hasFullSended = false;

        for(auto& [type, domain, key, id, res, sended] : toSend) {
            if(sended == 0) {
                // Оповещаем о начале отправки ресурса
                p << (uint8_t) ToClient::L1::Resource
                    << (uint8_t) ToClient::L2Resource::InitResSend
                    << uint32_t(res.size());
                p.write((const std::byte*) res.hash().data(), 32);
                p << uint32_t(id) << uint8_t(type) << domain << key;
            }

            // Отправляем чанк
            size_t willSend = std::min(chunkSize, res.size()-sended);
            p << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::ChunkSend;
            p.write((const std::byte*) res.hash().data(), 32);
            p << uint32_t(willSend);
            p.write(res.data() + sended, willSend);
            sended += willSend;

            if(sended == willSend) {
                hasFullSended = true;
            }
        }

        if(hasFullSended) {
            for(ssize_t iter = toSend.size()-1; iter > 0; iter--) {
                if(std::get<4>(toSend[iter]).size() == std::get<5>(toSend[iter])) {
                    toSend.erase(toSend.begin()+iter);
                }
            }
        }
    }
}

std::vector<std::tuple<WorldId_t, Pos::Object, uint8_t>> RemoteClient::getViewPoints() {
    return {{0, CameraPos, 1}};
}

}