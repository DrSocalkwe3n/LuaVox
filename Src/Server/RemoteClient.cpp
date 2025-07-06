#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include <algorithm>
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

bool RemoteClient::maybe_prepareChunkUpdate_Voxels(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_voxels,
    const std::vector<DefVoxelId_t>& uniq_sorted_defines)
{
    bool lock = ResUses.RefChunkLock.exchange(1);
    if(lock)
        return false;

    /*
        Обновить зависимости
        Запросить недостающие
        Отправить всё клиенту
    */

    std::vector<DefVoxelId_t>
        newTypes, /* Новые типы вокселей */
        lostTypes /* Потерянные типы вокселей */;

    // Отметим использование этих вокселей
    for(const DefVoxelId_t& id : uniq_sorted_defines) {
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
    Pos::bvec4u lChunk = (chunkPos & 0xf);

    if(iterWorld != ResUses.RefChunk.end())
    // Исключим зависимости предыдущей версии чанка
    {
        auto iterRegion = iterWorld->second.find(chunkPos);
        if(iterRegion != iterWorld->second.end()) {
            // Уменьшим счётчик зависимостей
            for(const DefVoxelId_t& id : iterRegion->second[lChunk.pack()].Voxel) {
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

    iterWorld->second[chunkPos][lChunk.pack()].Voxel = uniq_sorted_defines;

    if(!newTypes.empty()) {
        // Добавляем новые типы в запрос
        NextRequest.Voxel.insert(NextRequest.Voxel.end(), newTypes.begin(), newTypes.end());
    }

    if(!lostTypes.empty()) {
        for(const DefVoxelId_t& id : lostTypes) {
            auto iter = ResUses.RefDefVoxel.find(id);
            assert(iter != ResUses.RefDefVoxel.end()); // Должны быть описаны зависимости вокселя
            decrementBinary(std::move(iter->second.Texture), {}, std::move(iter->second.Sound), {}, {});
            ResUses.RefDefVoxel.erase(iter);
        }
    }

    checkPacketBorder(1+4+8+2+4+compressed_voxels.size());
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkVoxels
        << worldId << chunkPos.pack() << uint32_t(compressed_voxels.size());
    NextPacket.write((const std::byte*) compressed_voxels.data(), compressed_voxels.size());

    ResUses.RefChunkLock.exchange(0);
    return true;
}

bool RemoteClient::maybe_prepareChunkUpdate_Nodes(WorldId_t worldId, Pos::GlobalChunk chunkPos, const std::u8string& compressed_nodes,
    const std::vector<DefNodeId_t>& uniq_sorted_defines)
{
    bool lock = ResUses.RefChunkLock.exchange(1);
    if(lock)
        return false;

    std::vector<DefNodeId_t>
        newTypes, /* Новые типы нод */
        lostTypes /* Потерянные типы нод */;

    // Отметим использование этих нод
    for(const DefNodeId_t& id : uniq_sorted_defines) {
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
    Pos::bvec4u lChunk = (chunkPos & 0xf);

    if(iterWorld != ResUses.RefChunk.end())
    // Исключим зависимости предыдущей версии чанка
    {
        auto iterRegion = iterWorld->second.find(chunkPos);
        if(iterRegion != iterWorld->second.end()) {
            // Уменьшим счётчик зависимостей
            for(const DefNodeId_t& id : iterRegion->second[lChunk.pack()].Node) {
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

    iterWorld->second[chunkPos][lChunk.pack()].Node = uniq_sorted_defines;

    if(!newTypes.empty()) {
        // Добавляем новые типы в запрос
        NextRequest.Node.insert(NextRequest.Node.end(), newTypes.begin(), newTypes.end());
    }

    if(!lostTypes.empty()) {
        for(const DefNodeId_t& id : lostTypes) {
            auto iter = ResUses.RefDefNode.find(id);
            assert(iter != ResUses.RefDefNode.end()); // Должны быть описаны зависимости ноды
            decrementBinary({}, {}, std::move(iter->second.Sound), std::move(iter->second.Model), {});
            ResUses.RefDefNode.erase(iter);
        }
    }

    checkPacketBorder(1+4+8+4+compressed_nodes.size());
    NextPacket << (uint8_t) ToClient::L1::Content
        << (uint8_t) ToClient::L2Content::ChunkNodes
        << worldId << chunkPos.pack() << uint32_t(compressed_nodes.size());
    NextPacket.write((const std::byte*) compressed_nodes.data(), compressed_nodes.size());

    ResUses.RefChunkLock.exchange(0);
    return true;
}

void RemoteClient::prepareRegionRemove(WorldId_t worldId, Pos::GlobalRegion regionPos) {
    std::vector<DefVoxelId_t>
        lostTypesV /* Потерянные типы вокселей */;
    std::vector<DefNodeId_t>
        lostTypesN /* Потерянные типы нод */;

    // Уменьшаем зависимости вокселей и нод
    {
        auto iterWorld = ResUses.RefChunk.find(worldId);
        assert(iterWorld != ResUses.RefChunk.end());

        auto iterRegion = iterWorld->second.find(regionPos);
        assert(iterRegion != iterWorld->second.end());
        
        for(const auto &iterChunk : iterRegion->second) {
            for(const DefVoxelId_t& id : iterChunk.Voxel) {
                auto iter = ResUses.DefVoxel.find(id);
                assert(iter != ResUses.DefVoxel.end()); // Воксель должен быть в зависимостях
                if(--iter->second == 0) {
                    // Вокселя больше нет в зависимостях
                    lostTypesV.push_back(id);
                    ResUses.DefVoxel.erase(iter);
                }
            }

            for(const DefNodeId_t& id : iterChunk.Node) {
                auto iter = ResUses.DefNode.find(id);
                assert(iter != ResUses.DefNode.end()); // Нода должна быть в зависимостях
                if(--iter->second == 0) {
                    // Ноды больше нет в зависимостях
                    lostTypesN.push_back(id);
                    ResUses.DefNode.erase(iter);
                }
            }
        }
    }

    if(!lostTypesV.empty()) {
        for(const DefVoxelId_t& id : lostTypesV) {
            auto iter = ResUses.RefDefVoxel.find(id);
            assert(iter != ResUses.RefDefVoxel.end()); // Должны быть описаны зависимости вокселя
            decrementBinary(std::move(iter->second.Texture), {}, std::move(iter->second.Sound), {}, {});
            ResUses.RefDefVoxel.erase(iter);
        }
    }

    if(!lostTypesN.empty()) {
        for(const DefNodeId_t& id : lostTypesN) {
            auto iter = ResUses.RefDefNode.find(id);
            assert(iter != ResUses.RefDefNode.end()); // Должны быть описаны зависимости ноды
            decrementBinary({}, {}, std::move(iter->second.Sound), std::move(iter->second.Model), {});
            ResUses.RefDefNode.erase(iter);
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
                decrementBinary(std::move(iterProfileRef->second.Texture), std::move(iterProfileRef->second.Animation), {}, 
                    std::move(iterProfileRef->second.Model), {});
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

            decrementBinary(std::move(iterProfileRef->second.Texture), std::move(iterProfileRef->second.Animation), {}, std::move(iterProfileRef->second.Model), {});
        
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
                decrementBinary(std::move(iterWorldProfRef->second.Texture), {}, {}, std::move(iterWorldProfRef->second.Model), {});
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
        decrementBinary(std::move(iterWorldProfDef->second.Texture), {}, {}, std::move(iterWorldProfDef->second.Model), {});
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

void RemoteClient::informateBin(ToClient::L2Resource type, ResourceId_t id, const std::shared_ptr<ResourceFile>& data) {
    checkPacketBorder(0);
    NextPacket << (uint8_t) ToClient::L1::Resource    // Оповещение
        << (uint8_t) type << id;
    for(auto part : data->Hash)
        NextPacket << part;

    NextPacket << (uint8_t) ToClient::L1::Resource    // Принудительная полная отправка
        << (uint8_t) ToClient::L2Resource::InitResSend
        << uint8_t(0) << uint8_t(0) << id
        << uint32_t(data->Data.size());
    for(auto part : data->Hash)
        NextPacket << part;

    NextPacket << uint8_t(0) << uint32_t(data->Data.size());

    size_t pos = 0;
    while(pos < data->Data.size()) {
        checkPacketBorder(0);
        size_t need = std::min(data->Data.size()-pos, std::min<size_t>(NextPacket.size(), 64000));
        NextPacket.write((const std::byte*) data->Data.data()+pos, need);
        pos += need;
    }
}

void RemoteClient::informateBinTexture(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t id = pair.first;
        if(!ResUses.BinTexture.contains(id))
            continue; // Клиент не наблюдает за этим объектом

        informateBin(ToClient::L2Resource::Texture, id, pair.second);
    }
}

void RemoteClient::informateBinAnimation(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t id = pair.first;
        if(!ResUses.BinTexture.contains(id))
            continue; // Клиент не наблюдает за этим объектом

        informateBin(ToClient::L2Resource::Animation, id, pair.second);
    }
}

void RemoteClient::informateBinModel(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t id = pair.first;
        if(!ResUses.BinTexture.contains(id))
            continue; // Клиент не наблюдает за этим объектом

        informateBin(ToClient::L2Resource::Model, id, pair.second);
    }
}

void RemoteClient::informateBinSound(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t id = pair.first;
        if(!ResUses.BinTexture.contains(id))
            continue; // Клиент не наблюдает за этим объектом

        informateBin(ToClient::L2Resource::Sound, id, pair.second);
    }
}

void RemoteClient::informateBinFont(const std::unordered_map<BinTextureId_t, std::shared_ptr<ResourceFile>> &textures)
{
    for(auto pair : textures) {
        BinTextureId_t id = pair.first;
        if(!ResUses.BinTexture.contains(id))
            continue; // Клиент не наблюдает за этим объектом

        informateBin(ToClient::L2Resource::Font, id, pair.second);
    }
}

void RemoteClient::informateDefVoxel(const std::unordered_map<DefVoxelId_t, void*> &voxels)
{
    for(auto pair : voxels) {
        DefVoxelId_t id = pair.first;
        if(!ResUses.DefVoxel.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Voxel
            << id;
    }
}

void RemoteClient::informateDefNode(const std::unordered_map<DefNodeId_t, void*> &nodes)
{
    for(auto pair : nodes) {
        DefNodeId_t id = pair.first;
        if(!ResUses.DefNode.contains(id))
            continue;

        NextPacket << (uint8_t) ToClient::L1::Definition
            << (uint8_t) ToClient::L2Definition::Node
            << id;
    }
}

void RemoteClient::informateDefWorld(const std::unordered_map<DefWorldId_t, void*> &worlds)
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

void RemoteClient::informateDefPortal(const std::unordered_map<DefPortalId_t, void*> &portals)
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

void RemoteClient::informateDefEntity(const std::unordered_map<DefEntityId_t, void*> &entityes)
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

void RemoteClient::informateDefItem(const std::unordered_map<DefItemId_t, void*> &items)
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
        CameraPos.x = co_await sock.read<decltype(CameraPos.x)>();
        CameraPos.y = co_await sock.read<decltype(CameraPos.x)>();
        CameraPos.z = co_await sock.read<decltype(CameraPos.x)>();

        for(int iter = 0; iter < 5; iter++)
            CameraQuat.Data[iter] = co_await sock.read<uint8_t>();

        co_return;
    }
    default:
        protocolError();
    }
}

void RemoteClient::incrementBinary(const std::vector<BinTextureId_t>& textures, const std::vector<BinAnimationId_t>& animation,
    const std::vector<BinSoundId_t>& sounds, const std::vector<BinModelId_t>& models,
    const std::vector<BinFontId_t>& fonts
) {
    for(BinTextureId_t id : textures) {
        if(++ResUses.BinTexture[id] == 1) {
            NextRequest.BinTexture.push_back(id);
            LOG.debug() << "Новое определение текстуры: " << id;
        }
    }

    for(BinAnimationId_t id : animation) {
        if(++ResUses.BinAnimation[id] == 1) {
            NextRequest.BinAnimation.push_back(id);
            LOG.debug() << "Новое определение анимации: " << id;
        }
    }

    for(BinSoundId_t id : sounds) {
        if(++ResUses.BinSound[id] == 1) {
            NextRequest.BinSound.push_back(id);
            LOG.debug() << "Новое определение звука: " << id;
        }
    }

    for(BinModelId_t id : models) {
        if(++ResUses.BinModel[id] == 1) {
            NextRequest.BinModel.push_back(id);
            LOG.debug() << "Новое определение модели: " << id;
        }
    }

    for(BinFontId_t id : fonts) {
        if(++ResUses.BinFont[id] == 1) {
            NextRequest.BinFont.push_back(id);
            LOG.debug() << "Новое определение шрифта: " << id;
        }
    }
}

void RemoteClient::decrementBinary(std::vector<BinTextureId_t>&& textures, std::vector<BinAnimationId_t>&& animation,
    std::vector<BinSoundId_t>&& sounds, std::vector<BinModelId_t>&& models,
    std::vector<BinFontId_t>&& fonts
) {
    for(BinTextureId_t id : textures) {
        if(--ResUses.BinTexture[id] == 0) {
            ResUses.BinTexture.erase(ResUses.BinTexture.find(id));
            LOG.debug() << "Потеряно определение текстуры: " << id;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeTexture
                << id;
        }
    }

    for(BinAnimationId_t id : animation) {
        if(--ResUses.BinAnimation[id] == 0) {
            ResUses.BinAnimation.erase(ResUses.BinAnimation.find(id));
            LOG.debug() << "Потеряно определение анимации: " << id;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeAnimation
                << id;
        }
    }

    for(BinSoundId_t id : sounds) {
        if(--ResUses.BinSound[id] == 0) {
            ResUses.BinSound.erase(ResUses.BinSound.find(id));
            LOG.debug() << "Потеряно определение звука: " << id;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeSound
                << id;
        }
    }

    for(BinModelId_t id : models) {
        if(--ResUses.BinModel[id] == 0) {
            ResUses.BinModel.erase(ResUses.BinModel.find(id));
            LOG.debug() << "Потеряно определение модели: " << id;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeModel
                << id;
        }
    }

    for(BinFontId_t id : fonts) {
        if(--ResUses.BinFont[id] == 0) {
            ResUses.BinFont.erase(ResUses.BinFont.find(id));
            LOG.debug() << "Потеряно определение шрифта: " << id;

            NextPacket << (uint8_t) ToClient::L1::Resource
                << (uint8_t) ToClient::L2Resource::FreeFont
                << id;
        }
    }
}

}