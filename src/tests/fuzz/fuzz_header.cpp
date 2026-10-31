// fuzz_header.cpp
// last updated: 31/10/2026
// compile with:
//   clang++ -O1 -g -fsanitize=fuzzer,address,undefined -Isrc/.hpp tests/fuzz/fuzz_header.cpp src/.cpp/am_i_evil.cpp src/.cpp/path_handler.cpp -o fuzz_header
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <string>
#include "../../.hpp/fileformat.hpp"
#include "../../.hpp/am_i_evil.hpp"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < sizeof(pk::crypto::format::header_f))
        return 0;
    if (size == sizeof(pk::crypto::format::header_f))
    {
        pk::crypto::format::header_f hdr{};
        std::memcpy(&hdr, data, sizeof(hdr));
        bool magic_ok = (std::memcmp(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4) == 0);
        bool version_ok = (hdr.version == 1);
        bool kdf_mem_safe = (hdr.memory_cost_kb <= 2 * 1024 * 1024);
        bool kdf_time_safe = (hdr.time_cost <= 32);
        bool kdf_lanes_safe = (hdr.parallelism <= 32);
        (void)magic_ok;
        (void)version_ok;
        (void)kdf_mem_safe;
        (void)kdf_time_safe;
        (void)kdf_lanes_safe;
    }
    return 0;
}

// end