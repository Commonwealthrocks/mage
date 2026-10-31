// roundtrip.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    namespace
    {
        void mk_test_tree(const std::filesystem::path &root)
        {
            std::filesystem::create_directories(root / "sub1" / "sub2");
            std::ofstream f1(root / "test1.txt");
            f1 << "sample file 1 content with redundant data for compression testing: AAAAAABBBBBBCCCCCC\n";
            std::ofstream f2(root / "sub1" / "test2.bin", std::ios::binary);
            std::vector<uint8_t> bdata(4096, 0xef);
            f2.write(reinterpret_cast<const char *>(bdata.data()), bdata.size());
        }
        void vut__(const std::filesystem::path &unpacked)
        {
            if (!std::filesystem::exists(unpacked / "test1.txt"))
                throw std::runtime_error("unpacked tree missing test1.txt");
            if (!std::filesystem::exists(unpacked / "sub1" / "test2.bin"))
                throw std::runtime_error("unpacked tree missing sub1/test2.bin");
            if (std::filesystem::file_size(unpacked / "sub1" / "test2.bin") != 4096)
                throw std::runtime_error("unpacked sub1/test2.bin size mismatch");
        }
    }
    void run_roundtrip_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf)
    {
        run_case(cur_id++, "Roundtrip: AES-GCM x ZSTD x metadata", "Matrix", [&]()
                 {
            auto src = sandbox / "src_rt1";
            auto out_arch = sandbox / "rt1.mage";
            auto dest = sandbox / "dest_rt1";
            mk_test_tree(src);
            std::vector<pk::crypto::format::archive_entry> entries;
            for (const auto &p : std::filesystem::recursive_directory_iterator(src))
            {
                pk::crypto::format::archive_entry ae;
                ae.source_path = p.path();
                ae.relative_path = std::filesystem::relative(p.path(), src).generic_string();
                ae.is_directory = p.is_directory();
                ae.file_size = ae.is_directory ? 0 : std::filesystem::file_size(p.path());
                entries.push_back(std::move(ae));
            }
            pk::crypto::format::pack_archive(entries, out_arch, "TestPass123!", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, true, 0, 3, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, "TestPass123!", 0, 1, nullptr);
            vut__(dest); });
        run_case(cur_id++, "Roundtrip: AES-GCM x LZMA2 x keyfile", "Matrix", [&]()
                 {
            auto src = sandbox / "src_rt2";
            auto out_arch = sandbox / "rt2.mage";
            auto dest = sandbox / "dest_rt2";
            auto kf_path = sandbox / "test.mgkx";
            mk_test_tree(src);
            pk::crypto::keyfile::generate(kf_path.string());
            auto kf_entropy = pk::crypto::keyfile::read(kf_path.string());
            std::string eff_pass(kf_entropy.begin(), kf_entropy.end());
            std::vector<pk::crypto::format::archive_entry> entries;
            for (const auto &p : std::filesystem::recursive_directory_iterator(src))
            {
                pk::crypto::format::archive_entry ae;
                ae.source_path = p.path();
                ae.relative_path = std::filesystem::relative(p.path(), src).generic_string();
                ae.is_directory = p.is_directory();
                ae.file_size = ae.is_directory ? 0 : std::filesystem::file_size(p.path());
                entries.push_back(std::move(ae));
            }
            pk::crypto::format::pack_archive(entries, out_arch, eff_pass, pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 1, 3, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, eff_pass, 0, 1, nullptr);
            vut__(dest); });
        run_case(cur_id++, "Roundtrip: XChaCha20 x Store x pwd+kf", "Matrix", [&]()
                 {
            auto src = sandbox / "src_rt3";
            auto out_arch = sandbox / "rt3.mage";
            auto dest = sandbox / "dest_rt3";
            auto kf_path = sandbox / "test3.mgkx";
            mk_test_tree(src);
            pk::crypto::keyfile::generate(kf_path.string());
            auto kf_entropy = pk::crypto::keyfile::read(kf_path.string());
            std::string eff_pass = "PasswordPart_" + std::string(kf_entropy.begin(), kf_entropy.end());
            std::vector<pk::crypto::format::archive_entry> entries;
            for (const auto &p : std::filesystem::recursive_directory_iterator(src))
            {
                pk::crypto::format::archive_entry ae;
                ae.source_path = p.path();
                ae.relative_path = std::filesystem::relative(p.path(), src).generic_string();
                ae.is_directory = p.is_directory();
                ae.file_size = ae.is_directory ? 0 : std::filesystem::file_size(p.path());
                entries.push_back(std::move(ae));
            }
            pk::crypto::format::pack_archive(entries, out_arch, eff_pass, pk::crypto::cipher::algorithm::xchacha20_poly1305, fast_kdf, false, 0, 0, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, eff_pass, 0, 1, nullptr);
            vut__(dest); });
        run_case(cur_id++, "Roundtrip: AES-SIV x Store x metadata", "Matrix", [&]()
                 {
            auto src = sandbox / "src_rt4";
            auto out_arch = sandbox / "rt4.mage";
            auto dest = sandbox / "dest_rt4";
            mk_test_tree(src);
            std::vector<pk::crypto::format::archive_entry> entries;
            for (const auto &p : std::filesystem::recursive_directory_iterator(src))
            {
                pk::crypto::format::archive_entry ae;
                ae.source_path = p.path();
                ae.relative_path = std::filesystem::relative(p.path(), src).generic_string();
                ae.is_directory = p.is_directory();
                ae.file_size = ae.is_directory ? 0 : std::filesystem::file_size(p.path());
                entries.push_back(std::move(ae));
            }
            pk::crypto::kdf::kdf_cfg siv_kdf = fast_kdf;
            siv_kdf.hash_length = 64;
            pk::crypto::format::pack_archive(entries, out_arch, "SIV_Password_123!", pk::crypto::cipher::algorithm::aes_256_siv, siv_kdf, true, 0, 0, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, "SIV_Password_123!", 0, 1, nullptr);
            vut__(dest); });
    }
}

// end