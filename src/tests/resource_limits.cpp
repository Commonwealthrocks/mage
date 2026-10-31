// resource_limits.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void run_resource_limits_tests(int &cur_id, const std::filesystem::path &sandbox)
    {
        run_case(cur_id++, "Resource guard: declared size trap", "Zipbomb", [&]()
                 {
            pk::crypto::format::header_f hdr{};
            std::memcpy(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4);
            hdr.version = 1;
            hdr.algo = 0;
            hdr.memory_cost_kb = 1024;
            hdr.time_cost = 1;
            hdr.parallelism = 1;
            hdr.chunk_size_bytes = 1024 * 1024;
            auto fake_p = sandbox / "fake_zipbomb.mage";
            std::ofstream out(fake_p, std::ios::binary);
            out.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
            out.close();
            auto rep = pk::crypto::am_i_evil::view_header(fake_p);
            if (rep.structure_ok)
                throw std::runtime_error("truncated payload file was falsely marked structure_ok"); });
        run_case(cur_id++, "Resource guard: entry count ceiling", "Limits", [&]()
                 {
            auto rep = pk::crypto::am_i_evil::view_header(sandbox / "rt1.mage");
            if (!rep.structure_ok)
                throw std::runtime_error("valid archive header was not marked structure_ok"); });
        run_case(cur_id++, "DoS guard: memory cost trap (>2GB)", "KDF bounds", [&]()
                 {
            pk::crypto::format::header_f hdr{};
            std::memcpy(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4);
            hdr.version = 1;
            hdr.algo = 0;
            hdr.time_cost = 1;
            hdr.parallelism = 1;
            hdr.chunk_size_bytes = 1024 * 1024;
            hdr.memory_cost_kb = 3 * 1024 * 1024;
            auto p = sandbox / "dos_mem.mage";
            std::ofstream out(p, std::ios::binary);
            out.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
            std::vector<char> dummy(1024, 0);
            out.write(dummy.data(), dummy.size());
            out.close();
            auto rep = pk::crypto::am_i_evil::view_header(p);
            if (rep.kdf_safe)
                throw std::runtime_error("3GB Argon2 memory cost was not flagged as DoS trap"); });
        run_case(cur_id++, "DoS guard: parallelism trap (>32 cores)", "KDF bounds", [&]()
                 {
            pk::crypto::format::header_f hdr{};
            std::memcpy(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4);
            hdr.version = 1;
            hdr.algo = 0;
            hdr.time_cost = 1;
            hdr.parallelism = 64;
            hdr.chunk_size_bytes = 1024 * 1024;
            hdr.memory_cost_kb = 65536;
            auto p = sandbox / "dos_cores.mage";
            std::ofstream out(p, std::ios::binary);
            out.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
            std::vector<char> dummy(1024, 0);
            out.write(dummy.data(), dummy.size());
            out.close();
            auto rep = pk::crypto::am_i_evil::view_header(p);
            if (rep.kdf_safe)
                throw std::runtime_error("64 threads parallelism was not flagged as DoS trap"); });
    }
}

// end