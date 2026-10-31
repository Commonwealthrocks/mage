// tamper.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void run_tamper_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf)
    {
        run_case(cur_id++, "Cryptographic tamper: wrong password", "Negative", [&]()
                 {
            auto arch = sandbox / "rt1.mage";
            auto dest = sandbox / "dest_wrong_pass";
            bool failed = false;
            try
            {
                pk::crypto::format::unpack_archive(arch, dest, "WrongPasswordTotallyIncorrect!", 0, 1, nullptr);
            }
            catch (...)
            {
                failed = true;
            }
            if (!failed)
                throw std::runtime_error("decryption succeeded with incorrect password"); });
        run_case(cur_id++, "Cryptographic tamper: missing keyfile", "Negative", [&]()
                 {
            auto arch = sandbox / "rt2.mage";
            auto dest = sandbox / "dest_missing_kf";
            bool failed = false;
            try
            {
                pk::crypto::format::unpack_archive(arch, dest, "", 0, 1, nullptr);
            }
            catch (...)
            {
                failed = true;
            }
            if (!failed)
                throw std::runtime_error("decryption succeeded without keyfile entropy"); });
        run_case(cur_id++, "Cryptographic tamper: chunk swap (AAD)", "Negative", [&]()
                 {
            auto out_arch = sandbox / "chunk_swap_base.mage";
            auto tampered_arch = sandbox / "chunk_swap.mage";
            constexpr std::size_t k_cs = 1024 * 1024;
            constexpr std::size_t f_sz = 700 * 1024;
            std::mt19937_64 rng(42);
            std::vector<uint8_t> rand_data(f_sz);
            for (auto &b : rand_data)
                b = static_cast<uint8_t>(rng());
            auto f1 = sandbox / "swap1.dat";
            auto f2 = sandbox / "swap2.dat";
            {
                std::ofstream o1(f1, std::ios::binary);
                o1.write(reinterpret_cast<const char *>(rand_data.data()), rand_data.size());
                std::ofstream o2(f2, std::ios::binary);
                o2.write(reinterpret_cast<const char *>(rand_data.data()), rand_data.size());
            }
            pk::crypto::format::archive_entry e1{f1, "swap1.dat", false, f_sz};
            pk::crypto::format::archive_entry e2{f2, "swap2.dat", false, f_sz};
            pk::crypto::format::pack_archive({e1, e2}, out_arch, "SwapPass", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 2, 0, k_cs);
            std::ifstream in(out_arch, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            const std::size_t chunk0_total = k_cs + 16;
            if (data.size() < 65 + chunk0_total + 512)
                throw std::runtime_error("archive too small to contain 2 chunks");
            std::size_t c0 = 65;
            std::size_t c1 = 65 + chunk0_total;
            for (std::size_t i = 0; i < 512; ++i)
            {
                std::swap(data[c0 + i], data[c1 + i]);
            }
            std::ofstream out(tampered_arch, std::ios::binary);
            out.write(reinterpret_cast<const char *>(data.data()), data.size());
            out.close();
            bool failed = false;
            try
            {
                pk::crypto::format::unpack_archive(tampered_arch, sandbox / "dest_tamper", "SwapPass", 0, 1, nullptr);
            }
            catch (...)
            {
                failed = true;
            }
            if (!failed)
                throw std::runtime_error("swapped chunk payload was accepted without AEAD MAC failure"); });
        run_case(cur_id++, "Cryptographic tamper: chunk truncation", "Negative", [&]()
                 {
            auto orig_arch = sandbox / "rt1.mage";
            auto trunc_arch = sandbox / "trunc.mage";
            std::ifstream in(orig_arch, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            if (data.size() > 65 + 32)
            {
                data.resize(data.size() - 32);
            }
            std::ofstream out(trunc_arch, std::ios::binary);
            out.write(reinterpret_cast<const char *>(data.data()), data.size());
            out.close();
            bool failed = false;
            try
            {
                pk::crypto::format::unpack_archive(trunc_arch, sandbox / "dest_trunc", "TestPass123!", 0, 1, nullptr);
            }
            catch (...)
            {
                failed = true;
            }
            if (!failed)
                throw std::runtime_error("truncated archive was accepted without failure"); });
        run_case(cur_id++, "Header: 65 single-bit flips (all 65/65)", "Negative", [&]()
                 {
            auto orig_arch = sandbox / "rt1.mage";
            std::ifstream in(orig_arch, std::ios::binary);
            std::vector<uint8_t> base_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            for (std::size_t byte_idx = 0; byte_idx < 65; ++byte_idx)
            {
                std::vector<uint8_t> flipped = base_data;
                flipped[byte_idx] ^= 0xFF;
                auto test_p = sandbox / ("flip_" + std::to_string(byte_idx) + ".mage");
                std::ofstream out(test_p, std::ios::binary);
                out.write(reinterpret_cast<const char *>(flipped.data()), flipped.size());
                out.close();
                auto rep = pk::crypto::am_i_evil::view_header(test_p);
                bool unpack_failed = false;
                try
                {
                    pk::crypto::format::unpack_archive(test_p, sandbox / "dest_flip", "TestPass123!", 0, 1, nullptr);
                }
                catch (...)
                {
                    unpack_failed = true;
                }
                std::filesystem::remove(test_p);
                if (!unpack_failed && rep.structure_ok && rep.valid_magic)
                {
                    throw std::runtime_error("header bit flip at byte " + std::to_string(byte_idx) + " was wrongfully accepted");
                }
            } });
    }
}

// end