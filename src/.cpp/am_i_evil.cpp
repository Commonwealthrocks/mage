// am_i_evil.cpp
// last updated: 04/10/2026
// am i really that evil?
#include "../.hpp/am_i_evil.hpp"
#include "../.hpp/path_handler.hpp"
#include "../.hpp/compression.hpp"
#include "../.hpp/logger.hpp"
#include "../.hpp/secure_memory.hpp"
#include <fstream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <sodium.h>
namespace pk::crypto::am_i_evil
{
    std::string numbers_with_commas_unlike_in_gta_5(uint64_t val)
    {
        std::string s = std::to_string(val);
        int n = static_cast<int>(s.length()) - 3;
        while (n > 0)
        {
            s.insert(n, ",");
            n -= 3;
        }
        return s;
    }
    std::string format_bytes(uint64_t bytes)
    {
        if (bytes < 1024)
        {
            return numbers_with_commas_unlike_in_gta_5(bytes) + "B";
        }
        double val = 0.0;
        std::string unit;
        if (bytes < 1024ULL * 1024)
        {
            val = bytes / 1024.0;
            unit = "KB";
        }
        else if (bytes < 1024ULL * 1024 * 1024)
        {
            val = bytes / (1024.0 * 1024.0);
            unit = "MB";
        }
        else if (bytes < 1024ULL * 1024 * 1024 * 1024)
        {
            val = bytes / (1024.0 * 1024.0 * 1024.0);
            unit = "GB";
        }
        else
        {
            val = bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0);
            unit = "TB";
        }
        char buf[64];
        double rounded = std::round(val * 100.0) / 100.0;
        if (std::abs(rounded - 1.0) < 0.0001)
        {
            return "1" + unit;
        }
        if (std::abs(val - std::round(val)) < 0.001)
        {
            uint64_t int_val = static_cast<uint64_t>(std::round(val));
            std::snprintf(buf, sizeof(buf), "%llu%ss", static_cast<unsigned long long>(int_val), unit.c_str());
            return std::string(buf);
        }
        std::snprintf(buf, sizeof(buf), "%.2f%ss", val, unit.c_str());
        return std::string(buf);
    }
    std::string verdict_to_str(__vv_ v)
    {
        switch (v)
        {
        case __vv_::success:
            return "authentic n' verified (OK)";
        case __vv_::wrong_password:
            return "wrong password / authentication failed";
        case __vv_::tampered_chunk:
            return "tampered / corrupted chunk (bit rot or data tampering)";
        case __vv_::header_but_not_ok:
            return "invalid / corrupted MAGE header";
        case __vv_::header_dos_trap:
            return "malicious / dangerous KDF parameters (DoS trap?)";
        case __vv_::stream_corrupted:
            return "corrupted compressed payload stream";
        case __vv_::zipbomb_warning:
            return "potential zipbomb";
        case __vv_::traversal_attack:
            return "path traversal / malicious relative path detected";
        case __vv_::io_error:
        default:
            return "I/O or read error";
        }
    }
    int calculate_tab_width(const std::string &header, const std::vector<std::string> &entries, int default_min_width, int min_side_padding)
    {
        int max_len = static_cast<int>(header.length());
        for (const auto &e : entries)
        {
            max_len = std::max(max_len, static_cast<int>(e.length()));
        }
        int needed_width = max_len + (min_side_padding * 2);
        return std::max(default_min_width, needed_width);
    }
    std::string center_text(const std::string &text, int width)
    {
        int len = static_cast<int>(text.length());
        if (len >= width)
        {
            return text;
        }
        int total_padding = width - len;
        int left_padding = total_padding / 2;
        int right_padding = total_padding - left_padding;
        return std::string(left_padding, ' ') + text + std::string(right_padding, ' ');
    }
    static std::string algo_name_str(uint8_t id)
    {
        switch (id)
        {
        case 0:
            return "AES-256-GCM";
        case 1:
            return "XChaCha20-Poly1305";
        case 2:
            return "AES-256-SIV";
        default:
            return "Unknown (" + std::to_string(id) + ")";
        }
    }
    static std::string cmp_name_str(uint8_t id)
    {
        switch (id)
        {
        case 0:
            return "none (store)";
        case 1:
            return "Zstandard";
        case 2:
            return "LZMA2";
        default:
            return "unknown (" + std::to_string(id) + ")";
        }
    }
    header_report view_header(const std::filesystem::path &archive_path)
    {
        header_report rep;
        rep.___filepath = archive_path;
        std::error_code ec;
        if (!std::filesystem::exists(archive_path, ec) || std::filesystem::is_directory(archive_path, ec))
        {
            rep.file_exists = false;
            rep.sanity_notes = "target file does not exist or is a directory.";
            return rep;
        }
        rep.file_exists = true;
        rep.file_size_bytes = std::filesystem::file_size(archive_path, ec);
        if (ec)
        {
            rep.sanity_notes = "failed to determine file size: " + ec.message();
            return rep;
        }
        if (rep.file_size_bytes < sizeof(pk::crypto::format::header_f))
        {
            rep.structure_ok = false;
            rep.sanity_notes = "file is truncated (smaller than MAGE header: " + std::to_string(rep.file_size_bytes) + " < " + std::to_string(sizeof(pk::crypto::format::header_f)) + " bytes).";
            return rep;
        }
        std::ifstream in(archive_path, std::ios::binary);
        if (!in)
        {
            rep.sanity_notes = "failed to open archive file for reading.";
            return rep;
        }
        pk::crypto::format::header_f hdr{};
        in.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
        if (in.gcount() != sizeof(hdr))
        {
            rep.sanity_notes = "failed to read complete header structure.";
            return rep;
        }
        if (std::memcmp(hdr.magic, pk::crypto::format::MAGIC_BYTES, 4) != 0)
        {
            rep.valid_magic = false;
            rep.structure_ok = false;
            rep.kdf_safe = true;
            rep.sanity_notes = "invalid magic bytes (not a MAGE archive).";
            return rep;
        }
        if (hdr.version != 1)
        {
            rep.valid_magic = false;
            rep.structure_ok = false;
            rep.kdf_safe = true;
            if (hdr.version >= 32 && hdr.version <= 126)
            {
                rep.sanity_notes = "invalid container format; plain text or non-archive file detected (not a binary MAGE container).";
            }
            else
            {
                rep.sanity_notes = "unsupported container format version (version " + std::to_string(static_cast<int>(hdr.version)) + ", expected version 1).";
            }
            return rep;
        }
        if (hdr.algo > 2 && hdr.compression_algo > 2)
        {
            rep.valid_magic = false;
            rep.structure_ok = false;
            rep.kdf_safe = true;
            rep.sanity_notes = "corrupted container header; unrecognized cipher and compression identifiers.";
            return rep;
        }
        rep.valid_magic = true;
        rep.version = hdr.version;
        rep.algo_id = hdr.algo;
        rep.algo_name = algo_name_str(hdr.algo);
        rep.has_metadata = (hdr.has_metadata != 0);
        rep.compression_algo_id = hdr.compression_algo;
        rep.compression_algo_name = cmp_name_str(hdr.compression_algo);
        rep.compression_level = hdr.compression_level;
        rep.memory_cost_kb = hdr.memory_cost_kb;
        rep.time_cost = hdr.time_cost;
        rep.parallelism = hdr.parallelism;
        rep.chunk_size = hdr.chunk_size_bytes;
        std::memcpy(rep.salt, hdr.salt, 16);
        std::memcpy(rep.base_nonce, hdr.base_nonce, 24);
        bool dos_detected = false;
        if (hdr.memory_cost_kb > 2 * 1024 * 1024)
        {
            dos_detected = true;
            rep.sanity_notes += "WARN (memory exceeds 2GBs) (" + format_bytes(static_cast<uint64_t>(hdr.memory_cost_kb) * 1024) + "). ";
        }
        if (hdr.time_cost > 32)
        {
            dos_detected = true;
            rep.sanity_notes += "WARN (time cost exceeds 32) -> (" + std::to_string(hdr.time_cost) + " passes). ";
        }
        if (hdr.parallelism < 1 || hdr.parallelism > 32)
        {
            dos_detected = true;
            rep.sanity_notes += "WARN (parallelism are one too many) -> (" + std::to_string(hdr.parallelism) + " threads). ";
        }
        rep.kdf_safe = !dos_detected;
        if (hdr.algo > 2)
        {
            rep.structure_ok = false;
            rep.sanity_notes += "unrecognized cipher algorithm ID (" + std::to_string(hdr.algo) + "). ";
        }
        if (hdr.compression_algo > 2)
        {
            rep.structure_ok = false;
            rep.sanity_notes += "unrecognized compression algorithm ID (" + std::to_string(hdr.compression_algo) + "). ";
        }
        if (hdr.chunk_size_bytes < 1024 * 1024 || hdr.chunk_size_bytes > 64 * 1024 * 1024)
        {
            rep.structure_ok = false;
            rep.sanity_notes += "chunk size out of bounds (1MB - 64MBs required). ";
        }
        else
        {
            const uint64_t payload_bytes = rep.file_size_bytes - sizeof(pk::crypto::format::header_f);
            const uint64_t chunk_payload = hdr.chunk_size_bytes + 16; // 16-byte MAC tag
            rep.est_chunks = (payload_bytes + chunk_payload - 1) / chunk_payload;
            rep.structure_ok = !dos_detected;
        }
        if (rep.sanity_notes.empty())
        {
            rep.sanity_notes = "container header structure and parameters are standard and valid.";
        }

        return rep;
    }
    verification_report verify_archive(
        const std::filesystem::path &archive_path,
        std::string_view password,
        std::function<void(uint64_t processed_bytes, uint64_t total_bytes, const std::string &status)> progress_cb,
        std::function<bool()> cancel_cb)
    {
        verification_report rep;
        rep.header = view_header(archive_path);
        if (!rep.header.file_exists)
        {
            rep.verdict = __vv_::io_error;
            rep.error_details = rep.header.sanity_notes;
            return rep;
        }
        if (!rep.header.valid_magic)
        {
            rep.verdict = __vv_::header_but_not_ok;
            rep.error_details = rep.header.sanity_notes;
            return rep;
        }
        if (!rep.header.kdf_safe)
        {
            rep.verdict = __vv_::header_dos_trap;
            rep.error_details = rep.header.sanity_notes;
            return rep;
        }
        if (!rep.header.structure_ok)
        {
            rep.verdict = __vv_::header_but_not_ok;
            rep.error_details = rep.header.sanity_notes;
            return rep;
        }
        const uint64_t total_bytes = rep.header.file_size_bytes;
        uint64_t processed_bytes = sizeof(pk::crypto::format::header_f);
        if (progress_cb)
            progress_cb(processed_bytes, total_bytes, "Deriving encryption key...");
        pk::crypto::kdf::kdf_cfg kdf_cfg;
        kdf_cfg.memory_cost_kb = rep.header.memory_cost_kb;
        kdf_cfg.time_cost = rep.header.time_cost;
        kdf_cfg.parallelism = rep.header.parallelism;
        kdf_cfg.hash_length = (rep.header.algo_id == static_cast<uint8_t>(cipher::algorithm::aes_256_siv)) ? 64 : 32;
        auto key_res = pk::crypto::kdf::derive_key(password, rep.header.salt, 16, kdf_cfg);
        if (!pk::crypto::kdf::ok(key_res.second))
        {
            rep.verdict = __vv_::wrong_password;
            rep.error_details = "Key derivation failed.";
            return rep;
        }
        std::ifstream in(archive_path, std::ios::binary);
        if (!in)
        {
            pk::mem_::secure_wipe(key_res.first.data(), key_res.first.size());
            key_res.first.clear();
            rep.verdict = __vv_::io_error;
            rep.error_details = "Could not open archive for decryption.";
            return rep;
        }
        pk::crypto::format::header_f header{};
        in.read(reinterpret_cast<char *>(&header), sizeof(header));
        auto cipher = cipher::mk_cipher(static_cast<cipher::algorithm>(rep.header.algo_id));
        cipher->init(key_res.first.data(), key_res.first.size());
        // Securely wipe master key material from RAM immediately after cipher initialization
        pk::mem_::secure_wipe(key_res.first.data(), key_res.first.size());
        key_res.first.clear();

        const std::size_t chunk_size = rep.header.chunk_size;
        const std::size_t block_size = chunk_size + cipher->what_mac_size();
        std::vector<uint8_t> buffer(block_size);
        uint64_t chunk_index = 0;
        pk::mem_::secure_vector plaintext_stream;
        std::size_t plaintext_pos = 0;
        const std::size_t nonce_size = cipher->nonce_size_abl();
        uint8_t current_nonce[24]{};
        std::vector<uint8_t> ad(sizeof(header) + sizeof(uint64_t));
        std::memcpy(ad.data(), &header, sizeof(header));
        auto decompressor = pk::crypto::cmp_e::mk_dmp_e(
            static_cast<pk::crypto::cmp_e::algorithm>(rep.header.compression_algo_id));

        auto cleanup_sensitive = [&]()
        {
            if (!plaintext_stream.empty())
            {
                pk::mem_::secure_wipe(plaintext_stream.data(), plaintext_stream.size());
                plaintext_stream.clear();
            }
            if (!buffer.empty())
            {
                pk::mem_::secure_wipe(buffer.data(), buffer.size());
                buffer.clear();
            }
        };

        auto fetch_next_chunk = [&]()
        {
            if (cancel_cb && cancel_cb())
                throw std::runtime_error("verification cancelled by user.");

            in.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            std::size_t bytes_read = in.gcount();
            if (bytes_read == 0)
            {
                rep.failed_chunk_index = chunk_index;
                rep.verdict = __vv_::stream_corrupted;
                rep.error_details = "unexpected EOF encountered while reading chunk #" + std::to_string(chunk_index) + ".";
                throw std::runtime_error(rep.error_details);
            }
            if (bytes_read <= cipher->what_mac_size())
            {
                rep.failed_chunk_index = chunk_index;
                rep.verdict = __vv_::tampered_chunk;
                rep.error_details = "corrupted or truncated chunk #" + std::to_string(chunk_index) + " (" + std::to_string(bytes_read) + " bytes); chunk too small for AEAD tag.";
                throw std::runtime_error(rep.error_details);
            }
            std::memcpy(current_nonce, header.base_nonce, nonce_size);
            for (std::size_t i = 0; i < 8 && i < nonce_size; ++i)
            {
                current_nonce[i] ^= static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }
            for (std::size_t i = 0; i < 8; ++i)
            {
                ad[sizeof(header) + i] = static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }

            try
            {
                plaintext_stream = cipher->decrypt_chunk(buffer.data(), bytes_read, ad.data(), ad.size(), current_nonce, nonce_size);
                plaintext_pos = 0;
                chunk_index++;
                rep.verified_chunks++;
                processed_bytes += bytes_read;
                if (progress_cb)
                {
                    progress_cb(processed_bytes, total_bytes, "Verifying chunk #" + std::to_string(chunk_index) + "...");
                }
            }
            catch (const std::exception &e)
            {
                rep.failed_chunk_index = chunk_index;
                if (chunk_index == 0)
                {
                    rep.verdict = __vv_::wrong_password;
                    rep.error_details = "AEAD authentication tag rejected at chunk #0 (wrong password or corrupted header).";
                }
                else
                {
                    rep.verdict = __vv_::tampered_chunk;
                    rep.error_details = "AEAD authentication tag mismatch at chunk #" + std::to_string(chunk_index) + " (offset ~" + format_bytes(processed_bytes) + "); file is corrupted or tampered.";
                }
                throw;
            }
        };
        auto read_from_stream = [&](void *out_data, std::size_t size)
        {
            uint8_t *ptr = static_cast<uint8_t *>(out_data);
            if (decompressor)
            {
                while (size > 0)
                {
                    std::size_t bytes_read = decompressor->read(ptr, size);
                    ptr += bytes_read;
                    size -= bytes_read;
                    if (size > 0)
                    {
                        if (plaintext_pos >= plaintext_stream.size())
                            fetch_next_chunk();
                        std::size_t avail = plaintext_stream.size() - plaintext_pos;
                        decompressor->feed(plaintext_stream.data() + plaintext_pos, avail);
                        plaintext_pos += avail;
                    }
                }
            }
            else
            {
                while (size > 0)
                {
                    if (plaintext_pos >= plaintext_stream.size())
                        fetch_next_chunk();
                    std::size_t avail = plaintext_stream.size() - plaintext_pos;
                    std::size_t to_read = std::min(size, avail);
                    std::memcpy(ptr, plaintext_stream.data() + plaintext_pos, to_read);
                    ptr += to_read;
                    plaintext_pos += to_read;
                    size -= to_read;
                }
            }
        };
        try
        {
            uint64_t total_origin_size = 0;
            read_from_stream(&total_origin_size, sizeof(total_origin_size));
            rep.dub = total_origin_size;

            if (total_origin_size > total_bytes * 100 && total_origin_size > 1024ULL * 1024 * 1024)
            {
                rep.verdict = __vv_::zipbomb_warning;
                rep.error_details = "uncompressed size (" + format_bytes(total_origin_size) + ") exceeds archive on disk size by >100x (perhaps zipbomb?).";
                cleanup_sensitive();
                return rep;
            }
            uint32_t num_entries = 0;
            read_from_stream(&num_entries, sizeof(num_entries));
            if (num_entries > 1000000)
            {
                rep.verdict = __vv_::stream_corrupted;
                rep.error_details = "excessive entry count declared (" + std::to_string(num_entries) + "); archive is invalid.";
                cleanup_sensitive();
                return rep;
            }
            rep.total_entries = num_entries;
            uint64_t accumulated_origin_bytes = 0;
            std::vector<char> discard_buf(1024 * 1024);
            struct buf_cleaner
            {
                std::vector<char> &buf;
                ~buf_cleaner()
                {
                    if (!buf.empty())
                    {
                        pk::mem_::secure_wipe(buf.data(), buf.size());
                    }
                }
            } cleaner{discard_buf};

            for (uint32_t i = 0; i < num_entries; ++i)
            {
                uint16_t path_len = 0;
                read_from_stream(&path_len, sizeof(path_len));
                std::string rel_path(path_len, '\0');
                read_from_stream(rel_path.data(), path_len);
                std::string norm_path;
                if (!pk::path::ok(pk::path::validate_archive_path(rel_path, norm_path)))
                {
                    rep.verdict = __vv_::traversal_attack;
                    rep.error_details = "malicious or illegal relative path detected: " + rel_path;
                    cleanup_sensitive();
                    return rep;
                }
                if (rep.entry_sample.size() < 10)
                {
                    rep.entry_sample.push_back(norm_path);
                }
                uint8_t is_dir = 0;
                read_from_stream(&is_dir, sizeof(is_dir));
                uint64_t file_size = 0;
                read_from_stream(&file_size, sizeof(file_size));
                if (is_dir)
                {
                    rep.directories++;
                }
                else
                {
                    rep.regular_files++;
                    accumulated_origin_bytes += file_size;
                }
                if (accumulated_origin_bytes > total_origin_size)
                {
                    rep.verdict = __vv_::stream_corrupted;
                    rep.error_details = "sum of entry file sizes exceeds declared total origin size.";
                    cleanup_sensitive();
                    return rep;
                }
                if (rep.header.has_metadata)
                {
                    uint64_t ctime = 0, atime = 0, mtime = 0;
                    uint32_t attrs = 0;
                    read_from_stream(&ctime, sizeof(ctime));
                    read_from_stream(&atime, sizeof(atime));
                    read_from_stream(&mtime, sizeof(mtime));
                    read_from_stream(&attrs, sizeof(attrs));
                }
                if (!is_dir && file_size > 0)
                {
                    uint64_t rem = file_size;
                    while (rem > 0)
                    {
                        if (cancel_cb && cancel_cb())
                            throw std::runtime_error("verification cancelled by user.");
                        std::size_t to_drain = static_cast<std::size_t>(std::min<uint64_t>(rem, discard_buf.size()));
                        read_from_stream(discard_buf.data(), to_drain);
                        rem -= to_drain;
                    }
                }
            }
            rep.actual_uncompressed_bytes = accumulated_origin_bytes;
            rep.total_chunks = chunk_index;
            while (in && in.peek() != EOF)
            {
                fetch_next_chunk();
            }
            rep.total_chunks = chunk_index;
            if (rep.dub > 0)
            {
                rep.compression_ratio_percent =
                    (static_cast<double>(rep.header.file_size_bytes) / static_cast<double>(rep.dub)) * 100.0;
            }
            rep.verdict = __vv_::success;
            rep.error_details.clear();
        }
        catch (const std::exception &e)
        {
            if (rep.verdict == __vv_::io_error)
            {
                std::string what_str = e.what();
                if (what_str.find("cancelled") != std::string::npos)
                {
                    rep.error_details = "verification cancelled by user.";
                }
                else
                {
                    rep.error_details = e.what();
                }
            }
        }

        cleanup_sensitive();
        return rep;
    }
    std::string mk_export_log(const std::vector<verification_report> &reports)
    {
        std::ostringstream log;
        time_t rawtime;
        struct tm *timeinfo;
        char timebuf[80];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timebuf, sizeof(timebuf), "%d. %B %Y, %H:%M", timeinfo);
        log << "MAGE Archive Verification v0.6a (build " << __DATE__ << ")\n";
        log << "Verification log file from " << timebuf << "\n\n";
        log << "[!] NOTICE\n";
        log << "    This diagnostic report discloses container geometry, cryptographic parameters,\n";
        log << "    and archive metadata. Do not publish or disclose this log to untrusted parties.\n\n";
        uint32_t total = static_cast<uint32_t>(reports.size());
        uint32_t header_ok = 0;
        uint32_t header_warn = 0;
        uint32_t aead_verified = 0;
        uint32_t aead_failed = 0;
        uint32_t aead_pending = 0;
        for (const auto &r : reports)
        {
            if (r.header.valid_magic && r.header.kdf_safe && r.header.structure_ok)
                header_ok++;
            else
                header_warn++;

            if (r.verdict == __vv_::success)
                aead_verified++;
            else if (r.verdict == __vv_::io_error && r.verified_chunks == 0 && r.error_details.empty())
                aead_pending++;
            else
                aead_failed++;
        }
        auto pad_field = [](const std::string &field, const std::string &val) -> std::string
        {
            std::ostringstream s;
            s << std::left << std::setw(38) << (field + ":") << val << "\n";
            return s.str();
        };
        log << pad_field("Total archives in queue", std::to_string(total));
        log << pad_field("Header pre-checks", std::to_string(header_ok) + " OK, " + std::to_string(header_warn) + " warning / issues");
        log << pad_field("AEAD integrity", std::to_string(aead_verified) + " OK, " + std::to_string(aead_failed) + " failed, " + std::to_string(aead_pending) + " pending");
        log << "\n";
        struct table_row_data
        {
            std::string item_str;
            std::string size_str;
            std::string cipher_str;
            std::string comp_str;
            std::string hdr_status;
            std::string aead_status;
        };
        std::vector<table_row_data> table_rows;
        table_rows.reserve(reports.size());
        std::vector<std::string> item_entries;
        std::vector<std::string> size_entries;
        std::vector<std::string> cipher_entries;
        std::vector<std::string> comp_entries;
        std::vector<std::string> hdr_entries;
        std::vector<std::string> aead_entries;
        for (std::size_t i = 0; i < reports.size(); ++i)
        {
            const auto &r = reports[i];
            table_row_data row;
            row.item_str = std::to_string(i + 1);
            row.size_str = format_bytes(r.header.file_size_bytes);
            row.cipher_str = (!r.header.valid_magic || r.header.algo_name.empty()) ? "-" : r.header.algo_name;
            std::string comp_str = "-";
            if (r.header.valid_magic)
            {
                comp_str = r.header.compression_algo_name.empty() ? "None (Store)" : r.header.compression_algo_name;
                if (comp_str == "none" || comp_str == "store" || comp_str == "None")
                    comp_str = "None (Store)";
                else if (r.header.compression_algo_id > 0)
                    comp_str += " (level " + std::to_string(static_cast<int>(r.header.compression_level)) + ")";
            }
            row.comp_str = comp_str;
            row.hdr_status = (r.header.valid_magic && r.header.structure_ok && r.header.kdf_safe) ? "OK" : "FAIL";
            if (r.verdict == __vv_::success)
                row.aead_status = "OK";
            else if (r.verdict == __vv_::io_error && r.verified_chunks == 0 && r.error_details.empty())
                row.aead_status = "PENDING";
            else
                row.aead_status = "FAILED";
            table_rows.push_back(row);
            item_entries.push_back(row.item_str);
            size_entries.push_back(row.size_str);
            cipher_entries.push_back(row.cipher_str);
            comp_entries.push_back(row.comp_str);
            hdr_entries.push_back(row.hdr_status);
            aead_entries.push_back(row.aead_status);
        }
        int w_item = calculate_tab_width("Item", item_entries, 6, 1);
        int w_size = calculate_tab_width("Size", size_entries, 12, 1);
        int w_cipher = calculate_tab_width("Cipher", cipher_entries, 22, 2);
        int w_comp = calculate_tab_width("Compression", comp_entries, 19, 2);
        int w_hdr = calculate_tab_width("Header", hdr_entries, 17, 2);
        int w_aead = calculate_tab_width("AEAD integrity", aead_entries, 18, 2);
        int total_inner_w = w_item + 1 + w_size + 1 + w_cipher + 1 + w_comp + 1 + w_hdr + 1 + w_aead;
        std::string border_line = "    |" + std::string(total_inner_w, '-') + "|\n";
        log << "Overall queue summary:\n";
        log << border_line;
        log << "    |" << center_text("Item", w_item)
            << "|" << center_text("Size", w_size)
            << "|" << center_text("Cipher", w_cipher)
            << "|" << center_text("Compression", w_comp)
            << "|" << center_text("Header", w_hdr)
            << "|" << center_text("AEAD integrity", w_aead)
            << "|\n";
        log << border_line;
        for (const auto &row : table_rows)
        {
            log << "    |" << center_text(row.item_str, w_item)
                << "|" << center_text(row.size_str, w_size)
                << "|" << center_text(row.cipher_str, w_cipher)
                << "|" << center_text(row.comp_str, w_comp)
                << "|" << center_text(row.hdr_status, w_hdr)
                << "|" << center_text(row.aead_status, w_aead)
                << "|\n";
        }
        log << border_line << "\n";
        bool has_errors = false;
        for (std::size_t i = 0; i < reports.size(); ++i)
        {
            const auto &r = reports[i];
            log << "Archive " << (i + 1) << ": " << r.header.___filepath.string() << "\n";
            auto pad_item = [](const std::string &field, const std::string &val) -> std::string
            {
                std::ostringstream s;
                s << "     " << std::left << std::setw(33) << (field + ":") << val << "\n";
                return s.str();
            };
            log << pad_item("File size", numbers_with_commas_unlike_in_gta_5(r.header.file_size_bytes) + "B (" + format_bytes(r.header.file_size_bytes) + ")");
            log << pad_item("Format signature", r.header.valid_magic ? "MAGE (OK)" : "INVALID");
            log << pad_item("Format version", r.header.valid_magic ? std::to_string(static_cast<int>(r.header.version)) : "-");
            log << pad_item("Cipher algorithm", (!r.header.valid_magic || r.header.algo_name.empty()) ? "-" : r.header.algo_name);
            std::string comp_info = "-";
            if (r.header.valid_magic)
            {
                comp_info = r.header.compression_algo_name.empty() ? "None (Store)" : r.header.compression_algo_name;
                if (comp_info == "none" || comp_info == "store" || comp_info == "None")
                    comp_info = "None (Store)";
                else if (r.header.compression_algo_id > 0)
                    comp_info += " (level " + std::to_string(static_cast<int>(r.header.compression_level)) + ")";
            }
            log << pad_item("Compression", comp_info);
            log << pad_item("Preserves metadata", !r.header.valid_magic ? "-" : (r.header.has_metadata ? "yes" : "no"));
            log << pad_item("Chunk geometry", !r.header.valid_magic ? "-" : (format_bytes(r.header.chunk_size) + " (" + std::to_string(r.header.est_chunks) + " chunk" + (r.header.est_chunks == 1 ? "" : "s") + " estimated)"));
            std::string kdf_params = "memory: " + format_bytes(static_cast<uint64_t>(r.header.memory_cost_kb) * 1024) + ", passes: " + std::to_string(r.header.time_cost) + ", cores: " + std::to_string(r.header.parallelism);
            log << pad_item("KDF algorithm", !r.header.valid_magic ? "-" : "Argon2id");
            log << pad_item("KDF parameters", !r.header.valid_magic ? "-" : kdf_params);
            log << pad_item("KDF DoS safety", !r.header.valid_magic ? "N/A" : (r.header.kdf_safe ? "OK (no limits exceeded)" : "FAIL (safety threshold exceeded)"));
            log << pad_item("Container health", (r.header.structure_ok && r.header.valid_magic) ? "OK" : r.header.sanity_notes);
            if (!r.header.valid_magic || !r.header.structure_ok || !r.header.kdf_safe)
                has_errors = true;
            log << "\n";
            bool header_failed = (!r.header.file_exists || !r.header.valid_magic || !r.header.structure_ok || !r.header.kdf_safe);
            if (header_failed)
            {
                std::string v_str = "FAIL (" + verdict_to_str(r.verdict == __vv_::io_error ? (r.header.kdf_safe ? __vv_::header_but_not_ok : __vv_::header_dos_trap) : r.verdict) + ")";
                log << pad_item("Verdict", v_str);
                log << pad_item("Chunks verified", "0 / 0");
                log << pad_item("Failure reason", r.error_details.empty() ? r.header.sanity_notes : r.error_details);
                log << pad_item("Status", "FAIL (integrity check failed)");
                has_errors = true;
            }
            else if (r.verdict != __vv_::io_error || r.verified_chunks > 0 || !r.error_details.empty())
            {
                if (r.verdict == __vv_::success)
                {
                    log << pad_item("Verdict", "OK (100% integrity)");
                    log << pad_item("Chunks verified", std::to_string(r.verified_chunks) + " / " + std::to_string(r.total_chunks));
                    log << pad_item("Declared original", numbers_with_commas_unlike_in_gta_5(r.dub) + "B (" + format_bytes(r.dub) + ")");
                    char comp_buf[64];
                    snprintf(comp_buf, sizeof(comp_buf), "%.1f%% (%.1f%% savings)", r.compression_ratio_percent, 100.0 - r.compression_ratio_percent);
                    log << pad_item("Compression ratio", std::string(comp_buf));
                    log << pad_item("Payload entries", std::to_string(r.regular_files) + " files, " + std::to_string(r.directories) + " directories");
                    log << pad_item("Path traversal check", "OK (no path traversal detected)");
                    log << pad_item("Status", "OK (integrity passes)");
                }
                else
                {
                    log << pad_item("Verdict", "FAIL (" + verdict_to_str(r.verdict) + ")");
                    log << pad_item("Chunks verified", std::to_string(r.verified_chunks) + " / " + std::to_string(r.total_chunks));
                    if (!r.error_details.empty())
                        log << pad_item("Failure reason", r.error_details);
                    log << pad_item("Status", "FAIL (integrity check failed)");
                    has_errors = true;
                }
            }
            else
            {
                log << pad_item("Verdict", "Not run (no password / keyfile entered)");
                log << pad_item("Status", "PENDING (parameter check only)");
            }
            log << "\n";
        }
        if (has_errors)
        {
            log << "Errors occurred during archive verification\n";
        }
        else if (aead_verified == total && total > 0)
        {
            log << "All archives verified successfully\nno errors occurred\n";
        }
        else
        {
            log << "All container headers inspected successfully\nno errors occurred\n";
        }
        log << "end of status report.\n\n";
        std::string final_log = log.str();
        uint8_t hash[crypto_hash_sha256_BYTES];
        crypto_hash_sha256(hash, reinterpret_cast<const unsigned char *>(final_log.data()), final_log.size());
        char hash_str[crypto_hash_sha256_BYTES * 2 + 1];
        for (std::size_t i = 0; i < crypto_hash_sha256_BYTES; ++i)
        {
            snprintf(&hash_str[i * 2], 3, "%02X", hash[i]);
        }
        final_log += "==== Log checksum (SHA-256): " + std::string(hash_str) + " ====";
        return final_log;
    }
}

// end