// aes_ni.cpp
// last updated: 17/06/2026
#include "../.hpp/aes_ni.hpp"
#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif
namespace pk::crypto
{
    bool aes_ni_there()
    {
#ifdef _WIN32
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        return (cpuInfo[2] & (1 << 25)) != 0; // there's an easier way to do this... but don't matter
#else
        unsigned int eax, ebx, ecx, edx;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        {
            return (ecx & (1 << 25)) != 0;
        }
        return false;
#endif
    }
}

// end