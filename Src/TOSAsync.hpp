#pragma once

#include "boost/asio/awaitable.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/use_awaitable.hpp"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <memory>
#include <type_traits>

namespace TOS {

using namespace boost::asio::experimental::awaitable_operators;
template<typename T = void>
using coro = boost::asio::awaitable<T>;

class AsyncSemaphore
{
    boost::asio::deadline_timer Deadline;
    std::atomic<uint8_t> Lock = 0;

public:
    AsyncSemaphore(
        boost::asio::io_context& ioc)
            : Deadline(ioc, boost::posix_time::ptime(boost::posix_time::pos_infin))
    {}

    boost::asio::awaitable<void> async_wait() {
        try {
            co_await Deadline.async_wait(boost::asio::use_awaitable);
        } catch(boost::system::system_error code) {
            if(code.code() != boost::system::errc::operation_canceled)
                throw;
        }

        co_await boost::asio::this_coro::throw_if_cancelled();
    }

    boost::asio::awaitable<void> async_wait(std::function<bool()> predicate) {
        while(!predicate())
            co_await async_wait();
    }
    
    void notify_one() {
        Deadline.cancel_one();
    }

    void notify_all() {
        Deadline.cancel();
    }
};

class IAsyncDestructible : public std::enable_shared_from_this<IAsyncDestructible> {
protected:
    boost::asio::any_io_executor IOC;
    boost::asio::deadline_timer DestructLine;

    virtual coro<> asyncDestructor() { DestructLine.cancel(); co_return; }

public:
    IAsyncDestructible(boost::asio::any_io_executor ioc)
        : IOC(ioc), DestructLine(ioc, boost::posix_time::ptime(boost::posix_time::pos_infin))
    {}

    virtual ~IAsyncDestructible() {}

    coro<std::variant<std::monostate, std::monostate>> cancelable(coro<> &&c) { return std::move(c) || DestructLine.async_wait(boost::asio::use_awaitable); }

    template<typename T, typename = typename std::is_same<IAsyncDestructible, T>>
    static std::shared_ptr<T> createShared(boost::asio::any_io_executor ioc, T *ptr)
    {
        return std::shared_ptr<T>(ptr, [ioc = std::move(ioc)](IAsyncDestructible *ptr) {
            boost::asio::co_spawn(ioc, [](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        });
    }

    template<typename T, typename ...Args, typename = typename std::is_same<IAsyncDestructible, T>>
    static std::shared_ptr<T> makeShared(boost::asio::any_io_executor ioc, Args&& ... args)
    {
        std::shared_ptr<T>(new T(ioc, std::forward<Args>(args)..., [ioc = std::move(ioc)](IAsyncDestructible *ptr) {
            boost::asio::co_spawn(ioc, [](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        }));
    }
};

}