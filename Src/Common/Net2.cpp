#include "Net2.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>
#include <algorithm>
#include <tuple>

namespace LV::Net2 {

using namespace TOS;

namespace {

struct HeaderFields {
    uint32_t size = 0;
    uint16_t type = 0;
    Priority priority = Priority::Normal;
    FrameFlags flags = FrameFlags::None;
    uint32_t streamId = 0;
};

std::array<std::byte, AsyncSocket::kHeaderSize> encodeHeader(const HeaderFields &h) {
    std::array<std::byte, AsyncSocket::kHeaderSize> out{};
    uint32_t sizeNet = detail::toNetwork(h.size);
    uint16_t typeNet = detail::toNetwork(h.type);
    uint32_t streamNet = detail::toNetwork(h.streamId);

    std::memcpy(out.data(), &sizeNet, sizeof(sizeNet));
    std::memcpy(out.data() + 4, &typeNet, sizeof(typeNet));
    out[6] = std::byte(static_cast<uint8_t>(h.priority));
    out[7] = std::byte(static_cast<uint8_t>(h.flags));
    std::memcpy(out.data() + 8, &streamNet, sizeof(streamNet));
    return out;
}

HeaderFields decodeHeader(const std::array<std::byte, AsyncSocket::kHeaderSize> &in) {
    HeaderFields h{};
    std::memcpy(&h.size, in.data(), sizeof(h.size));
    std::memcpy(&h.type, in.data() + 4, sizeof(h.type));
    h.priority = static_cast<Priority>(std::to_integer<uint8_t>(in[6]));
    h.flags = static_cast<FrameFlags>(std::to_integer<uint8_t>(in[7]));
    std::memcpy(&h.streamId, in.data() + 8, sizeof(h.streamId));

    h.size = detail::fromNetwork(h.size);
    h.type = detail::fromNetwork(h.type);
    h.streamId = detail::fromNetwork(h.streamId);
    return h;
}

} // namespace

PacketWriter& PacketWriter::writeBytes(std::span<const std::byte> data) {
    Buffer.insert(Buffer.end(), data.begin(), data.end());
    return *this;
}

PacketWriter& PacketWriter::writeString(std::string_view str) {
    write<uint32_t>(static_cast<uint32_t>(str.size()));
    auto bytes = std::as_bytes(std::span<const char>(str.data(), str.size()));
    Buffer.insert(Buffer.end(), bytes.begin(), bytes.end());
    return *this;
}

std::vector<std::byte> PacketWriter::release() {
    std::vector<std::byte> out = std::move(Buffer);
    Buffer.clear();
    return out;
}

void PacketWriter::clear() {
    Buffer.clear();
}

PacketReader::PacketReader(std::span<const std::byte> data)
    : Data(data)
{
}

void PacketReader::readBytes(std::span<std::byte> out) {
    require(out.size());
    std::memcpy(out.data(), Data.data() + Pos, out.size());
    Pos += out.size();
}

std::string PacketReader::readString() {
    uint32_t size = read<uint32_t>();
    require(size);
    std::string out(size, '\0');
    std::memcpy(out.data(), Data.data() + Pos, size);
    Pos += size;
    return out;
}

void PacketReader::require(size_t size) {
    if(Data.size() - Pos < size)
        MAKE_ERROR("Net2::PacketReader: not enough data");
}

SocketServer::SocketServer(asio::io_context &ioc, std::function<coro<>(tcp::socket)> &&onConnect, uint16_t port)
    : AsyncObject(ioc), Acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
    assert(onConnect);
    co_spawn(run(std::move(onConnect)));
}

bool SocketServer::isStopped() const {
    return !Acceptor.is_open();
}

uint16_t SocketServer::getPort() const {
    return Acceptor.local_endpoint().port();
}

coro<void> SocketServer::run(std::function<coro<>(tcp::socket)> onConnect) {
    while(true) {
        try {
            co_spawn(onConnect(co_await Acceptor.async_accept()));
        } catch(const std::exception &exc) {
            if(const boost::system::system_error *errc = dynamic_cast<const boost::system::system_error*>(&exc);
                    errc && (errc->code() == asio::error::operation_aborted || errc->code() == asio::error::bad_descriptor))
                break;
        }
    }
}

AsyncSocket::SendQueue::SendQueue(asio::io_context &ioc)
    : semaphore(ioc)
{
    semaphore.expires_at(std::chrono::steady_clock::time_point::max());
}

bool AsyncSocket::SendQueue::empty() const {
    for(const auto &queue : queues) {
        if(!queue.empty())
            return false;
    }
    return true;
}

AsyncSocket::AsyncSocket(asio::io_context &ioc, tcp::socket &&socket, Limits limits)
    : AsyncObject(ioc), LimitsCfg(limits), Socket(std::move(socket)), Outgoing(ioc)
{
    Context = std::make_shared<AsyncContext>();

    boost::asio::socket_base::linger optionLinger(true, 4);
    Socket.set_option(optionLinger);
    boost::asio::ip::tcp::no_delay optionNoDelay(true);
    Socket.set_option(optionNoDelay);

    co_spawn(sendLoop());
}

AsyncSocket::~AsyncSocket() {
    if(Context)
        Context->needShutdown.store(true);

    {
        boost::lock_guard lock(Outgoing.mtx);
        Outgoing.semaphore.cancel();
        WorkDeadline.cancel();
    }

    if(Socket.is_open())
        try { Socket.close(); } catch(...) {}
}

void AsyncSocket::enqueue(OutgoingMessage &&msg) {
    if(msg.payload.size() > LimitsCfg.maxMessageSize) {
        setError("Net2::AsyncSocket: message too large");
        close();
        return;
    }

    boost::unique_lock lock(Outgoing.mtx);
    const size_t msgSize = msg.payload.size();
    const size_t lowIndex = static_cast<size_t>(Priority::Low);

    if(msg.priority == Priority::Low) {
        while(Outgoing.bytesInLow + msgSize > LimitsCfg.maxLowPriorityBytes && !Outgoing.queues[lowIndex].empty()) {
            Outgoing.bytesInQueue -= Outgoing.queues[lowIndex].front().payload.size();
            Outgoing.bytesInLow -= Outgoing.queues[lowIndex].front().payload.size();
            Outgoing.queues[lowIndex].pop_front();
        }
        if(Outgoing.bytesInLow + msgSize > LimitsCfg.maxLowPriorityBytes) {
            return;
        }
    }

    if(Outgoing.bytesInQueue + msgSize > LimitsCfg.maxQueueBytes) {
        dropLow(msgSize);
        if(Outgoing.bytesInQueue + msgSize > LimitsCfg.maxQueueBytes) {
            if(msg.dropIfOverloaded)
                return;
            setError("Net2::AsyncSocket: send queue overflow");
            close();
            return;
        }
    }

    const size_t idx = static_cast<size_t>(msg.priority);
    Outgoing.bytesInQueue += msgSize;
    if(msg.priority == Priority::Low)
        Outgoing.bytesInLow += msgSize;
    Outgoing.queues[idx].push_back(std::move(msg));

    if(Outgoing.waiting) {
        Outgoing.waiting = false;
        Outgoing.semaphore.cancel();
        Outgoing.semaphore.expires_at(std::chrono::steady_clock::time_point::max());
    }
}

coro<IncomingMessage> AsyncSocket::readMessage() {
    while(true) {
        std::array<std::byte, kHeaderSize> headerBytes{};
        co_await readExact(headerBytes.data(), headerBytes.size());
        HeaderFields header = decodeHeader(headerBytes);

        if(header.size > LimitsCfg.maxFrameSize)
            MAKE_ERROR("Net2::AsyncSocket: frame too large");

        std::vector<std::byte> chunk(header.size);
        if(header.size)
            co_await readExact(chunk.data(), chunk.size());

        if(header.streamId != 0) {
            if(Fragments.size() >= LimitsCfg.maxOpenStreams && !Fragments.contains(header.streamId))
                MAKE_ERROR("Net2::AsyncSocket: too many open streams");

            FragmentState &state = Fragments[header.streamId];
            if(state.data.empty()) {
                state.type = header.type;
                state.priority = header.priority;
            }

            if(state.data.size() + chunk.size() > LimitsCfg.maxMessageSize)
                MAKE_ERROR("Net2::AsyncSocket: reassembled message too large");

            state.data.insert(state.data.end(), chunk.begin(), chunk.end());

            if(!hasFlag(header.flags, FrameFlags::HasMore)) {
                IncomingMessage msg{state.type, state.priority, std::move(state.data)};
                Fragments.erase(header.streamId);
                co_return msg;
            }

            continue;
        }

        if(hasFlag(header.flags, FrameFlags::HasMore))
            MAKE_ERROR("Net2::AsyncSocket: stream id missing for fragmented frame");

        IncomingMessage msg{header.type, header.priority, std::move(chunk)};
        co_return msg;
    }
}

coro<> AsyncSocket::readLoop(std::function<coro<>(IncomingMessage&&)> onMessage) {
    while(isAlive()) {
        IncomingMessage msg = co_await readMessage();
        co_await onMessage(std::move(msg));
    }
}

void AsyncSocket::closeRead() {
    if(Socket.is_open() && !Context->readClosed.exchange(true)) {
        try { Socket.shutdown(boost::asio::socket_base::shutdown_receive); } catch(...) {}
    }
}

void AsyncSocket::close() {
    if(Context)
        Context->needShutdown.store(true);
    if(Socket.is_open())
        try { Socket.close(); } catch(...) {}
}

bool AsyncSocket::isAlive() const {
    return Context && !Context->needShutdown.load() && !Context->senderStopped.load() && Socket.is_open();
}

std::string AsyncSocket::getError() const {
    boost::lock_guard lock(Context->errorMtx);
    return Context->error;
}

coro<> AsyncSocket::sendLoop() {
    try {
        while(!Context->needShutdown.load()) {
            OutgoingMessage msg;
            {
                boost::unique_lock lock(Outgoing.mtx);
                if(Outgoing.empty()) {
                    Outgoing.waiting = true;
                    auto coroutine = Outgoing.semaphore.async_wait();
                    lock.unlock();
                    try { co_await std::move(coroutine); } catch(...) {}
                    continue;
                }

                if(!popNext(msg))
                    continue;
            }

            co_await sendMessage(std::move(msg));
        }
    } catch(const std::exception &exc) {
        setError(exc.what());
    } catch(...) {
        setError("Net2::AsyncSocket: send loop stopped");
    }

    Context->senderStopped.store(true);
}

coro<> AsyncSocket::sendMessage(OutgoingMessage &&msg) {
    const size_t total = msg.payload.size();
    if(total <= LimitsCfg.maxFrameSize) {
        co_await sendFrame(msg.type, msg.priority, FrameFlags::None, 0, msg.payload);
        co_return;
    }

    if(!msg.allowFragment) {
        setError("Net2::AsyncSocket: message requires fragmentation");
        close();
        co_return;
    }

    uint32_t streamId = NextStreamId++;
    if(streamId == 0)
        streamId = NextStreamId++;

    size_t offset = 0;
    while(offset < total) {
        const size_t chunk = std::min(LimitsCfg.maxFrameSize, total - offset);
        const bool more = (offset + chunk) < total;
        FrameFlags flags = more ? FrameFlags::HasMore : FrameFlags::None;
        std::span<const std::byte> view(msg.payload.data() + offset, chunk);
        co_await sendFrame(msg.type, msg.priority, flags, streamId, view);
        offset += chunk;
    }
}

coro<> AsyncSocket::sendFrame(uint16_t type, Priority priority, FrameFlags flags, uint32_t streamId,
    std::span<const std::byte> payload) {
    HeaderFields header{
        .size = static_cast<uint32_t>(payload.size()),
        .type = type,
        .priority = priority,
        .flags = flags,
        .streamId = streamId
    };
    auto headerBytes = encodeHeader(header);
    std::array<asio::const_buffer, 2> buffers{
        asio::buffer(headerBytes),
        asio::buffer(payload.data(), payload.size())
    };
    if(payload.empty())
        co_await asio::async_write(Socket, asio::buffer(headerBytes));
    else
        co_await asio::async_write(Socket, buffers);
}

coro<> AsyncSocket::readExact(std::byte *data, size_t size) {
    if(size == 0)
        co_return;
    co_await asio::async_read(Socket, asio::buffer(data, size));
}

bool AsyncSocket::popNext(OutgoingMessage &out) {
    static constexpr int kWeights[4] = {8, 4, 2, 1};

    for(int attempt = 0; attempt < 4; ++attempt) {
        const uint8_t idx = static_cast<uint8_t>((Outgoing.nextIndex + attempt) % 4);
        auto &queue = Outgoing.queues[idx];
        if(queue.empty())
            continue;

        if(Outgoing.credits[idx] <= 0)
            Outgoing.credits[idx] = kWeights[idx];

        if(Outgoing.credits[idx] <= 0)
            continue;

        out = std::move(queue.front());
        queue.pop_front();
        Outgoing.credits[idx]--;
        Outgoing.nextIndex = idx;

        const size_t msgSize = out.payload.size();
        Outgoing.bytesInQueue -= msgSize;
        if(idx == static_cast<uint8_t>(Priority::Low))
            Outgoing.bytesInLow -= msgSize;
        return true;
    }

    for(int i = 0; i < 4; ++i)
        Outgoing.credits[i] = kWeights[i];
    return false;
}

void AsyncSocket::dropLow(size_t needBytes) {
    const size_t lowIndex = static_cast<size_t>(Priority::Low);
    while(Outgoing.bytesInQueue + needBytes > LimitsCfg.maxQueueBytes && !Outgoing.queues[lowIndex].empty()) {
        const size_t size = Outgoing.queues[lowIndex].front().payload.size();
        Outgoing.bytesInQueue -= size;
        Outgoing.bytesInLow -= size;
        Outgoing.queues[lowIndex].pop_front();
    }
}

void AsyncSocket::setError(const std::string &msg) {
    if(!Context)
        return;
    boost::lock_guard lock(Context->errorMtx);
    Context->error = msg;
}

coro<tcp::socket> asyncConnectTo(const std::string &address,
    std::function<void(const std::string&)> onProgress) {
    std::string progress;
    auto addLog = [&](const std::string &msg) {
        progress += '\n';
        progress += msg;
        if(onProgress)
            onProgress('\n' + msg);
    };

    auto ioc = co_await asio::this_coro::executor;

    addLog("Parsing address " + address);
    auto re = Str::match(address, "((?:\\[[\\d\\w:]+\\])|(?:[\\d\\.]+))(?:\\:(\\d+))?");

    std::vector<std::tuple<tcp::endpoint, std::string>> eps;

    if(!re) {
        re = Str::match(address, "([-_\\.\\w\\d]+)(?:\\:(\\d+))?");
        if(!re)
            MAKE_ERROR("Failed to parse address");

        tcp::resolver resv{ioc};
        tcp::resolver::results_type result;

        addLog("Resolving name...");
        result = co_await resv.async_resolve(*re->at(1), re->at(2) ? *re->at(2) : "7890");

        addLog("Got " + std::to_string(result.size()) + " endpoints");
        for(auto iter : result) {
            std::string addr = iter.endpoint().address().to_string() + ':' + std::to_string(iter.endpoint().port());
            std::string hostname = iter.host_name();
            if(hostname == addr)
                addLog("ep: " + addr);
            else
                addLog("ep: " + hostname + " (" + addr + ')');

            eps.emplace_back(iter.endpoint(), iter.host_name());
        }
    } else {
        eps.emplace_back(tcp::endpoint{asio::ip::make_address(*re->at(1)),
            static_cast<uint16_t>(re->at(2) ? Str::toVal<int>(*re->at(2)) : 7890)},
            *re->at(1));
    }

    for(auto [ep, hostname] : eps) {
        addLog("Connecting to " + hostname + " (" + ep.address().to_string() + ':'
            + std::to_string(ep.port()) + ")");
        try {
            tcp::socket sock{ioc};
            co_await sock.async_connect(ep);
            addLog("Connected");
            co_return sock;
        } catch(const std::exception &exc) {
            addLog(std::string("Connect failed: ") + exc.what());
        }
    }

    MAKE_ERROR("Unable to connect to server");
}

} // namespace LV::Net2
