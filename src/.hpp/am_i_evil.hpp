// am_i_evil.hpp
// last updated: 04/10/2026
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "fileformat.hpp"
namespace pk::crypto::am_i_evil
{
    struct header_report
    {
        std::filesystem::path ___filepath;
        uint64_t file_size_bytes = 0;
        bool file_exists = false;
        bool valid_magic = false;
        uint8_t version = 0;
        uint8_t algo_id = 0;
        std::string algo_name;
        uint32_t chunk_size = 0;
        uint8_t salt[16]{};
        uint8_t base_nonce[24]{};
        uint32_t memory_cost_kb = 0;
        uint32_t time_cost = 0;
        uint32_t parallelism = 0;
        uint8_t compression_algo_id = 0;
        std::string compression_algo_name;
        int8_t compression_level = 0;
        bool has_metadata = false;
        uint64_t est_chunks = 0;
        bool kdf_safe = false;
        bool structure_ok = false;
        std::string sanity_notes;
    };
    enum class __vv_
    {
        success,
        wrong_password,
        tampered_chunk,
        header_but_not_ok,
        header_dos_trap,
        stream_corrupted,
        zipbomb_warning,
        traversal_attack,
        io_error
    };
    struct verification_report
    {
        header_report header;
        __vv_ verdict = __vv_::io_error;
        std::string error_details;
        uint64_t total_chunks = 0;
        uint64_t verified_chunks = 0;
        uint64_t failed_chunk_index = 0;
        uint64_t dub = 0;
        uint64_t actual_uncompressed_bytes = 0;
        uint32_t total_entries = 0;
        uint32_t regular_files = 0;
        uint32_t directories = 0;
        double compression_ratio_percent = 0.0;
        std::vector<std::string> entry_sample;
    };
    header_report view_header(const std::filesystem::path &archive_path);
    verification_report verify_archive(
        const std::filesystem::path &archive_path,
        std::string_view password,
        std::function<void(uint64_t processed_bytes, uint64_t total_bytes, const std::string &status)> progress_cb = nullptr,
        std::function<bool()> cancel_cb = nullptr);
    std::string verdict_to_str(__vv_ v);
    std::string format_bytes(uint64_t bytes);
    std::string numbers_with_commas_unlike_in_gta_5(uint64_t val);
    int calculate_tab_width(const std::string &header, const std::vector<std::string> &entries, int default_min_width, int min_side_padding = 2);
    std::string center_text(const std::string &text, int width);
    std::string mk_export_log(const std::vector<verification_report> &reports);
}

// end