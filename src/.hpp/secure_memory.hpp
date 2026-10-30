// secure_memory.hpp
// last updated: 30/10/2026
#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <new>
namespace pk::mem_
{
    bool secure_lock(void *ptr, std::size_t size);
    bool secure_unlock(void *ptr, std::size_t size);
    void secure_wipe(void *ptr, std::size_t size);
    void *secure_alloc_pages(std::size_t size);
    void secure_free_pages(void *ptr, std::size_t size) noexcept;
    template <typename T>
    class secure_allocator
    {
    public:
        using value_type = T;
        secure_allocator() noexcept = default;
        template <typename U>
        secure_allocator(const secure_allocator<U> &) noexcept {}
        T *allocate(std::size_t n)
        {
            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_alloc();
            void *p = secure_alloc_pages(n * sizeof(T));
            if (!p)
                throw std::bad_alloc();
            return static_cast<T *>(p);
        }
        void deallocate(T *p, std::size_t n) noexcept
        {
            if (p)
            {
                secure_free_pages(p, n * sizeof(T));
            }
        }
    };
    template <typename T, typename U>
    bool operator==(const secure_allocator<T> &, const secure_allocator<U> &) noexcept { return true; }
    template <typename T, typename U>
    bool operator!=(const secure_allocator<T> &, const secure_allocator<U> &) noexcept { return false; }
    using secure_vector = std::vector<uint8_t, secure_allocator<uint8_t>>;
    using secure_string = std::basic_string<char, std::char_traits<char>, secure_allocator<char>>;
}

// end