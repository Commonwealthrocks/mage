// fileformat.hpp
// last updated: 17/06/2026
#pragma once
#include "__cipher.hpp"
#include "argon2id_hashing.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
namespace pk::crypto::format
{
    constexpr char MAGIC_BYTES[4] = {'M', 'A', 'G', 'E'};
#pragma pack(push, 1)
    struct header_f
    {
        char magic[4];
        uint8_t version;
        uint8_t algo;
        uint8_t has_metadata;
        uint8_t compression_algo;
        int8_t compression_level;
        uint32_t memory_cost_kb;
        uint32_t time_cost;
        uint32_t parallelism;
        uint8_t salt[16];
        uint8_t base_nonce[24];
        uint32_t chunk_size_bytes;
    };
#pragma pack(pop)
    struct archive_entry
    {
        std::filesystem::path source_path;
        std::string relative_path;
        bool is_directory;
        uint64_t file_size;

        uint64_t ctime = 0;
        uint64_t atime = 0;
        uint64_t mtime = 0;
        uint32_t attrs = 0;
    };
    void pack_archive(
        const std::vector<archive_entry> &entries,
        const std::filesystem::path &out_path,
        std::string_view password,
        cipher::algorithm algo,
        const pk::crypto::kdf::kdf_cfg &kdf_cfg,
        bool include_metadata,
        uint8_t compression_algo,
        int8_t compression_level,
        std::size_t chunk_size,
        std::function<void(uint64_t bytes_processed, uint64_t total_bytes)> progress_cb = nullptr);
    void unpack_archive(
        const std::filesystem::path &in_path,
        const std::filesystem::path &out_dir,
        std::string_view password,
        std::function<void(uint64_t bytes_processed, uint64_t total_bytes)> progress_cb = nullptr,
        std::function<bool()> zipbomb_cb = nullptr);
}

// end