// secure_memory.cpp
// last updated: 03/08/2026
#include "../.hpp/secure_memory.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <string.h>
#endif
#include "../.hpp/logger.hpp"
namespace pk::mem_
{
        bool secure_lock(void *ptr, std::size_t size)
        {
                if (!ptr || size == 0)
                        return true;
#ifdef _WIN32
                SIZE_T min_ws, max_ws;
                if (GetProcessWorkingSetSize(GetCurrentProcess(), &min_ws, &max_ws))
                {
                        SIZE_T needed = size + 4096 * 1024;
                        SIZE_T new_min = min_ws + needed;
                        SIZE_T new_max = max_ws + needed;
                        SetProcessWorkingSetSize(GetCurrentProcess(), new_min, new_max);
                }
                bool success = VirtualLock(ptr, size) != 0;
                if (!success)
                {
                        pk::core::logger::log("exception: VirtualLock failed; sensitive data may be paged to disk.");
                }
                return success;
#else
                bool success = mlock(ptr, size) == 0;
                if (!success)
                {
                        pk::core::logger::log("exception: mlock failed; sensitive data may be paged to disk.");
                }
                return success;
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
#else
                explicit_bzero(ptr, size);
#endif
        }
}

// end