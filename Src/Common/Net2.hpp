#pragma once

#include "Async.hpp"
#include "TOSLib.hpp"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <array>
#include <bit>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace LV::Net2 {

namespace detail {

constexpr bool kLittleEndian = (std::endian::native == std::endian::little);

template<typename T>
requires std::is_integral_v<T>
inline T toNetwork(T value) {
    if constexpr (kLittleEndian && sizeof(T) > 1)
        return std::byteswap(value);
    return value;
}

template<typename T>
requires std::is_floating_point_v<T>
inline T toNetwork(T value) {
    using U = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
    U u = std::bit_cast<U>(value);
    u = toNetwork(u);
    return std::bit_cast<T>(u);
}

template<typename T>
inline T fromNetwork(T value) {
    return toNetwork(value);
}

} // namespace detail

enum class Priority : uint8_t {
    Realtime = 0,
    High = 1,
    Normal = 2,
    Low = 3
};

enum class FrameFlags : uint8_t {
    None = 0,
    HasMore = 1
};

inline FrameFlags operator|(FrameFlags a, FrameFlags b) {
    return static_cast<FrameFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool hasFlag(FrameFlags value, FrameFlags flag) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

struct Limits {
    size_t maxFrameSize = 1 << 24;
    size_t maxMessageSize = 1 << 26;
    size_t maxQueueBytes = 1 << 27;
    size_t maxLowPriorityBytes = 1 << 26;
    size_t maxOpenStreams = 64;
};

struct OutgoingMessage {
    uint16_t type = 0;
    Priority priority = Priority::Normal;
    bool dropIfOverloaded = false;
    bool allowFragment = true;
    std::vector<std::byte> payload;
};

struct IncomingMessage {
    uint16_t type = 0;
    Priority priority = Priority::Normal;
    std::vector<std::byte> payload;
};

class PacketWriter {
public:
    PacketWriter& writeBytes(std::span<const std::byte> data);

    template<typename T>
    requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
    PacketWriter& write(T value) {
        T net = detail::toNetwork(value);
        std::array<std::byte, sizeof(T)> bytes{};
        std::memcpy(bytes.data(), &net, sizeof(T));
        Buffer.insert(Buffer.end(), bytes.begin(), bytes.end());
        return *this;
    }

    PacketWriter& writeString(std::string_view str);

    const std::vector<std::byte>& data() const { return Buffer; }
    std::vector<std::byte> release();
    void clear();

private:
    std::vector<std::byte> Buffer;
};

class PacketReader {
public:
    explicit PacketReader(std::span<const std::byte> data);

    template<typename T>
    requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
    T read() {
        require(sizeof(T));
        T net{};
        std::memcpy(&net, Data.data() + Pos, sizeof(T));
        Pos += sizeof(T);
        return detail::fromNetwork(net);
    }

    void readBytes(std::span<std::byte> out);
    std::string readString();
    bool empty() const { return Pos >= Data.size(); }
    size_t remaining() const { return Data.size() - Pos; }

private:
    void require(size_t size);

    size_t Pos = 0;
    std::span<const std::byte> Data;
};

class SocketServer : public AsyncObject {
public:
    SocketServer(asio::io_context &ioc, std::function<coro<>(tcp::socket)> &&onConnect, uint16_t port = 0);
    bool isStopped() const;
    uint16_t getPort() const;

private:
    coro<void> run(std::function<coro<>(tcp::socket)> onConnect);

    tcp::acceptor Acceptor;
};

class AsyncSocket : public AsyncObject {
public:
    static constexpr size_t kHeaderSize = 12;

    AsyncSocket(asio::io_context &ioc, tcp::socket &&socket, Limits limits = {});
    ~AsyncSocket();

    void enqueue(OutgoingMessage &&msg);
    coro<IncomingMessage> readMessage();
    coro<> readLoop(std::function<coro<>(IncomingMessage&&)> onMessage);

    void closeRead();
    void close();
    bool isAlive() const;
    std::string getError() const;

private:
    struct FragmentState {
        uint16_t type = 0;
        Priority priority = Priority::Normal;
        std::vector<std::byte> data;
    };

    struct AsyncContext {
        std::atomic_bool needShutdown{false};
        std::atomic_bool senderStopped{false};
        std::atomic_bool readClosed{false};
        boost::mutex errorMtx;
        std::string error;
    };

    struct SendQueue {
        boost::mutex mtx;
        bool waiting = false;
        asio::steady_timer semaphore;
        std::deque<OutgoingMessage> queues[4];
        size_t bytesInQueue = 0;
        size_t bytesInLow = 0;
        uint8_t nextIndex = 0;
        int credits[4] = {8, 4, 2, 1};

        explicit SendQueue(asio::io_context &ioc);
        bool empty() const;
    };

    coro<> sendLoop();
    coro<> sendMessage(OutgoingMessage &&msg);
    coro<> sendFrame(uint16_t type, Priority priority, FrameFlags flags, uint32_t streamId,
        std::span<const std::byte> payload);

    coro<> readExact(std::byte *data, size_t size);

    bool popNext(OutgoingMessage &out);
    void dropLow(size_t needBytes);
    void setError(const std::string &msg);

    Limits LimitsCfg;
    tcp::socket Socket;
    SendQueue Outgoing;
    std::shared_ptr<AsyncContext> Context;
    std::unordered_map<uint32_t, FragmentState> Fragments;
    uint32_t NextStreamId = 1;
};

coro<tcp::socket> asyncConnectTo(const std::string &address,
    std::function<void(const std::string&)> onProgress = nullptr);

} // namespace LV::Net2
