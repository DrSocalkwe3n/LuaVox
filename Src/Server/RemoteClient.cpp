#include <TOSLib.hpp>
#include "RemoteClient.hpp"
#include "Common/Abstract.hpp"
#include "Common/Net.hpp"
#include "Server/Abstract.hpp"
#include "Server/GameServer.hpp"
#include "Server/World.hpp"
#include <algorithm>
#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
#include <Common/Packets.hpp>


namespace LV::Server {

Net::Packet RemoteClient::makePacket_informateAssets_DK(
    const std::array<
        std::vector<AssetsPreloader::BindDomainKeyInfo>, 
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    >& dkVector
) {
    Net::Packet pack;

    // Сжатие по дедубликации доменов
    std::unordered_map<std::string, uint16_t> domainsToId;

    {
        std::unordered_set<std::string> domains;

        for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); type++) {
            for(const auto& bind : dkVector[type]) {
                domains.insert(bind.Domain);
            }
        }

        pack << uint16_t(domains.size());

        int counter = 0;
        for(const std::string& domain : domains) {
            pack << domain;
            domainsToId[domain] = counter++;
        }
    }

    // Запись связок домен+ключ
    for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); type++) {
        const std::vector<AssetsPreloader::BindDomainKeyInfo>& binds = dkVector[type];
        pack << uint32_t(binds.size());

        for(const auto& bind : binds) {
            auto iter = domainsToId.find(bind.Domain);
            assert(iter != domainsToId.end());

            pack << iter->second << bind.Key;
        }
    }

    // Сжатие
    std::u8string compressed = compressLinear(pack.complite());
    pack << uint8_t(ToClient::AssetsBindDK) << (const std::string&) compressed;

    return pack;
}

Net::Packet RemoteClient::makePacket_informateAssets_HH(
    const std::array< 
        std::vector<AssetsPreloader::BindHashHeaderInfo>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    >& hhVector,
    const std::array<
        std::vector<ResourceId>,
        static_cast<size_t>(EnumAssets::MAX_ENUM)
    >& lost
) {
    Net::Packet pack;
    pack << uint8_t(ToClient::AssetsBindHH);

    // Запись связок hash+header
    for(size_t type = 0; type < static_cast<size_t>(EnumAssets::MAX_ENUM); type++) {
        const std::vector<AssetsPreloader::BindHashHeaderInfo>& binds = hhVector[type];
        pack << uint32_t(binds.size());

        for(const auto& bind : binds) {
            pack << bind.Id;
            pack.write((const std::byte*) bind.Hash.data(), bind.Hash.size());
            pack << (const std::string&) bind.Header;
        }
    }

    return pack;
}

std::vector<Net::Packet> RemoteClient::makePackets_sendDefContentUpdate(
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
) {
    std::vector<Net::Packet> packets;
    Net::Packet pack;

    auto check = [&](size_t needSize) {
        if(pack.size()+needSize > 65500) {
            packets.emplace_back(std::move(pack));
            pack.clear();
        }
    };

    pack << (uint8_t) ToClient::DefinitionsUpdate;
    pack << uint32_t(newOrUpdate.size());
    for(size_t type = 0; type < static_cast<size_t>(EnumDefContent::MAX_ENUM); type++) {
        pack << uint32_t(newOrUpdate[type].size());

        for(const auto& [id, data] : newOrUpdate[type]) {
            check(data.size());
            pack << id << (const std::string&) data;
        }
    }

    pack << uint32_t(lost.size());
    for(size_t type = 0; type < static_cast<size_t>(EnumDefContent::MAX_ENUM); type++) {
        pack << uint32_t(lost[type].size());
        
        for(ResourceId id : lost[type]) {
            check(4);
            pack << id;
        }
    }

    pack << uint32_t(idToDK.size());
    for(size_t type = 0; type < static_cast<size_t>(EnumDefContent::MAX_ENUM); type++) {
        pack << uint32_t(idToDK[type].size());
        
        for(const auto& [domain, key] : idToDK[type]) {
            check(domain.size() + key.size() + 8);
            pack << key << domain;
        }
    }

    if(pack.size())
        packets.emplace_back(std::move(pack));

    return packets;
}

namespace {

const char* assetTypeName(EnumAssets type) {
    switch(type) {
    case EnumAssets::Nodestate: return "nodestate";
    case EnumAssets::Model: return "model";
    case EnumAssets::Texture: return "texture";
    case EnumAssets::Particle: return "particle";
    case EnumAssets::Animation: return "animation";
    case EnumAssets::Sound: return "sound";
    case EnumAssets::Font: return "font";
    default: return "unknown";
    }
}

}

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
    packet << (uint8_t) ToClient::Disconnect
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


// void RemoteClient::prepareChunkUpdate_Voxels(
//     WorldId_t worldId,
//     Pos::GlobalChunk chunkPos,
//     const std::u8string& compressed_voxels
// ) {
//     Pos::bvec4u localChunk = chunkPos & 0x3;
//     Pos::GlobalRegion regionPos = chunkPos >> 2;

//     packet << (uint8_t) ToClient::ChunkVoxels
//         << worldId << chunkPos.pack() << uint32_t(compressed_voxels.size());
//     packet.write((const std::byte*) compressed_voxels.data(), compressed_voxels.size());
// }

// void RemoteClient::prepareChunkUpdate_Nodes(
//     WorldId_t worldId,
//     Pos::GlobalChunk chunkPos,
//     const std::u8string& compressed_nodes
// ) {
//     Pos::bvec4u localChunk = chunkPos & 0x3;
//     Pos::GlobalRegion regionPos = chunkPos >> 2;

//     packet << (uint8_t) ToClient::ChunkNodes
//         << worldId << chunkPos.pack() << uint32_t(compressed_nodes.size());
//     packet.write((const std::byte*) compressed_nodes.data(), compressed_nodes.size());
// }

void RemoteClient::NetworkAndResource_t::prepareRegionsRemove(WorldId_t worldId, std::vector<Pos::GlobalRegion> regionPoses)
{
    for(Pos::GlobalRegion regionPos : regionPoses) {
        checkPacketBorder(16);
        NextPacket << (uint8_t) ToClient::RemoveRegion
            << worldId << regionPos.pack();
    }
}

void RemoteClient::NetworkAndResource_t::prepareEntitiesUpdate(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities)
{
    // for(auto& [entityId, entity] : entities) {
    //     // Сопоставим с идентификатором клиента
    //     ClientEntityId_t ceId = ReMapEntities.toClient(entityId);

    //     checkPacketBorder(32);
    //     NextPacket << (uint8_t) ToClient::Entity
    //         << ceId
    //         << (uint32_t) entity->getDefId()
    //         << (uint32_t) entity->WorldId
    //         << entity->Pos.x
    //         << entity->Pos.y
    //         << entity->Pos.z;

    //     {
    //         ToServer::PacketQuat q;
    //         q.fromQuat(entity->Quat);
    //         for(int iter = 0; iter < 5; iter++)
    //             NextPacket << q.Data[iter];
    //     }
    // }
}

void RemoteClient::NetworkAndResource_t::prepareEntitiesUpdate_Dynamic(const std::vector<std::tuple<ServerEntityId_t, const Entity*>>& entities)
{
    prepareEntitiesUpdate(entities);
}

void RemoteClient::NetworkAndResource_t::prepareEntitySwap(ServerEntityId_t prev, ServerEntityId_t next)
{
    ReMapEntities.rebindClientKey(prev, next);
}

void RemoteClient::NetworkAndResource_t::prepareEntitiesRemove(const std::vector<ServerEntityId_t>& entityIds)
{
    // for(ServerEntityId_t entityId : entityIds) {
    //     ClientEntityId_t cId = ReMapEntities.erase(entityId);

    //     checkPacketBorder(16);
    //     NextPacket << (uint8_t) ToClient::L1::Content
    //         << (uint8_t) ToClient::L2Content::RemoveEntity
    //         << cId;
    // }
}

void RemoteClient::NetworkAndResource_t::prepareWorldUpdate(WorldId_t worldId, World* world)
{
    // TODO: отправить мир
}

void RemoteClient::NetworkAndResource_t::prepareWorldRemove(WorldId_t worldId)
{
}

// void RemoteClient::NetworkAndResource_t::preparePortalUpdate(PortalId portalId, void* portal) {}
// void RemoteClient::NetworkAndResource_t::preparePortalRemove(PortalId portalId) {}

void RemoteClient::prepareCameraSetEntity(ServerEntityId_t entityId) {
    auto lock = NetworkAndResource.lock();
    ClientEntityId_t cId = lock->ReMapEntities.toClient(entityId);
    lock->checkPacketBorder(8);
    lock->NextPacket << (uint8_t) ToClient::TestLinkCameraToEntity
        << cId;
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

    if(!AssetsInWork.AssetsPackets.empty()) {
        for(Net::Packet& packet : AssetsInWork.AssetsPackets)
            toSend.push_back(std::move(packet));
        AssetsInWork.AssetsPackets.clear();
    }
    if(AssetsInWork.AssetsPacket.size())
        toSend.push_back(std::move(AssetsInWork.AssetsPacket));

    {
        Net::Packet p;
        p << (uint8_t) ToClient::Tick;
        toSend.push_back(std::move(p));
    }

    Socket.pushPackets(&toSend);
    toSend.clear();

    nextRequest.uniq();

    return std::move(nextRequest);
}

void RemoteClient::informateBinaryAssets(const std::vector<AssetBinaryInfo>& resources)
{
    for(const AssetBinaryInfo& resource : resources) {
        auto lock = NetworkAndResource.lock();
        auto iter = std::find(lock->ClientRequested.begin(), lock->ClientRequested.end(), resource.Hash);
        if(iter == lock->ClientRequested.end())
            continue;

        lock->ClientRequested.erase(iter);
        lock.unlock();

        auto it = std::lower_bound(AssetsInWork.OnClient.begin(), AssetsInWork.OnClient.end(), resource.Hash);
        if(it == AssetsInWork.OnClient.end() || *it != resource.Hash) {
            AssetsInWork.OnClient.insert(it, resource.Hash);
            AssetsInWork.ToSend.emplace_back(resource.Data, 0);
        } else {
            LOG.warn() << "Клиент повторно запросил имеющийся у него ресурс";
        }
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
    case ToServer::L2System::ResourceRequest:
    {
        static std::atomic<uint32_t> debugRequestLogCount = 0;
        uint16_t count = co_await sock.read<uint16_t>();
        std::vector<Hash_t> hashes;
        hashes.reserve(count);

        for(int iter = 0; iter < count; iter++) {
            Hash_t hash;
            co_await sock.read((std::byte*) hash.data(), 32);
            hashes.push_back(hash);
        }

        auto lock = NetworkAndResource.lock();
        lock->NextRequest.Hashes.append_range(hashes);
        lock->ClientRequested.append_range(hashes);

        if(debugRequestLogCount.fetch_add(1) < 64) {
            if(!hashes.empty()) {
                const auto& h = hashes.front();
                LOG.debug() << "ResourceRequest count=" << count
                    << " first=" << int(h[0]) << '.'
                    << int(h[1]) << '.'
                    << int(h[2]) << '.'
                    << int(h[3]);
            } else {
                LOG.debug() << "ResourceRequest count=" << count;
            }
        }
        co_return;
    }
    case ToServer::L2System::ReloadMods:
    {
        if(Server) {
            Server->requestModsReload();
            LOG.info() << "Запрос на перезагрузку модов";
        } else {
            LOG.warn() << "Запрос на перезагрузку модов отклонён: сервер не назначен";
        }
        co_return;
    }
    default:
        protocolError();
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
        constexpr uint16_t kMaxAssetPacketSize = 64000;
        const size_t maxChunkPayload = std::max<size_t>(1, kMaxAssetPacketSize - 1 - 1 - 32 - 4);
        size_t chunkSize = std::max<size_t>(1'024'000 / toSend.size(), 4096);
        chunkSize = std::min(chunkSize, maxChunkPayload);
        static std::atomic<uint32_t> debugInitSendLogCount = 0;

        Net::Packet& p = AssetsInWork.AssetsPacket;

        auto flushAssetsPacket = [&]() {
            if(p.size() == 0)
                return;
            AssetsInWork.AssetsPackets.push_back(std::move(p));
        };

        bool hasFullSended = false;

        for(auto& [res, sended] : toSend) {
            if(sended == 0) {
                // Оповещаем о начале отправки ресурса
                const size_t initSize = 1 + 1 + 4 + 32 + 4 + 1;
                if(p.size() + initSize > kMaxAssetPacketSize)
                    flushAssetsPacket();
                p << (uint8_t) ToClient::AssetsInitSend
                    << uint32_t(res.size());
                p.write((const std::byte*) res.hash().data(), 32);
            }

            // Отправляем чанк
            size_t willSend = std::min(chunkSize, res.size()-sended);
            const size_t chunkMsgSize = 1 + 1 + 32 + 4 + willSend;
            if(p.size() + chunkMsgSize > kMaxAssetPacketSize)
                flushAssetsPacket();
            p << (uint8_t) ToClient::AssetsNextSend;
            p.write((const std::byte*) res.hash().data(), 32);
            p << uint32_t(willSend);
            p.write(res.data() + sended, willSend);
            sended += willSend;

            if(sended == res.size()) {
                hasFullSended = true;
            }
        }

        if(hasFullSended) {
            for(ssize_t iter = toSend.size()-1; iter >= 0; iter--) {
                if(std::get<0>(toSend[iter]).size() == std::get<1>(toSend[iter])) {
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
