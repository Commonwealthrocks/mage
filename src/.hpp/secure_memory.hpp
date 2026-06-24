// secure_memory.hpp
// last updated: 17/06/2026
#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <stdexcept>
namespace pk::mem_
{
    bool secure_lock(void *ptr, std::size_t size);
    bool secure_unlock(void *ptr, std::size_t size);
    void secure_wipe(void *ptr, std::size_t size);
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
            if (auto p = static_cast<T *>(std::malloc(n * sizeof(T))))
            {
                if (!secure_lock(p, n * sizeof(T)))
                {
                    secure_wipe(p, n * sizeof(T));
                    std::free(p);
                    throw std::runtime_error("secure_allocator failed to lock mem_ in RAM (RLIMIT_MEMLOCK / VirtualLock quota exceeded)");
                }
                return p;
            }
            throw std::bad_alloc();
        }
        void deallocate(T *p, std::size_t n) noexcept
        {
            if (p)
            {
                secure_wipe(p, n * sizeof(T));
                secure_unlock(p, n * sizeof(T));
                std::free(p);
            }
        }
    };
    template <typename T, typename U>
    bool operator==(const secure_allocator<T> &, const secure_allocator<U> &) { return true; }
    template <typename T, typename U>
    bool operator!=(const secure_allocator<T> &, const secure_allocator<U> &) { return false; }
    using secure_vector = std::vector<uint8_t, secure_allocator<uint8_t>>;
    using secure_string = std::basic_string<char, std::char_traits<char>, secure_allocator<char>>;
}

// end