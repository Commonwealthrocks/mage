// secure_memory.cpp
// last updated: 30/10/2026
#include "../.hpp/secure_memory.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#endif
#include "../.hpp/logger.hpp"
namespace pk::mem_
{
    namespace
    {
        std::atomic<bool> g_lock_warned{false};
#ifdef _WIN32
        std::mutex g_ws_mutex;
#else
        std::size_t round_up_page_size(std::size_t size) noexcept
        {
            long pg = sysconf(_SC_PAGESIZE);
            std::size_t page_sz = (pg > 0) ? static_cast<std::size_t>(pg) : 4096u;
            std::size_t eff = (size == 0) ? page_sz : size;
            std::size_t rem = eff % page_sz;
            if (rem == 0)
                return eff;
            if (eff > std::numeric_limits<std::size_t>::max() - (page_sz - rem))
                return 0;
            return eff + (page_sz - rem);
        }
#endif
    }
    bool secure_lock(void *ptr, std::size_t size)
    {
        if (!ptr || size == 0)
            return true;
#ifdef _WIN32
        if (VirtualLock(ptr, size) != 0)
            return true;
        if (GetLastError() == ERROR_WORKING_SET_QUOTA && size <= 256ULL * 1024 * 1024)
        {
            std::lock_guard<std::mutex> lock(g_ws_mutex);
            SIZE_T min_ws = 0, max_ws = 0;
            if (GetProcessWorkingSetSize(GetCurrentProcess(), &min_ws, &max_ws))
            {
                constexpr SIZE_T k_ws_cap = 256ULL * 1024 * 1024;
                SIZE_T desired_min = std::min<SIZE_T>(k_ws_cap, std::max<SIZE_T>(min_ws, size + 8ULL * 1024 * 1024));
                SIZE_T desired_max = std::max<SIZE_T>(max_ws, desired_min + 32ULL * 1024 * 1024);
                SetProcessWorkingSetSize(GetCurrentProcess(), desired_min, desired_max);
            }
            if (VirtualLock(ptr, size) != 0)
                return true;
        }
        if (!g_lock_warned.exchange(true))
        {
            pk::core::logger::log("warn: VirtualLock quota reached; large sensitive buffers use isolated pages without RAM lock.");
        }
        return false;
#else
        if (mlock(ptr, size) == 0)
            return true;
        if (!g_lock_warned.exchange(true))
        {
            pk::core::logger::log("warn: mlock limit (RLIMIT_MEMLOCK) reached; sensitive buffers use isolated non-dumpable pages.");
        }
        return false;
#endif
    }
    bool secure_unlock(void *ptr, std::size_t size)
    {
        if (!ptr || size == 0)
            return true;
#ifdef _WIN32
        return VirtualUnlock(ptr, size) != 0;
#else
        return munlock(ptr, size) == 0;
#endif
    }
    void secure_wipe(void *ptr, std::size_t size)
    {
        if (!ptr || size == 0)
            return;
#ifdef _WIN32
        RtlSecureZeroMemory(ptr, size);
#elif defined(__GLIBC__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
        explicit_bzero(ptr, size);
#else
        volatile unsigned char *vp = static_cast<volatile unsigned char *>(ptr);
        for (std::size_t i = 0; i < size; ++i)
        {
            vp[i] = 0;
        }
#endif
    }
    void *secure_alloc_pages(std::size_t size)
    {
#ifdef _WIN32
        const std::size_t alloc_sz = (size == 0) ? 4096u : size;
        void *p = VirtualAlloc(nullptr, alloc_sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p)
            return nullptr;
        (void)secure_lock(p, alloc_sz);
        return p;
#else
        const std::size_t alloc_sz = round_up_page_size(size);
        if (alloc_sz == 0)
            return nullptr;
        void *p = mmap(nullptr, alloc_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED)
            return nullptr;
#ifdef MADV_DONTDUMP
        (void)madvise(p, alloc_sz, MADV_DONTDUMP);
#endif
        (void)secure_lock(p, alloc_sz);
        return p;
#endif
    }
    void secure_free_pages(void *ptr, std::size_t size) noexcept
    {
        if (!ptr)
            return;
#ifdef _WIN32
        const std::size_t alloc_sz = (size == 0) ? 4096u : size;
        secure_wipe(ptr, alloc_sz);
        (void)VirtualUnlock(ptr, alloc_sz);
        (void)VirtualFree(ptr, 0, MEM_RELEASE);
#else
        const std::size_t alloc_sz = round_up_page_size(size);
        if (alloc_sz > 0)
        {
            secure_wipe(ptr, alloc_sz);
            (void)munlock(ptr, alloc_sz);
            (void)munmap(ptr, alloc_sz);
        }
#endif
    }
}

// end