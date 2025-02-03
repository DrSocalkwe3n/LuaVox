#pragma once

#include <boost/pool/pool_alloc.hpp>


namespace AL {

template<unsigned PageSize_PowOf2, unsigned CountPageInChunk_PowOf2, unsigned MaxSize = 0, typename Mutex = boost::details::pool::default_mutex, typename UserAllocator = boost::default_user_allocator_new_delete>
struct BoostPool {
    static constexpr unsigned
        PageSize = 1 << PageSize_PowOf2,
        PagesInChunk = 1 << CountPageInChunk_PowOf2,
        ChunkSize = PageSize << CountPageInChunk_PowOf2;

    using Page = std::array<std::byte, PageSize>;
    using Allocator = boost::fast_pool_allocator<Page, UserAllocator, Mutex, PagesInChunk, MaxSize>;
    using SingletonPool = boost::singleton_pool<boost::fast_pool_allocator_tag, PageSize, UserAllocator, Mutex, PagesInChunk, MaxSize>;
    
    using vector = std::vector<Page, Allocator>;
    
    template<uint16_t PagesNum>
    class Array {
        void *Ptr = nullptr;

    public:
        Array() {
            Ptr = SingletonPool::ordered_malloc(PagesNum);
        }

        ~Array() {
            if(Ptr)
                SingletonPool::ordered_free(Ptr, PagesNum);
        }

        Array(const Array &array) {
            Ptr = SingletonPool::ordered_malloc(PagesNum);
            std::copy((const std::byte*) array.Ptr, (const std::byte*) array.Ptr + PageSize*PagesNum, (std::byte*) Ptr);
        }

        Array(Array &&pages) {
            Ptr = pages.Ptr;
            pages.Ptr = nullptr;
        }

        Array& operator=(const Array &pages) {
            if(this == &pages)
                return *this;

            std::copy((const std::byte*) pages.Ptr, (const std::byte*) pages.Ptr + PageSize*PagesNum, (std::byte*) Ptr);
            
            return *this;
        }

        Array& operator=(Array &&pages) {
            if(this == &pages)
                return *this;

            std::swap(Ptr, pages.Ptr);

            return *this;
        }

        std::byte* data() { return (std::byte*) Ptr; }
        const std::byte* data() const { return (std::byte*) Ptr; }
        constexpr size_t size() const { return PageSize*PagesNum; }

        std::byte& front() { return *(std::byte*) Ptr; }
        const std::byte& front() const { return *(const std::byte*) Ptr; }
        std::byte& back() { return *((std::byte*) Ptr + size()); }
        const std::byte& back() const { return *((const std::byte*) Ptr + size()); }

        std::byte& operator[](size_t index) { return ((std::byte*) Ptr)[index]; }
        const std::byte& operator[](size_t index) const { return ((const std::byte*) Ptr)[index]; }
    };

    class PagePtr {
        void *Ptr = nullptr;

    public:
        PagePtr() {
            Ptr = SingletonPool::malloc();
        }

        ~PagePtr() {
            if(Ptr)
                SingletonPool::free(Ptr);
        }

        PagePtr(const PagePtr &array) {
            Ptr = SingletonPool::malloc();
            std::copy((const std::byte*) array.Ptr, (const std::byte*) array.Ptr + PageSize, (std::byte*) Ptr);
        }

        PagePtr(PagePtr &&pages) {
            Ptr = pages.Ptr;
            pages.Ptr = nullptr;
        }

        PagePtr& operator=(const PagePtr &pages) {
            if(this == &pages)
                return *this;

            std::copy((const std::byte*) pages.Ptr, (const std::byte*) pages.Ptr + PageSize, (std::byte*) Ptr);
            
            return *this;
        }

        PagePtr& operator=(PagePtr &&pages) {
            if(this == &pages)
                return *this;

            std::swap(Ptr, pages.Ptr);

            return *this;
        }

        std::byte* data() { return (std::byte*) Ptr; }
        const std::byte* data() const { return (std::byte*) Ptr; }
        constexpr size_t size() const { return PageSize; }

        std::byte& front() { return *(std::byte*) Ptr; }
        const std::byte& front() const { return *(const std::byte*) Ptr; }
        std::byte& back() { return *((std::byte*) Ptr + size()); }
        const std::byte& back() const { return *((const std::byte*) Ptr + size()); }

        std::byte& operator[](size_t index) { return ((std::byte*) Ptr)[index]; }
        const std::byte& operator[](size_t index) const { return ((const std::byte*) Ptr)[index]; }
    };
};

}