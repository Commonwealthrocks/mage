// boundary.cpp
// last updated: 31/10/2026
#include "test_common.hpp"

namespace pk::test
{
    void run_boundary_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf)
    {
        run_case(cur_id++, "Boundary: empty archive (0 entries)", "Geometry", [&]()
                 {
            auto out_arch = sandbox / "empty.mage";
            auto dest = sandbox / "dest_empty";
            std::vector<pk::crypto::format::archive_entry> entries;
            pk::crypto::format::pack_archive(entries, out_arch, "EmptyPass", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 0, 0, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, "EmptyPass", 0, 1, nullptr);
            auto rep = pk::crypto::am_i_evil::view_header(out_arch);
            if (!rep.structure_ok || !rep.valid_magic)
                throw std::runtime_error("empty archive header inspection failed"); });
        run_case(cur_id++, "Boundary: single 0-byte file", "Geometry", [&]()
                 {
            auto out_arch = sandbox / "zero.mage";
            auto dest = sandbox / "dest_zero";
            auto zfile = sandbox / "zero.dat";
            {
                std::ofstream f(zfile);
            }
            pk::crypto::format::archive_entry ae;
            ae.source_path = zfile;
            ae.relative_path = "zero.dat";
            ae.is_directory = false;
            ae.file_size = 0;
            pk::crypto::format::pack_archive({ae}, out_arch, "ZeroPass", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 0, 0, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, "ZeroPass", 0, 1, nullptr);
            if (!std::filesystem::exists(dest / "zero.dat") || std::filesystem::file_size(dest / "zero.dat") != 0)
                throw std::runtime_error("Extracted 0-byte file check failed"); });
        run_case(cur_id++, "Boundary: chunk sizes (-1, exact, +1)", "Geometry", [&]()
                 {
            auto out_arch = sandbox / "chunk_edge.mage";
            auto dest = sandbox / "dest_chunk_edge";
            constexpr std::size_t k_cs = 1024 * 1024;
            std::vector<pk::crypto::format::archive_entry> entries;
            auto create_test_file = [&](const std::string &name, std::size_t sz) {
                auto fp = sandbox / name;
                std::ofstream of(fp, std::ios::binary);
                std::vector<char> buf(sz, 'X');
                of.write(buf.data(), buf.size());
                pk::crypto::format::archive_entry ae;
                ae.source_path = fp;
                ae.relative_path = name;
                ae.is_directory = false;
                ae.file_size = sz;
                entries.push_back(std::move(ae));
            };
            create_test_file("exact.dat", k_cs);
            create_test_file("minus1.dat", k_cs - 1);
            create_test_file("plus1.dat", k_cs + 1);
            pk::crypto::format::pack_archive(entries, out_arch, "ChunkPass", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 0, 0, k_cs);
            pk::crypto::format::unpack_archive(out_arch, dest, "ChunkPass", 0, 1, nullptr);
            if (std::filesystem::file_size(dest / "exact.dat") != k_cs ||
                std::filesystem::file_size(dest / "minus1.dat") != k_cs - 1 ||
                std::filesystem::file_size(dest / "plus1.dat") != k_cs + 1)
            {
                throw std::runtime_error("Chunk boundary sizes mismatch on extraction");
            } });
        run_case(cur_id++, "Boundary: unicode n' non-ASCII filenames", "Path", [&]()
                 {
            auto out_arch = sandbox / "unicode.mage";
            auto dest = sandbox / "dest_unicode";
            std::string uni_name = "archive_тест_日本語_🔒.txt";
            auto ufile = sandbox / uni_name;
            {
                std::ofstream f(ufile);
                f << "unicode path content\n";
            }
            pk::crypto::format::archive_entry ae;
            ae.source_path = ufile;
            ae.relative_path = uni_name;
            ae.is_directory = false;
            ae.file_size = std::filesystem::file_size(ufile);
            pk::crypto::format::pack_archive({ae}, out_arch, "UniPass", pk::crypto::cipher::algorithm::aes_256_gcm, fast_kdf, false, 0, 0, 1024 * 1024);
            pk::crypto::format::unpack_archive(out_arch, dest, "UniPass", 0, 1, nullptr);
            if (!std::filesystem::exists(dest / uni_name))
                throw std::runtime_error("unicode filename did not survive extraction"); });
    }
}

// end