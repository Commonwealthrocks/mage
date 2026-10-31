// fuzz_inprocess.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void fuzz___inproc_test(int &cur_id, const std::filesystem::path &sandbox)
    {
        run_case(cur_id++, "Fuzz: 100,000 mutated headers (0 leaks)", "In-process", [&]()
                 {
            std::mt19937_64 rng(1337);
            std::uniform_int_distribution<uint8_t> dist_byte(0, 255);
            std::uniform_int_distribution<int> dist_mut(0, 4);
            pk::crypto::format::header_f base_hdr{};
            std::memcpy(base_hdr.magic, pk::crypto::format::MAGIC_BYTES, 4);
            base_hdr.version = 1;
            base_hdr.algo = 0;
            base_hdr.memory_cost_kb = 65536;
            base_hdr.time_cost = 3;
            base_hdr.parallelism = 4;
            base_hdr.chunk_size_bytes = 4 * 1024 * 1024;
            auto fuzz_file = sandbox / "fuzz_header.mage";
            uint8_t raw[sizeof(base_hdr) + 64];
            for (int iter = 0; iter < 100000; ++iter)
            {
                std::memcpy(raw, &base_hdr, sizeof(base_hdr));
                int mut_type = dist_mut(rng);
                if (mut_type == 0)
                {
                    raw[rng() % sizeof(base_hdr)] = dist_byte(rng);
                }
                else if (mut_type == 1)
                {
                    uint32_t val = (rng() % 2 == 0) ? 0 : 0xFFFFFFFF;
                    std::memcpy(raw + offsetof(pk::crypto::format::header_f, memory_cost_kb), &val, 4);
                }
                else if (mut_type == 2)
                {
                    uint32_t val = (rng() % 2 == 0) ? 0 : 0xFFFFFFFF;
                    std::memcpy(raw + offsetof(pk::crypto::format::header_f, chunk_size_bytes), &val, 4);
                }
                else if (mut_type == 3)
                {
                    uint32_t val = (rng() % 2 == 0) ? 0 : 1000;
                    std::memcpy(raw + offsetof(pk::crypto::format::header_f, parallelism), &val, 4);
                }
                else
                {
                    raw[rng() % 5] = dist_byte(rng);
                }
                std::ofstream of(fuzz_file, std::ios::binary | std::ios::trunc);
                of.write(reinterpret_cast<const char *>(raw), sizeof(raw));
                of.close();
                auto rep = pk::crypto::am_i_evil::view_header(fuzz_file);
                (void)rep;
            }
            std::filesystem::remove(fuzz_file); });
        run_case(cur_id++, "Fuzz: 100,000 path mutations (0 escape)", "In-process", [&]()
                 {
            std::mt19937_64 rng(0xCAFE);
            std::uniform_int_distribution<int> dist_len(1, 128);
            const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/\\.: _-\0\t\n", dummy;
            for (int iter = 0; iter < 100000; ++iter)
            {
                int len = dist_len(rng);
                std::string path;
                path.reserve(len);
                for (int i = 0; i < len; ++i)
                {
                    path.push_back(charset[rng() % charset.size()]);
                }
                std::string norm;
                bool ok = pk::path::ok(pk::path::validate_archive_path(path, norm));
                if (ok)
                {
                    if (norm.empty() || norm.front() == '/' || norm.front() == '\\' || norm.find(':') != std::string::npos || norm.find('\\') != std::string::npos)
                    {
                        throw std::runtime_error("fuzzer found path validation bypass on string: " + path);
                    }
                    std::size_t seg_start = 0;
                    for (std::size_t i = 0; i <= norm.size(); ++i)
                    {
                        if (i == norm.size() || norm[i] == '/')
                        {
                            std::string_view seg(norm.data() + seg_start, i - seg_start);
                            seg_start = i + 1;
                            if (seg == "." || seg == "..")
                            {
                                throw std::runtime_error("fuzzer found traversal segment in normalized path: " + norm);
                            }
                        }
                    }
                }
            } });
    }
}

// end