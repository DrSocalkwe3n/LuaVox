#include "Net.hpp"
#include <TOSLib.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/system_error.hpp>

namespace LV::Net {

using namespace TOS;

Server::~Server() {
    stop();
    wait();
}

bool Server::isStopped() {
    return !IsAlive;
}

void Server::stop() {
    NeedClose = true;
    NeedClose.notify_all();
    if(Acceptor.is_open())
        Acceptor.close();
}

void Server::wait() {
    while(bool val = IsAlive)
        IsAlive.wait(val);
}

coro<void> Server::async_wait() {
    co_await Lock.async_wait();
}

coro<void> Server::run() {
    IsAlive.store(true);

    try {
        while(true) { // TODO: ловить ошибки на async_accept
            co_spawn(OnConnect(co_await Acceptor.async_accept()));
        }
    } catch(const std::exception &exc) {
        //if(!NeedClose)
            // TODO: std::cout << exc.what() << std::endl;
    }

    Lock.cancel();
    IsAlive.store(false);
    IsAlive.notify_all();
}


AsyncSocket::~AsyncSocket() {
    boost::lock_guard lock(SendPackets.Mtx);

    if(SendPackets.Context)
        SendPackets.Context->NeedShutdown = true;

    SendPackets.SenderGuard.cancel();
    WorkDeadline.cancel();
}

void AsyncSocket::pushPackets(std::vector<Packet> *simplePackets, std::vector<SmartPacket> *smartPackets) {
    boost::unique_lock lock(SendPackets.Mtx);

    if(Socket.is_open() 
        && (SendPackets.SimpleBuffer.size() + (simplePackets ? simplePackets->size() : 0) >= MAX_SIMPLE_PACKETS
            || SendPackets.SmartBuffer.size() + (smartPackets ? smartPackets->size() : 0) >= MAX_SMART_PACKETS
            || SendPackets.SizeInQueue >= MAX_PACKETS_SIZE_IN_WAIT)) 
    {
        Socket.close();
        // TODO: std::cout << "Передоз пакетами, сокет закрыт" << std::endl;
    }

    if(!Socket.is_open()) {
        if(simplePackets)
            simplePackets->clear();

        if(smartPackets)
            smartPackets->clear();
        return;
    }

    size_t addedSize = 0;

    if(simplePackets) {
        for(Packet &packet : *simplePackets) {
            addedSize += packet.size();
            SendPackets.SimpleBuffer.push_back(std::move(packet));
        }

        simplePackets->clear();
    }

    if(smartPackets) {
        for(SmartPacket &packet : *smartPackets) {
            addedSize += packet.size();
            SendPackets.SmartBuffer.push_back(std::move(packet));
        }

        smartPackets->clear();
    }

    SendPackets.SizeInQueue += addedSize;

    if(SendPackets.WaitForSemaphore) {
        SendPackets.WaitForSemaphore = false;
        SendPackets.Semaphore.cancel();
        SendPackets.Semaphore.expires_at(boost::posix_time::pos_infin);
    }
}

std::string AsyncSocket::getError() const {
    return SendPackets.Context->Error;
}

bool AsyncSocket::isAlive() const {
    return !SendPackets.Context->NeedShutdown
        && !SendPackets.Context->RunSendShutdowned
        && Socket.is_open();
}

coro<> AsyncSocket::read(std::byte *data, uint32_t size) {
    while(size) {
        if(RecvSize == 0) {
            RecvSize = co_await Socket.async_receive(asio::buffer(RecvBuffer.data()+RecvPos, RecvBuffer.size()-RecvPos));
        }

        uint32_t needRecv = std::min<size_t>(size, RecvSize);
        std::copy(RecvBuffer.data()+RecvPos, RecvBuffer.data()+RecvPos+needRecv, data);
        data += needRecv;
        RecvPos += needRecv;
        RecvSize -= needRecv;
        size -= needRecv;

        if(RecvPos >= RecvBuffer.size())
            RecvPos = 0;
    }
}

void AsyncSocket::closeRead() {
    if(Socket.is_open() && !ReadShutdowned) {
        ReadShutdowned = true;
        Socket.shutdown(boost::asio::socket_base::shutdown_receive);
    }
}

coro<> AsyncSocket::waitForSend() {
    asio::deadline_timer waiter(IOC);

    while(!SendPackets.SimpleBuffer.empty()
        || !SendPackets.SmartBuffer.empty()
        || SendSize)
    {
        waiter.expires_from_now(boost::posix_time::milliseconds(1));
        co_await waiter.async_wait();
    }
}

coro<> AsyncSocket::runSender(std::shared_ptr<AsyncContext> context) {
    int NextBuffer = 0;

    try {
        while(!context->NeedShutdown) {
            {
                boost::unique_lock lock(SendPackets.Mtx);
                if(SendPackets.SimpleBuffer.empty() && SendPackets.SmartBuffer.empty()) {
                    SendPackets.WaitForSemaphore = true;
                    auto coroutine = SendPackets.Semaphore.async_wait();
                    lock.unlock();

                    try { co_await std::move(coroutine); } catch(...) {}
                    continue;
                } else {
                    for(int cycle = 0; cycle < 2; cycle++, NextBuffer++) {
                        if(NextBuffer % 2) {
                            while(!SendPackets.SimpleBuffer.empty()) {
                                Packet &packet = SendPackets.SimpleBuffer.front();

                                if(SendSize+packet.size() >= SendBuffer.size())
                                    break;

                                size_t packetSize = packet.size();
                                for(const auto &page : packet.getPages()) {
                                    size_t needCopy = std::min<size_t>(packetSize, NetPool::PageSize);
                                    std::copy(page.data(), page.data()+needCopy, SendBuffer.data()+SendSize);
                                    SendSize += needCopy;
                                    packetSize -= needCopy;
                                }

                                SendPackets.SimpleBuffer.pop_front();
                            }
                        } else {
                            while(!SendPackets.SmartBuffer.empty()) {
                                SmartPacket &packet = SendPackets.SmartBuffer.front();

                                if(SendSize+packet.size() >= SendBuffer.size())
                                    break;

                                if(packet.IsStillRelevant && !packet.IsStillRelevant()) {
                                    SendPackets.SmartBuffer.pop_front();
                                    continue;
                                }

                                size_t packetSize = packet.size();
                                for(const auto &page : packet.getPages()) {
                                    size_t needCopy = std::min<size_t>(packetSize, NetPool::PageSize);
                                    std::copy(page.data(), page.data()+needCopy, SendBuffer.data()+SendSize);
                                    SendSize += needCopy;
                                    packetSize -= needCopy;
                                }

                                if(packet.OnSend) {
                                    std::optional<SmartPacket> nextPacket = packet.OnSend();
                                    if(nextPacket)
                                        SendPackets.SmartBuffer.push_back(std::move(*nextPacket));
                                }

                                SendPackets.SmartBuffer.pop_front();
                            }
                        }
                    }
                }
            }

            if(!SendSize)
                continue;

            try {
                co_await asio::async_write(Socket, asio::buffer(SendBuffer.data(), SendSize));
                SendSize = 0;
            } catch(const std::exception &exc) {
                context->Error = exc.what();
                break;
            }
        }
    } catch(...) {}

    context->RunSendShutdowned = true;
}

coro<tcp::socket> asyncConnectTo(const std::string address, std::function<void(const std::string&)> onProgress) {
    std::string progress;
    auto addLog = [&](const std::string &msg) {
        progress += '\n';
        progress += msg;

        if(onProgress)
            onProgress('\n'+msg);
    };

    auto ioc = co_await asio::this_coro::executor;

    addLog("Разбор адреса " + address);
    auto re = Str::match(address, "((?:\\[[\\d\\w:]+\\])|(?:[\\d\\.]+))(?:\\:(\\d+))?");

    std::vector<std::tuple<tcp::endpoint, std::string>> eps;

    if(!re) {
        re = Str::match(address, "([-_\\.\\w\\d]+)(?:\\:(\\d+))?");
        if(!re) {
            MAKE_ERROR("Не удалось разобрать адрес");
        }

        tcp::resolver resv{ioc};
        tcp::resolver::results_type result;

        addLog("Разрешение имён...");

        result = co_await resv.async_resolve(*re->at(1), re->at(2) ? *re->at(2) : "7890");

        addLog("Получено " + std::to_string(result.size()) + " точек");
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
        eps.emplace_back(tcp::endpoint{asio::ip::make_address(*re->at(1)), (uint16_t) (re->at(2) ? Str::toVal<int>(*re->at(2)) : 7890)}, *re->at(1));
    }

    for(auto [ep, hostname] : eps) {
        addLog("Подключение к " + hostname +" (" + ep.address().to_string() + ':' + std::to_string(ep.port()) + ")");
        
        try {
            tcp::socket sock{ioc};
            co_await sock.async_connect(ep);
            addLog("Подключились");
            co_return sock;
        } catch(const std::exception &exc) {
            addLog(std::string("Сокет не смог установить соединение: ") + exc.what());
        }
    }

    MAKE_ERROR("Не удалось подключится к серверу");
}

}