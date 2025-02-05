#pragma once

#include <atomic>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/mutex.hpp>
#include <mutex>
#include <boost/thread.hpp>

namespace LV {

template<typename T>
class Lockable {
public:
    template<typename ...Args>
    Lockable(Args&& ...args)
        : Obj(std::forward<Args>(args)...)
    {}

    class ReadLockGuard {
    public:
        template<typename ...Args>
        ReadLockGuard(T& obj, Args&& ...args) 
            : Lock(std::forward<Args>(args)...), Ref(obj) {}

        const T& operator*() const { assert(Lock.owns_lock()); return Ref; }
        const T* operator->() const { assert(Lock.owns_lock()); return &Ref; }

        bool owns_lock() {
            return Lock.owns_lock();
        }

        operator bool() {
            return Lock.owns_lock();
        }

        void unlock() {
            Lock.unlock();
        }

    private:
        boost::shared_lock<boost::shared_mutex> Lock;
        T& Ref;
    };

    class WriteLockGuard {
    public:
        template<typename ...Args>
        WriteLockGuard(T& obj, Args&& ...args) 
            : Lock(std::forward<Args>(args)...), Ref(obj) {}

        T& operator*() const { assert(Lock.owns_lock()); return Ref; }
        T* operator->() const { assert(Lock.owns_lock()); return &Ref; }

        bool owns_lock() {
            return Lock.owns_lock();
        }

        operator bool() {
            return Lock.owns_lock();
        }

        void unlock() {
            Lock.unlock();
        }
    private:
        std::unique_lock<boost::shared_mutex> Lock;
        T& Ref;
    };

    ReadLockGuard lock_read() {
        return ReadLockGuard(Obj, Mtx);
    }

    ReadLockGuard try_lock_read() {
        return ReadLockGuard(Obj, Mtx, boost::try_to_lock);
    }

    template<typename Clock, typename Duration>
    ReadLockGuard try_lock_read(const boost::chrono::time_point<Clock, Duration>& atime) {
        return ReadLockGuard(Obj, Mtx, atime);
    }

    WriteLockGuard lock_write() {
        return WriteLockGuard(Obj, Mtx);
    }

    WriteLockGuard try_lock_write() {
        return WriteLockGuard(Obj, Mtx, boost::try_to_lock);
    }

    template<typename Clock, typename Duration>
    WriteLockGuard try_lock_write(const boost::chrono::time_point<Clock, Duration>& atime) {
        return WriteLockGuard(Obj, Mtx, atime);
    }

    const T& no_lock_readable() { return Obj; }
    T& no_lock_writeable() { return Obj; }

private:
    T Obj;
    boost::shared_mutex Mtx;
};

class DestroyLock {
public:
    DestroyLock() = default;

    struct Guard {
        Guard(DestroyLock &lock)
            : Lock(lock)
        {
            lock.UseCount++;
        }

        ~Guard() {
            Lock.UseCount--;
        }

    private:
        DestroyLock &Lock;
    };

    void wait_no_use() {
        while(int val = UseCount)
            UseCount.wait(val);
    }

    Guard lock() {
        return Guard(*this);
    }

private:
    std::atomic<int> UseCount;
};

}