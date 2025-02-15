#pragma once

#include "MemoryPool.hpp"
#include "Async.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>
#include <boost/thread.hpp>
#include <boost/circular_buffer.hpp>
#include <condition_variable>

namespace LV::Net {

class SocketServer : public AsyncObject {
protected:
    tcp::acceptor Acceptor;

public:
    SocketServer(asio::io_context &ioc, std::function<coro<>(tcp::socket)> &&onConnect, uint16_t port = 0)
        : AsyncObject(ioc), Acceptor(ioc, tcp::endpoint(tcp::v4(), port))
    {
        assert(onConnect);

        co_spawn(run(std::move(onConnect)));
    }

    bool isStopped();
    uint16_t getPort() {
        return Acceptor.local_endpoint().port();
    }

protected:
    coro<void> run(std::function<coro<>(tcp::socket)> onConnect);
};

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN
    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    static inline T swapEndian(const T &u) { return u; }
#else
    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    static inline T swapEndian(const T &u) {
        if constexpr (sizeof(T) == 1) {
            return u;
        } else if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(u);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(u);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(u);
        } else {
            static_assert(sizeof(T) <= 8, "Неподдерживаемый размер для перестановки порядка байтов (Swap Endian)");
            return u;
        }
    }
#endif
    // Запись в сторону сокета производится пакетами
    // Считывание потоком

    using NetPool = BoostPool<12, 14>;

    class Packet {
        static constexpr size_t MAX_PACKET_SIZE = 1 << 16;
        uint16_t Size = 0;
        std::vector<NetPool::PagePtr> Pages;

    public:
        Packet() = default;
        Packet(const Packet&) = default;
        Packet(Packet &&obj)
            : Size(obj.Size), Pages(std::move(obj.Pages))
        {
            obj.Size = 0;
        }

        Packet& operator=(const Packet&) = default;

        Packet& operator=(Packet &&obj) {
            if(&obj == this)
                return *this;

            Size = obj.Size;
            Pages = std::move(obj.Pages);
            obj.Size = 0;

            return *this;
        }

        inline Packet& write(const std::byte *data, uint16_t size) {
            assert(Size+size < MAX_PACKET_SIZE);

            while(size) {
                if(Pages.size()*NetPool::PageSize == Size)
                    Pages.emplace_back();

                uint16_t needWrite = std::min<uint16_t>(Pages.size()*NetPool::PageSize-Size, size);
                std::byte *ptr = &Pages.back().front() + (Size % NetPool::PageSize);
                std::copy(data, data+needWrite, ptr);
                Size += needWrite;
                size -= needWrite;
            }

            return *this;
        }

        template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
        inline Packet& write(T u) {
            u = swapEndian(u);
            write((const std::byte*) &u, sizeof(u));
            return *this;
        }

        inline Packet& write(std::string_view str) {
            assert(Size+str.size()+2 < MAX_PACKET_SIZE);
            write((uint16_t) str.size());
            write((const std::byte*) str.data(), str.size());
            return *this;
        }

        inline Packet& write(const std::string &str) {
            return write(std::string_view(str));
        }

        inline uint16_t size() const { return Size; }
        inline const std::vector<NetPool::PagePtr>& getPages() const { return Pages; }

        template<typename T, std::enable_if_t<std::is_integral_v<T> or std::is_convertible_v<T, std::string_view>, int> = 0>
        inline Packet& operator<<(const T &value) {
            if constexpr (std::is_convertible_v<T, std::string_view>)
                return write((std::string_view) value);
            else
                return write(value);
        }

        void clear() {
            clearFast();
            Pages.clear();
        }

        void clearFast() {
            Size = 0;
        }

        Packet& complite(std::vector<std::byte> &out) {
            out.resize(Size);

            for(size_t pos = 0; pos < Size; pos += NetPool::PageSize) {
                const char *data = (const char*) Pages[pos / NetPool::PageSize].data();
                std::copy(data, data+std::min<size_t>(Size-pos, NetPool::PageSize), (char*) &out[pos]);
            }

            return *this;
        }

        std::vector<std::byte> complite() {
            std::vector<std::byte> out;
            complite(out);
            return out;
        }

        coro<> sendAndFastClear(tcp::socket &socket) {
            for(size_t pos = 0; pos < Size; pos += NetPool::PageSize) {
                const char *data = (const char*) Pages[pos / NetPool::PageSize].data();
                size_t size = std::min<size_t>(Size-pos, NetPool::PageSize);
                co_await asio::async_write(socket, asio::const_buffer(data, size));
            }

            clearFast();
        }
    };

    class SmartPacket : public Packet {
    public:
        std::function<bool()> IsStillRelevant;
        std::function<std::optional<SmartPacket>()> OnSend;
    };

    class AsyncSocket : public AsyncObject {
        NetPool::Array<32> RecvBuffer, SendBuffer;
        size_t RecvPos = 0, RecvSize = 0, SendSize = 0;
        bool ReadShutdowned = false;
        tcp::socket Socket;

        static constexpr uint32_t 
            MAX_SIMPLE_PACKETS = 8192, 
            MAX_SMART_PACKETS = MAX_SIMPLE_PACKETS/4,
            MAX_PACKETS_SIZE_IN_WAIT = 1 << 24;

        struct AsyncContext {
            volatile bool NeedShutdown = false, RunSendShutdowned = false;
            std::string Error;
        };

        struct SendPacketsObj {
            boost::mutex Mtx;
            bool WaitForSemaphore = false;
            asio::deadline_timer Semaphore, SenderGuard;
            boost::circular_buffer_space_optimized<Packet> SimpleBuffer;
            boost::circular_buffer_space_optimized<SmartPacket> SmartBuffer;
            size_t SizeInQueue = 0;
            std::shared_ptr<AsyncContext> Context;

            SendPacketsObj(asio::io_context &ioc)
                : Semaphore(ioc, boost::posix_time::pos_infin), SenderGuard(ioc, boost::posix_time::pos_infin)
            {}
        } SendPackets;

    public:
        AsyncSocket(asio::io_context &ioc, tcp::socket &&socket)
            : AsyncObject(ioc), Socket(std::move(socket)), SendPackets(ioc)
        { 
            SendPackets.SimpleBuffer.set_capacity(512);
            SendPackets.SmartBuffer.set_capacity(SendPackets.SimpleBuffer.capacity()/4);
            SendPackets.Context = std::make_shared<AsyncContext>();

            boost::asio::socket_base::linger optionLinger(true, 4); // После закрытия сокета оставшиеся данные будут доставлены
            Socket.set_option(optionLinger);
            boost::asio::ip::tcp::no_delay optionNoDelay(true); // Отключает попытки объёденить данные в крупные пакеты
            Socket.set_option(optionNoDelay);

            co_spawn(runSender(SendPackets.Context));
        }

        ~AsyncSocket();
        
        void pushPackets(std::vector<Packet> *simplePackets, std::vector<SmartPacket> *smartPackets = nullptr);
        
        void pushPacket(Packet &&simplePacket) {
            std::vector<Packet> out(1);
            out[0] = std::move(simplePacket);
            pushPackets(&out);
        }

        std::string getError() const;
        bool isAlive() const;

        coro<> read(std::byte *data, uint32_t size);
        void closeRead();

        template<typename T, std::enable_if_t<std::is_integral_v<T> or std::is_same_v<T, std::string>, int> = 0>
        coro<T> read() {
            if constexpr(std::is_integral_v<T>) {
                T value;
                co_await read((std::byte*) &value, sizeof(value));
                co_return swapEndian(value);
            } else {
                uint16_t size = co_await read<uint16_t>();
                T value(size, ' ');
                co_await read((std::byte*) value.data(), size);
                co_return value;}
        }

        coro<> waitForSend();


        static inline coro<> read(tcp::socket &socket, std::byte *data, uint32_t size) {
            co_await asio::async_read(socket, asio::mutable_buffer(data, size));
        }

        template<typename T, std::enable_if_t<std::is_integral_v<T> or std::is_same_v<T, std::string>, int> = 0>
        static inline coro<T> read(tcp::socket &socket) {
            if constexpr(std::is_integral_v<T>) {
                T value;
                co_await read(socket, (std::byte*) &value, sizeof(value));
                co_return swapEndian(value);
            } else {
                uint16_t size = co_await read<uint16_t>(socket);
                T value(size, ' ');
                co_await read(socket, (std::byte*) value.data(), size);
                co_return value;}
        }

        static inline coro<> write(tcp::socket &socket, const std::byte *data, uint16_t size) {
            co_await asio::async_write(socket, asio::const_buffer(data, size));
        }

        template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
        static inline coro<> write(tcp::socket &socket, T u) {
            u = swapEndian(u);
            co_await write(socket, (const std::byte*) &u, sizeof(u));
        }

        static inline coro<> write(tcp::socket &socket, std::string_view str) {
            co_await write(socket, (uint16_t) str.size());
            co_await write(socket, (const std::byte*) str.data(), str.size());
        }

        static inline coro<> write(tcp::socket &socket, const std::string &str) {
            return write(socket, std::string_view(str));
        }

        static inline coro<> write(tcp::socket &socket, const char *str) {
            return write(socket, std::string_view(str));
        }

    private:
        coro<> runSender(std::shared_ptr<AsyncContext> context);
    };

    coro<tcp::socket> asyncConnectTo(const std::string address, std::function<void(const std::string&)> onProgress = nullptr);
}