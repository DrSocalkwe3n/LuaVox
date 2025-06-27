#pragma once

#include "TOSLib.hpp"
#include <functional>
#include "boost/system/detail/error_code.hpp"
#include "boost/system/system_error.hpp"
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <exception>
#include <memory>
#include <type_traits>
#include <list>



namespace TOS {

using namespace boost::asio::experimental::awaitable_operators;
template<typename T = void>
using coro = boost::asio::awaitable<T>;
namespace asio = boost::asio;

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


/*
    Многие могут уведомлять одного
    Ждёт события. После доставки уведомления ждёт повторно
*/
class MultipleToOne_AsyncSymaphore {
    asio::deadline_timer Timer;

public:
    MultipleToOne_AsyncSymaphore(asio::io_context &ioc)
        : Timer(ioc, boost::posix_time::ptime(boost::posix_time::pos_infin))
    {}

    void notify() {
        Timer.cancel();
    }

    void wait() {
        try { Timer.wait(); } catch(...) {}
        Timer.expires_at(boost::posix_time::ptime(boost::posix_time::pos_infin));
    }

    coro<> async_wait() {
        try { co_await Timer.async_wait(); } catch(...) {}
    }
};

class WaitableCoro {
    asio::io_context &IOC;
    std::shared_ptr<MultipleToOne_AsyncSymaphore> Symaphore;
    std::exception_ptr LastException;

public:
    WaitableCoro(asio::io_context &ioc)
        : IOC(ioc)
    {}

    void co_spawn(coro<> token) {
        Symaphore = std::make_shared<MultipleToOne_AsyncSymaphore>(IOC);
        asio::co_spawn(IOC, [token = std::move(token), symaphore = Symaphore]() -> coro<> {
            try { co_await std::move(const_cast<coro<>&>(token)); } catch(...) {}
            symaphore->notify();
        }, asio::detached);
    }

    void wait() {
        Symaphore->wait();
    }

    coro<> async_wait() {
        return Symaphore->async_wait();
    }
};

class AsyncUseControl {
public:
    class Lock {
        AsyncUseControl *AUC;

    public:
        Lock(AsyncUseControl *auc)
            : AUC(auc)
        {}

        Lock()
            : AUC(nullptr)
        {}

        ~Lock() {
            if(AUC)
                unlock();
        }

        Lock(const Lock&) = delete;
        Lock(Lock&& obj)
            : AUC(obj.AUC)
        {
            obj.AUC = nullptr;
        }

        Lock& operator=(const Lock&) = delete;
        Lock& operator=(Lock&& obj) {
            if(&obj == this)
                return *this;

            if(AUC)
                unlock();

            AUC = obj.AUC;
            obj.AUC = nullptr;

            return *this;           
        }

        void unlock() {
            assert(AUC);

            if(--AUC->Uses == 0 && AUC->OnNoUse) {
                asio::post(AUC->IOC, std::move(AUC->OnNoUse));
            }

            AUC = nullptr;
        }
    };

private:
    asio::io_context &IOC;
    std::move_only_function<void()> OnNoUse;
    std::atomic_int Uses = 0;

public:
    AsyncUseControl(asio::io_context &ioc)
        : IOC(ioc)
    {

    }

    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void()) Token = asio::default_completion_token_t<asio::io_context>>
    auto wait(Token&& token = asio::default_completion_token_t<asio::io_context>()) {
        auto initiation = [this](auto&& token) {
            int value;
            do {
                value = Uses.exchange(-1);
            } while(value == -1);

            OnNoUse = std::move(token);

            if(value == 0)
                OnNoUse();

            Uses.exchange(value);
        };

        return asio::async_initiate<Token, void()>(initiation, token);
    }

    Lock use() {
        int value;
        do {
            value = Uses.exchange(-1);
        } while(value == -1);

        if(OnNoUse)
            throw boost::system::system_error(asio::error::operation_aborted, "OnNoUse");

        Uses.exchange(++value);
        return Lock(this);
    }
};

/*
    Используется, чтобы вместо уничтожения объекта в умной ссылке, вызвать корутину с co_await asyncDestructor()
*/
class IAsyncDestructible : public std::enable_shared_from_this<IAsyncDestructible> {
protected:
    asio::io_context &IOC;
    AsyncUseControl AUC;

    virtual coro<> asyncDestructor() { co_await AUC.wait(); }

public:
    IAsyncDestructible(asio::io_context &ioc)
        : IOC(ioc), AUC(ioc)
    {}

    virtual ~IAsyncDestructible() {}

protected:
    template<typename T, typename = typename std::is_same<IAsyncDestructible, T>>
    static std::shared_ptr<T> createShared(asio::io_context &ioc, T *ptr)
    {
        return std::shared_ptr<T>(ptr, [&ioc = ioc](T *ptr) {
            boost::asio::co_spawn(ioc, [&ioc = ioc](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        });
    }

    template<typename T, typename = typename std::is_same<IAsyncDestructible, T>>
    static coro<std::shared_ptr<T>> createShared(T *ptr)
    {
        co_return std::shared_ptr<T>(ptr, [ioc = asio::get_associated_executor(co_await asio::this_coro::executor)](T *ptr) {
            boost::asio::co_spawn(ioc, [](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        });
    }

    template<typename T, typename = typename std::is_same<IAsyncDestructible, T>>
    static std::unique_ptr<T, std::function<void(T*)>> createUnique(asio::io_context &ioc, T *ptr)
    {
        return std::unique_ptr<T, std::function<void(T*)>>(ptr, [&ioc = ioc](T *ptr) {
            boost::asio::co_spawn(ioc, [](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        });
    }

    template<typename T, typename = typename std::is_same<IAsyncDestructible, T>>
    static coro<std::unique_ptr<T, std::function<void(T*)>>> createUnique(T *ptr)
    {
        co_return std::unique_ptr<T, std::function<void(T*)>>(ptr, [ioc = asio::get_associated_executor(co_await asio::this_coro::executor)](T *ptr) {
            boost::asio::co_spawn(ioc, [](IAsyncDestructible *ptr) -> coro<> {
                try { co_await ptr->asyncDestructor(); } catch(...) { }
                delete ptr;
                co_return;
            } (ptr), boost::asio::detached);
        });
    }
};

template<typename T>
class AsyncMutexObject {
public:
    class Lock {
    public:
        Lock(AsyncMutexObject* obj)
            : Obj(obj)
        {}

        Lock(const Lock& other) = delete;
        Lock(Lock&& other)
            : Obj(other.Obj)
        {
            other.Obj = nullptr;
        }

        ~Lock() {
			if(Obj)
                unlock();
        }

        Lock& operator=(const Lock& other) = delete;
        Lock& operator=(Lock& other) {
            if(&other == this)
                return *this;

            if(Obj)
                unlock();

            Obj = other.Obj;
            other.Obj = nullptr;
        }

        T& get() const { assert(Obj); return Obj->value; }
		T* operator->() const { assert(Obj); return &Obj->value; }
		T& operator*() const { assert(Obj); return Obj->value; }

		void unlock() { 
            assert(Obj);

            typename SpinlockObject<Context>::Lock ctx = Obj->Ctx.lock();
            if(ctx->Chain.empty()) {
                ctx->InExecution = false;
            } else {
                auto token = std::move(ctx->Chain.front());
                ctx->Chain.pop_front();
                ctx.unlock();
                token(Lock(Obj));
            }

            Obj = nullptr;
        }

    private:
        AsyncMutexObject *Obj;
    };

private:
    struct Context {
        std::list<std::move_only_function<void(Lock)>> Chain;
        bool InExecution = false;
    };

    SpinlockObject<Context> Ctx;
    T value;

public:
    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Lock)) Token = asio::default_completion_token_t<asio::io_context::executor_type>>
    auto lock(Token&& token = Token()) {
        auto initiation = [this](auto&& token) mutable {
            typename SpinlockObject<Context>::Lock ctx = Ctx.lock();

            if(ctx->InExecution) {
                ctx->Chain.emplace_back(std::move(token));
            } else {
                ctx->InExecution = true;
                ctx.unlock();
                token(Lock(this));
            }
        };

        return boost::asio::async_initiate<Token, void(Lock)>(std::move(initiation), token);
    }
};

}