#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
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

void RemoteClient::murky_prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
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
            decrementBinary(std::move(iter->second));
            ResUses.RefDefVoxel.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeVoxel
                << id;
        }
    }

    murkyCheckPacketBorder(4+4+8+2+4+compressed_voxels.size());
    MurkyNextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkVoxels
        << worldId << chunkPos.pack() << uint32_t(compressed_voxels.size());
    MurkyNextPacket.write((const std::byte*) compressed_voxels.data(), compressed_voxels.size());
}

void RemoteClient::maybe_prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
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
            decrementBinary(std::move(iter->second));
            ResUses.RefDefNode.erase(iter);

            checkPacketBorder(16);
            MurkyNextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeNode
                << id;
        }
    }

    checkPacketBorder(4+4+8+4+compressed_nodes.size());
    MurkyNextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkNodes
        << worldId << chunkPos.pack() << uint32_t(compressed_nodes.size());
    MurkyNextPacket.write((const std::byte*) compressed_nodes.data(), compressed_nodes.size());
}

void RemoteClient::prepareRegionRemove(WorldId_t worldId, Pos::GlobalRegion regionPos) {
    std::vector<DefVoxelId>
        lostTypesV /* Потерянные типы вокселей */;
    std::vector<DefNodeId>
        lostTypesN /* Потерянные типы нод */;

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
            decrementBinary(std::move(iter->second));
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
            decrementBinary(std::move(iter->second));
            ResUses.RefDefNode.erase(iter);

            checkPacketBorder(16);
            NextPacket << (uint8_t) ToClient::L1::Definition
                << (uint8_t) ToClient::L2Definition::FreeNode
                << id;
        }
    }

    checkPacketBorder(16);
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::RemoveRegion
        << worldId << regionPos.pack();
}

void RemoteClient::prepareEntityUpdate(ServerEntityId_t entityId, const Entity *entity)
{
    // Сопоставим с идентификатором клиента
    ClientEntityId_t ceId = ResRemap.Entityes.toClient(entityId);

    // Профиль новый
    {
        DefEntityId_t profile = entity->getDefId();
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
                decrementBinary(std::move(iterProfileRef->second));
                ResUses.DefEntity.erase(iterProfile);
            }

            // Убавляем зависимость к модификационным данным
            // iterEntity->second.
            // decrementBinary({}, {}, {}, {}, {});
        }
    }

    // TODO: отправить клиенту
}

void RemoteClient::prepareEntitySwap(ServerEntityId_t prev, ServerEntityId_t next)
{
    ResRemap.Entityes.rebindClientKey(prev, next);
}

void RemoteClient::prepareEntityRemove(ServerEntityId_t entityId)
{
    ClientEntityId_t cId = ResRemap.Entityes.erase(entityId);

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

            decrementBinary(std::move(iterProfileRef->second));
        
            ResUses.RefDefEntity.erase(iterProfileRef);
            ResUses.DefEntity.erase(iterProfile);
        }
    }

    checkPacketBorder(16);
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::RemoveEntity
        << cId;
}

void RemoteClient::prepareWorldUpdate(WorldId_t worldId, World* world)
{
    // Добавление зависимостей
    ResUses.RefChunk[worldId];

    // Профиль
    {
        DefWorldId_t defWorld = world->getDefId();
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
                decrementBinary(std::move(iterWorldProfRef->second));
                ResUses.RefDefWorld.erase(iterWorldProfRef);
            }
        }
    }

    // Указываем модификационные зависимости текущей версии мира
    ResUses.RefWorld[worldId] = {world->getDefId()};
    
    // TODO: отправить мир
}

void RemoteClient::prepareWorldRemove(WorldId_t worldId)
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
        decrementBinary(std::move(iterWorldProfDef->second));
        ResUses.RefDefWorld.erase(iterWorldProfDef);
    }

    ResUses.RefWorld.erase(iterWorld);

    auto iter = ResUses.RefChunk.find(worldId);
    assert(iter->second.empty());
    ResUses.RefChunk.erase(iter);
}

void RemoteClient::preparePortalUpdate(PortalId_t portalId, void* portal) {}
void RemoteClient::preparePortalRemove(PortalId_t portalId) {}

void RemoteClient::prepareCameraSetEntity(ServerEntityId_t entityId) {

}

ResourceRequest RemoteClient::pushPreparedPackets() {
    if(NextPacket.size())
        SimplePackets.push_back(std::move(NextPacket));

    Socket.pushPackets(&SimplePackets);
    SimplePackets.clear();

    NextRequest.uniq();

    return std::move(NextRequest);
}

void RemoteClient::informateBinary(const std::vector<std::shared_ptr<ResourceFile>>& resources) {
    for(auto& resource : resources) {
        auto &hash = resource->Hash;

        auto iter = std::find(NeedToSend.begin(), NeedToSend.end(), hash);
        if(iter == NeedToSend.end())
            continue; // Клиенту не требуется этот ресурс

        {
            auto it = std::lower_bound(ClientBinaryCache.begin(), ClientBinaryCache.end(), hash);

            if(it == ClientBinaryCache.end() || *it != hash)
                ClientBinaryCache.insert(it, hash);
        }

        // Полная отправка ресурса
        checkPacketBorder(2+4+32+4);
        NextPacket << (uint8_t) ToClient::L1::Resource    // Принудительная полная отправка
            << (uint8_t) ToClient::L2Resource::InitResSend
            << uint32_t(resource->Data.size());
        NextPacket.write((const std::byte*) hash.data(), hash.size());

        NextPacket << uint32_t(resource->Data.size());

        size_t pos = 0;
        while(pos < resource->Data.size()) {
            checkPacketBorder(0);
            size_t need = std::min(resource->Data.size()-pos, std::min<size_t>(NextPacket.size(), 64000));
            NextPacket.write((const std::byte*) resource->Data.data()+pos, need);
            pos += need;
        }
        
    }
}

void RemoteClient::informateIdToHash(const std::unordered_map<ResourceId_t, ResourceFile::Hash_t>* resourcesLink) {
    std::vector<std::tuple<EnumBinResource, ResourceId_t, Hash_t>> newForClient;

    for(int type = 0; type < (int) EnumBinResource::MAX_ENUM; type++) {
        for(auto& [id, hash] : resourcesLink[type]) {
            // Посмотрим что известно клиенту
            auto iter = ResUses.BinUse[uint8_t(type)].find(id);
            if(iter != ResUses.BinUse[uint8_t(type)].end()) {
                if(std::get<1>(iter->second) != hash) {
                    // Требуется перепривязать идентификатор к новому хешу
                    newForClient.push_back({(EnumBinResource) type, id, hash});
                    std::get<1>(iter->second) = hash;
                    // Проверить есть ли хеш на стороне клиента
                    if(!std::binary_search(ClientBinaryCache.begin(), ClientBinaryCache.end(), hash)) {
                        NeedToSend.push_back(hash);
                        NextRequest.Hashes.push_back(hash);
                    }
                }
            } else {
                // Ресурс не отслеживается клиентом
            }
        }
    }

    // Отправляем новые привязки ресурсов
    if(!newForClient.empty()) {
        assert(newForClient.size() < 65535*4);

        checkPacketBorder(2+4+newForClient.size()*(1+4+32));
        NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
            << ((uint8_t) ToClient::L2Resource::Bind) << uint32_t(newForClient.size());

        for(auto& [type, id, hash] : newForClient) {
            NextPacket << uint8_t(type) << uint32_t(id);
            NextPacket.write((const std::byte*) hash.data(), hash.size());
        }
    }
}

void RemoteClient::informateDefVoxel(const std::unordered_map<DefVoxelId, DefVoxel_t*> &voxels)
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

void RemoteClient::informateDefNode(const std::unordered_map<DefNodeId, DefNode_t*> &nodes)
{
    for(auto& [id, def] : nodes) {
        if(!ResUses.DefNode.contains(id))
            continue;
        
        size_t reserve = 0;
        for(int iter = 0; iter < 6; iter++)
            reserve += def->Texs[iter].Pipeline.size();

        checkPacketBorder(1+1+4+1+2*6+reserve);
        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Node
            << id << (uint8_t) def->DrawType;

        for(int iter = 0; iter < 6; iter++) {
            NextPacket << (uint16_t) def->Texs[iter].Pipeline.size();
            NextPacket.write((const std::byte*) def->Texs[iter].Pipeline.data(), def->Texs[iter].Pipeline.size());
        }

        ResUsesObj::RefDefBin_t refs;
        {
            auto &array = refs.Resources[(uint8_t) EnumBinResource::Texture];
            for(int iter = 0; iter < 6; iter++) {
                array.insert(array.end(), def->Texs[iter].BinTextures.begin(), def->Texs[iter].BinTextures.end());
            }

            std::sort(array.begin(), array.end());
            auto eraseLast = std::unique(array.begin(), array.end());
            array.erase(eraseLast, array.end());

            incrementBinary(refs);
        }

        
        {
            auto iterDefRef = ResUses.RefDefNode.find(id);
            if(iterDefRef != ResUses.RefDefNode.end()) {
                decrementBinary(std::move(iterDefRef->second));
                iterDefRef->second = std::move(refs);
            } else {
                ResUses.RefDefNode[id] = std::move(refs);
            }
        }

    }
}

void RemoteClient::informateDefWorld(const std::unordered_map<DefWorldId_t, DefWorld_t*> &worlds)
{
    for(auto pair : worlds) {
        DefWorldId_t id = pair.first;
        if(!ResUses.DefWorld.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::World
            << id;
    }
}

void RemoteClient::informateDefPortal(const std::unordered_map<DefPortalId_t, DefPortal_t*> &portals)
{
    for(auto pair : portals) {
        DefPortalId_t id = pair.first;
        if(!ResUses.DefPortal.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Portal
            << id;
    }
}

void RemoteClient::informateDefEntity(const std::unordered_map<DefEntityId_t, DefEntity_t*> &entityes)
{
    for(auto pair : entityes) {
        DefEntityId_t id = pair.first;
        if(!ResUses.DefEntity.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Entity
            << id;
    }
}

void RemoteClient::informateDefItem(const std::unordered_map<DefItemId_t, DefItem_t*> &items)
{
    for(auto pair : items) {
        DefItemId_t id = pair.first;
        if(!ResUses.DefNode.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::FuncEntity
            << id;
    }
}

void RemoteClient::checkPacketBorder(uint16_t size) {
    if(64000-NextPacket.size() < size || (NextPacket.size() != 0 && size == 0)) {
        SimplePackets.push_back(std::move(NextPacket));
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

void RemoteClient::incrementBinary(const ResUsesObj::RefDefBin_t& bin) {
    for(int iter = 0; iter < 5; iter++) {
        auto &use = ResUses.BinUse[iter];

        for(ResourceId_t id : bin.Resources[iter]) {
            if(++std::get<0>(use[id]) == 1) {
                NextRequest.BinToHash[iter].push_back(id);
                LOG.debug() << "Новое определение (тип " << iter << ") -> " << id;
            }
        }
    }
}

void RemoteClient::decrementBinary(ResUsesObj::RefDefBin_t&& bin) {
    std::vector<std::tuple<EnumBinResource, ResourceId_t>> lost;

    for(int iter = 0; iter < 5; iter++) {
        auto &use = ResUses.BinUse[iter];

        for(ResourceId_t id : bin.Resources[iter]) {
            if(--std::get<0>(use[id]) == 0) {
                use.erase(use.find(id));

                lost.push_back({(EnumBinResource) iter, id});
                LOG.debug() << "Потеряно определение (тип " << iter << ") -> " << id;
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
            NextPacket << uint8_t(type) << uint32_t(id);
    }
}

}