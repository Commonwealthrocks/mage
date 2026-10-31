// fuzz_header.cpp
// last updated: 31/10/2026
// compile with:
//   clang++ -O1 -g -fsanitize=fuzzer,address,undefined -I.hpp tests/fuzz/fuzz_header.cpp -o fuzz_header
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "../../.hpp/fileformat.hpp"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < sizeof(pk::crypto::format::header_f))
        return 0;
    pk::crypto::format::header_f hdr{};
    std::memcpy(&hdr, data, sizeof(hdr));
    bool magic_ok = (std::memcmp(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4) == 0);
    bool version_ok = (hdr.version == 1);
    bool algo_ok = (hdr.algo <= 2);
    bool comp_ok = (hdr.compression_algo <= 2);
    bool meta_ok = (hdr.has_metadata <= 1);
    bool mem_safe = (hdr.memory_cost_kb >= 1024 && hdr.memory_cost_kb <= 2 * 1024 * 1024);
    bool time_safe = (hdr.time_cost >= 1 && hdr.time_cost <= 32);
    bool lanes_safe = (hdr.parallelism >= 1 && hdr.parallelism <= 32);
    bool chunk_safe = (hdr.chunk_size_bytes >= 1024 * 1024 && hdr.chunk_size_bytes <= 64 * 1024 * 1024);
    (void)magic_ok;
    (void)version_ok;
    (void)algo_ok;
    (void)comp_ok;
    (void)meta_ok;
    (void)mem_safe;
    (void)time_safe;
    (void)lanes_safe;
    (void)chunk_safe;
    return 0;
}

// end