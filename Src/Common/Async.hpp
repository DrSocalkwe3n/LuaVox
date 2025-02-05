#pragma once

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/thread.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>


namespace LV {

using namespace boost::asio::experimental::awaitable_operators;

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
template<typename T = void>
using coro = asio::awaitable<T>;


class AsyncObject {
protected:
    asio::io_context &IOC;
    asio::deadline_timer WorkDeadline;

public:
    AsyncObject(asio::io_context &ioc)
        : IOC(ioc), WorkDeadline(ioc, boost::posix_time::pos_infin)
    {
    }

    inline asio::io_context& EXEC()
    {
        return IOC;
    }

protected:
    template<typename Coroutine>
    void co_spawn(Coroutine &&coroutine) {
        asio::co_spawn(IOC, WorkDeadline.async_wait(asio::use_awaitable) || std::move(coroutine), asio::detached);
    }
};


template<typename ValueType>
class AsyncAtomic : public AsyncObject {
protected:
    asio::deadline_timer Deadline;
    ValueType Value;
    boost::mutex Mtx;

public:
    AsyncAtomic(asio::io_context &ioc, ValueType &&value)
        : AsyncObject(ioc), Deadline(ioc), Value(std::move(value))
    {
    }

    AsyncAtomic& operator=(ValueType &&value) {
        boost::unique_lock lock(Mtx);
        Value = std::move(value);
        Deadline.expires_from_now(boost::posix_time::pos_infin);
        return *this;
    }

    operator ValueType() const {
        return Value;
    }

    ValueType operator*() const {
        return Value;
    }

    AsyncAtomic& operator++() {
        boost::unique_lock lock(Mtx);
        Value--;
        Deadline.expires_from_now(boost::posix_time::pos_infin);
        return *this;
    }

    AsyncAtomic& operator--() {
        boost::unique_lock lock(Mtx);
        Value--;
        Deadline.expires_from_now(boost::posix_time::pos_infin);
        return *this;
    }

    AsyncAtomic& operator+=(ValueType value) {
        boost::unique_lock lock(Mtx);
        Value += value;
        Deadline.expires_from_now(boost::posix_time::pos_infin);
        return *this;
    }

    AsyncAtomic& operator-=(ValueType value) {
        boost::unique_lock lock(Mtx);
        Value -= value;
        Deadline.expires_from_now(boost::posix_time::pos_infin);
        return *this;
    }

    void wait(ValueType oldValue) {
        while(true) {
            if(oldValue != Value)
                return;

            boost::unique_lock lock(Mtx);

            if(oldValue != Value)
                return;

            std::atomic_bool flag = false;
            Deadline.async_wait([&](boost::system::error_code errc) { flag.store(true); });
            lock.unlock();
            flag.wait(false);
        }
    }

    void await(ValueType needValue) {
        while(true) {
            if(needValue == Value)
                return;

            boost::unique_lock lock(Mtx);

            if(needValue == Value)
                return;

            std::atomic_bool flag = false;
            Deadline.async_wait([&](boost::system::error_code errc) { flag.store(true); });
            lock.unlock();
            flag.wait(false);
        }
    }

    coro<> async_wait(ValueType oldValue) {
        while(true) {
            if(oldValue != Value)
                co_return;

            boost::unique_lock lock(Mtx);

            if(oldValue != Value)
                co_return;

            auto coroutine = Deadline.async_wait();
            lock.unlock();
            try { co_await std::move(coroutine); } catch(...) {}
        }
    }

    coro<> async_await(ValueType needValue) {
        while(true) {
            if(needValue == Value)
                co_return;

            boost::unique_lock lock(Mtx);

            if(needValue == Value)
                co_return;

            auto coroutine = Deadline.async_wait();
            lock.unlock();
            try { co_await std::move(coroutine); } catch(...) {}
        }
    }
};

}