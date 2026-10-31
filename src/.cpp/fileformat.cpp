// fileformat.cpp
// last updated: 31/10/2026
#include "../.hpp/fileformat.hpp"
#include "../.hpp/compression.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <limits>
#include "../.hpp/path_handler.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include "../.hpp/logger.hpp"
namespace pk::crypto::format
{
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
        std::function<void(uint64_t, uint64_t, const std::string &)> progress_cb,
        std::function<void(const std::string &)> status_cb,
        std::function<bool()> cancel_cb,
        std::function<void(uint64_t, uint64_t)> entry_cb)
    {
        if (entries.size() > 1000000)
            throw std::runtime_error("too many files to archive (limit: 1,000,000)");
        if (chunk_size < 1024 * 1024 || chunk_size > 64 * 1024 * 1024)
            throw std::runtime_error("invalid chunk size");

        uint64_t total_bytes = sizeof(uint64_t) + sizeof(uint32_t);
        uint64_t total_origin_size = 0;
        for (const auto &e : entries)
        {
            std::string norm_rel;
            auto path_err = pk::path::validate_archive_path(e.relative_path, norm_rel);
            if (!pk::path::ok(path_err) || e.relative_path.size() > std::numeric_limits<uint16_t>::max())
            {
                throw std::runtime_error("Invalid or malicious path in archive: " + pk::path::describe(path_err));
            }
            uint64_t entry_hdr_sz = sizeof(uint16_t) + e.relative_path.size() + sizeof(uint8_t) + sizeof(e.file_size);
            if (include_metadata)
            {
                entry_hdr_sz += sizeof(e.ctime) + sizeof(e.atime) + sizeof(e.mtime) + sizeof(e.attrs);
            }
            if (entry_hdr_sz > std::numeric_limits<uint64_t>::max() - total_bytes)
                throw std::runtime_error("archive payload size overflow");
            total_bytes += entry_hdr_sz;
            if (!e.is_directory)
            {
                if (e.file_size > std::numeric_limits<uint64_t>::max() - total_bytes ||
                    e.file_size > std::numeric_limits<uint64_t>::max() - total_origin_size)
                {
                    throw std::runtime_error("archive payload size overflow");
                }
                total_bytes += e.file_size;
                total_origin_size += e.file_size;
            }
        }
        uint64_t processed_bytes = 0;
        uint64_t completed_entries = 0;
        const uint64_t total_entries = entries.size();
        auto last_emit_time = std::chrono::steady_clock::now();
        std::string current_file;
        std::filesystem::path p_out(out_path);
        if (pk::path::is_symlink_or_other_thingy_whatever(p_out))
            throw std::runtime_error("security error -> output archive target is a symlink or reparse point");
        if (p_out.has_parent_path())
        {
            std::error_code ec;
            (void)pk::path::mk_dirs_safe(p_out.parent_path(), ec);
            if (pk::path::is_symlink_or_other_thingy_whatever(p_out.parent_path()))
                throw std::runtime_error("security error -> output directory is a symlink or reparse point");
        }
#ifdef _WIN32
        std::ofstream out(pk::path::to_ext_path(out_path), std::ios::binary | std::ios::trunc);
#else
        std::ofstream out(out_path.c_str(), std::ios::binary | std::ios::trunc);
#endif
        if (!out)
            throw std::runtime_error("failed to open output file");
        out.exceptions(std::ios::failbit | std::ios::badbit);
        bool op_success = false;
        struct cleanup_guard
        {
            const std::filesystem::path &p;
            bool &ok;
            std::ofstream &f;
            ~cleanup_guard()
            {
                if (!ok)
                {
                    if (f.is_open())
                        f.close();
                    std::error_code ec;
                    pk::path::rfs___(p, ec);
                }
            }
        } file_guard{out_path, op_success, out};
        if (pk::path::is_symlink_or_other_thingy_whatever(p_out))
            throw std::runtime_error("security error -> output archive target became a symlink or reparse point");
        auto salt_res = pk::crypto::kdf::mk_salt(16);
        if (!pk::crypto::kdf::ok(salt_res.second))
            throw std::runtime_error("salt generation failed");
        auto nonce_res = pk::crypto::kdf::mk_salt(24);
        if (!pk::crypto::kdf::ok(nonce_res.second))
        {
            pk::core::logger::log("nonce generation failed.");
            throw std::runtime_error("nonce generation failed");
        }
        pk::core::logger::log("archive header init: ALGO=" + std::to_string(static_cast<int>(algo)) + ", COMP=" + std::to_string(compression_algo) + " / " + std::to_string(compression_level) + ", CHUNK=" + std::to_string(chunk_size) + " bytes");
        header_f header{};
        std::memcpy(header.magic, MAGIC_BYTES, 4);
        header.version = 1;
        header.algo = static_cast<uint8_t>(algo);
        header.has_metadata = include_metadata ? 1 : 0;
        header.compression_algo = compression_algo;
        header.compression_level = compression_level;
        header.memory_cost_kb = kdf_cfg.memory_cost_kb;
        header.time_cost = kdf_cfg.time_cost;
        header.parallelism = static_cast<uint32_t>(kdf_cfg.parallelism);
        header.chunk_size_bytes = static_cast<uint32_t>(chunk_size);
        std::memcpy(header.salt, salt_res.first.data(), 16);
        std::memcpy(header.base_nonce, nonce_res.first.data(), 24);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        if (status_cb)
            status_cb("Deriving encryption key...");
        pk::crypto::kdf::kdf_cfg actual_kdf_cfg = kdf_cfg;
        actual_kdf_cfg.hash_length = (algo == cipher::algorithm::aes_256_siv) ? 64 : 32;
        pk::core::logger::log("deriving key... mem: " + std::to_string(actual_kdf_cfg.memory_cost_kb) + " KB, time: " + std::to_string(actual_kdf_cfg.time_cost) + ", cores: " + std::to_string(actual_kdf_cfg.parallelism));
        auto key_res = pk::crypto::kdf::derive_key(password, salt_res.first.data(), salt_res.first.size(), actual_kdf_cfg);
        if (!pk::crypto::kdf::ok(key_res.second))
        {
            pk::core::logger::log("key derivation failed.");
            throw std::runtime_error("key derivation failed.");
        }
        pk::core::logger::log("key derived successfully.");
        if (status_cb)
            status_cb("Encrypting " + std::to_string(entries.size()) + " file(s)...");
        if (entry_cb)
            entry_cb(0, total_entries);
        if (progress_cb)
            progress_cb(0, total_bytes, "");
        auto cipher = cipher::mk_cipher(algo);
        cipher->init(key_res.first.data(), key_res.first.size());
        pk::mem_::secure_wipe(key_res.first.data(), key_res.first.size());
        key_res.first.clear();
        std::vector<uint8_t> buffer(chunk_size);
        struct buf_guard
        {
            std::vector<uint8_t> &b;
            ~buf_guard()
            {
                if (!b.empty())
                {
                    pk::mem_::secure_wipe(b.data(), b.size());
                    b.clear();
                }
            }
        } b_guard{buffer};
        std::size_t buffer_pos = 0;
        uint64_t chunk_index = 0;
        const std::size_t nonce_size = cipher->nonce_size_abl();
        uint8_t current_nonce[24]{};
        std::memcpy(current_nonce, nonce_res.first.data(), nonce_size);
        std::vector<uint8_t> ad(sizeof(header) + sizeof(uint64_t));
        std::memcpy(ad.data(), &header, sizeof(header));
        auto flush_chunk = [&]()
        {
            if (buffer_pos == 0)
                return;
            std::memcpy(current_nonce, nonce_res.first.data(), nonce_size);
            for (std::size_t i = 0; i < 8 && i < nonce_size; ++i)
            {
                current_nonce[i] ^= static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }
            for (std::size_t i = 0; i < 8; ++i)
            {
                ad[sizeof(header) + i] = static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }
            auto ct = cipher->encrypt_chunk(buffer.data(), buffer_pos, ad.data(), ad.size(), current_nonce, nonce_size);
            out.write(reinterpret_cast<const char *>(ct.data()), ct.size());
            pk::core::logger::log("flushed encrypted chunk #" + std::to_string(chunk_index) + " (" + std::to_string(ct.size()) + " bytes).");
            chunk_index++;
            buffer_pos = 0;
        };
        auto write_to_encrypt_buffer = [&](const void *data, std::size_t size)
        {
            const uint8_t *ptr = static_cast<const uint8_t *>(data);
            while (size > 0)
            {
                std::size_t space = chunk_size - buffer_pos;
                std::size_t to_write = std::min(size, space);
                std::memcpy(buffer.data() + buffer_pos, ptr, to_write);
                buffer_pos += to_write;
                ptr += to_write;
                size -= to_write;
                if (buffer_pos == chunk_size)
                    flush_chunk();
            }
        };
        auto compressor = pk::crypto::cmp_e::mk_cmp_e(static_cast<pk::crypto::cmp_e::algorithm>(compression_algo), compression_level);
        auto write_stream = [&](const void *data, std::size_t size)
        {
            if (cancel_cb && cancel_cb())
                throw std::runtime_error("operation cancelled by user.");
            if (compressor)
            {
                auto compressed = compressor->cmp(data, size);
                if (!compressed.empty())
                {
                    write_to_encrypt_buffer(compressed.data(), compressed.size());
                    pk::mem_::secure_wipe(compressed.data(), compressed.size());
                }
            }
            else
            {
                write_to_encrypt_buffer(data, size);
            }
            processed_bytes += size;
            if (progress_cb || entry_cb)
            {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_emit_time).count();
                if (ms >= 50 || processed_bytes == total_bytes)
                {
                    if (entry_cb)
                        entry_cb(completed_entries, total_entries);
                    if (progress_cb)
                        progress_cb(processed_bytes, total_bytes, current_file);
                    last_emit_time = now;
                }
            }
        };
        write_stream(&total_origin_size, sizeof(total_origin_size));
        uint32_t num_entries = static_cast<uint32_t>(entries.size());
        write_stream(&num_entries, sizeof(num_entries));
        for (const auto &entry : entries)
        {
            current_file = entry.relative_path;
            if (progress_cb || entry_cb)
            {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_emit_time).count();
                if (ms >= 50)
                {
                    if (entry_cb)
                        entry_cb(completed_entries, total_entries);
                    if (progress_cb)
                        progress_cb(processed_bytes, total_bytes, current_file);
                    last_emit_time = now;
                }
            }
            uint16_t path_len = static_cast<uint16_t>(entry.relative_path.size());
            write_stream(&path_len, sizeof(path_len));
            write_stream(entry.relative_path.data(), path_len);
            uint8_t is_dir = entry.is_directory ? 1 : 0;
            uint64_t eff_file_size = entry.is_directory ? 0ULL : entry.file_size;
            write_stream(&is_dir, sizeof(is_dir));
            write_stream(&eff_file_size, sizeof(eff_file_size));
            // likewise; we permit only safe metadata
            if (header.has_metadata)
            {
                write_stream(&entry.ctime, sizeof(entry.ctime));
                write_stream(&entry.atime, sizeof(entry.atime));
                write_stream(&entry.mtime, sizeof(entry.mtime));
                write_stream(&entry.attrs, sizeof(entry.attrs));
            }
            if (!entry.is_directory && eff_file_size > 0)
            {
#ifdef _WIN32
                std::ifstream in(pk::path::to_ext_path(entry.source_path), std::ios::binary);
#else
                std::ifstream in(entry.source_path, std::ios::binary);
#endif
                if (!in)
                    throw std::runtime_error("failed to open input file: " + pk::path::display_path_utf8(entry.source_path));
                std::vector<char> file_buf(std::min(chunk_size, std::size_t(1024 * 1024)));
                struct file_buf_guard
                {
                    std::vector<char> &f;
                    ~file_buf_guard()
                    {
                        if (!f.empty())
                        {
                            pk::mem_::secure_wipe(f.data(), f.size());
                            f.clear();
                        }
                    }
                } fb_guard{file_buf};
                uint64_t remaining = eff_file_size;
                while (remaining > 0 && in)
                {
                    if (cancel_cb && cancel_cb())
                        throw std::runtime_error("operation cancelled by user.");
                    std::size_t to_read = static_cast<std::size_t>(std::min<uint64_t>(remaining, file_buf.size()));
                    in.read(file_buf.data(), to_read);
                    std::size_t bytes_read = in.gcount();
                    if (bytes_read > 0)
                    {
                        write_stream(file_buf.data(), bytes_read);
                        remaining -= bytes_read;
                    }
                    if (bytes_read < to_read)
                    {
                        break;
                    }
                }
                if (remaining > 0)
                {
                    throw std::runtime_error("failed to read complete file: " + pk::path::display_path_utf8(entry.source_path));
                }
            }
            completed_entries++;
        }
        if (compressor)
        {
            auto compressed = compressor->done();
            if (!compressed.empty())
            {
                write_to_encrypt_buffer(compressed.data(), compressed.size());
                pk::mem_::secure_wipe(compressed.data(), compressed.size());
            }
        }
        flush_chunk();
        op_success = true;
        if (entry_cb)
            entry_cb(total_entries, total_entries);
        if (progress_cb)
            progress_cb(total_bytes, total_bytes, current_file);
    }
    void unpack_archive(
        const std::filesystem::path &in_path,
        const std::filesystem::path &out_dir,
        std::string_view password,
        int ext_behavior,
        int ext_overwrite,
        std::function<int(const std::string &)> overwrite_ask_cb,
        std::function<void(uint64_t, uint64_t, const std::string &)> progress_cb,
        std::function<bool()> zipbomb_cb,
        std::function<void(const std::string &)> status_cb,
        std::function<bool()> cancel_cb,
        std::function<void(uint64_t, uint64_t)> entry_cb)
    {
        std::error_code ec_out;
        if (!pk::path::mk_dirs_safe(out_dir, ec_out))
            throw std::runtime_error("failed to create destination directory: " + ec_out.message());
        std::error_code ec_in_sz;
        auto ext_in = pk::path::to_ext_path(in_path);
        if (!std::filesystem::is_regular_file(ext_in, ec_in_sz) || ec_in_sz)
            throw std::runtime_error("failed to open input file");
        uint64_t total_bytes = std::filesystem::file_size(ext_in, ec_in_sz);
        if (ec_in_sz)
            total_bytes = 0;
#ifdef _WIN32
        std::ifstream in(ext_in, std::ios::binary);
#else
        std::ifstream in(in_path, std::ios::binary);
#endif
        if (!in)
            throw std::runtime_error("failed to open input file");
        header_f header{};
        if (!in.read(reinterpret_cast<char *>(&header), sizeof(header)))
            throw std::runtime_error("file too small for header");
        if (std::memcmp(header.magic, MAGIC_BYTES, 4) != 0)
            throw std::runtime_error("invalid magic bytes");
        if (header.version != 1)
            throw std::runtime_error("unsupported archive version");
        if (header.algo > static_cast<uint8_t>(cipher::algorithm::aes_256_siv))
            throw std::runtime_error("corrupted or malicious archive -> unknown cipher algorithm");
        if (header.compression_algo > 1)
            throw std::runtime_error("corrupted or malicious archive -> unknown compression algorithm");
        bool has_meta = (header.has_metadata == 1);
        pk::crypto::kdf::kdf_cfg kdf_cfg{};
        kdf_cfg.parallelism = header.parallelism;
        if (kdf_cfg.parallelism < 1 || kdf_cfg.parallelism > 32)
            throw std::runtime_error("corrupted or malicious archive -> invalid parallelism");
        kdf_cfg.memory_cost_kb = header.memory_cost_kb;
        if (kdf_cfg.memory_cost_kb < 1024 || kdf_cfg.memory_cost_kb > 2097152 ||
            kdf_cfg.memory_cost_kb < static_cast<uint32_t>(8 * kdf_cfg.parallelism))
            throw std::runtime_error("corrupted or malicious archive -> excessive memory cost (limit: 2 GB)");
        kdf_cfg.time_cost = header.time_cost;
        if (kdf_cfg.time_cost < 1 || kdf_cfg.time_cost > 32)
            throw std::runtime_error("corrupted or malicious archive -> excessive time cost (limit: 32)");
        std::size_t chunk_size = header.chunk_size_bytes;
        if (chunk_size < 1024 * 1024 || chunk_size > 64 * 1024 * 1024)
            throw std::runtime_error("corrupted archive -> invalid chunk size");
        kdf_cfg.hash_length = (header.algo == static_cast<uint8_t>(cipher::algorithm::aes_256_siv)) ? 64 : 32;
        if (status_cb)
            status_cb("Deriving encryption key...");
        auto key_res = pk::crypto::kdf::derive_key(password, header.salt, 16, kdf_cfg);
        if (!pk::crypto::kdf::ok(key_res.second))
            throw std::runtime_error("key derivation failed");
        if (status_cb)
            status_cb("Extracting files...");
        cipher::algorithm algo = static_cast<cipher::algorithm>(header.algo);
        auto cipher = cipher::mk_cipher(algo);
        cipher->init(key_res.first.data(), key_res.first.size());
        pk::mem_::secure_wipe(key_res.first.data(), key_res.first.size());
        key_res.first.clear();
        std::size_t block_size = chunk_size + cipher->what_mac_size();
        std::vector<uint8_t> buffer(block_size);
        pk::mem_::secure_vector plaintext_stream;
        struct unpack_buf_guard
        {
            std::vector<uint8_t> &b;
            pk::mem_::secure_vector &pt;
            ~unpack_buf_guard()
            {
                if (!b.empty())
                {
                    pk::mem_::secure_wipe(b.data(), b.size());
                    b.clear();
                }
                if (!pt.empty())
                {
                    pk::mem_::secure_wipe(pt.data(), pt.size());
                    pt.clear();
                }
            }
        } up_guard{buffer, plaintext_stream};
        uint64_t chunk_index = 0;
        std::size_t plaintext_pos = 0;
        const std::size_t nonce_size = cipher->nonce_size_abl();
        uint8_t current_nonce[24]{};
        std::vector<uint8_t> ad(sizeof(header) + sizeof(uint64_t));
        std::memcpy(ad.data(), &header, sizeof(header));
        std::string current_file;
        auto last_emit_time = std::chrono::steady_clock::now();
        auto decompressor = pk::crypto::cmp_e::mk_dmp_e(
            static_cast<pk::crypto::cmp_e::algorithm>(header.compression_algo));
        auto fetch_next_chunk = [&]()
        {
            if (cancel_cb && cancel_cb())
                throw std::runtime_error("operation cancelled by user.");
            in.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            std::size_t bytes_read = in.gcount();
            if (bytes_read == 0)
                throw std::runtime_error("unexpected EOF");
            if (bytes_read <= cipher->what_mac_size())
                throw std::runtime_error("corrupted chunk -> too small");
            // reset to base; THEN xor it
            std::memcpy(current_nonce, header.base_nonce, nonce_size);
            for (std::size_t i = 0; i < 8 && i < nonce_size; ++i)
            {
                current_nonce[i] ^= static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }
            for (std::size_t i = 0; i < 8; ++i)
            {
                ad[sizeof(header) + i] = static_cast<uint8_t>((chunk_index >> (i * 8)) & 0xFF);
            }
            auto pt = cipher->decrypt_chunk(buffer.data(), bytes_read, ad.data(), ad.size(), current_nonce, nonce_size);
            plaintext_stream = std::move(pt);
            plaintext_pos = 0;
            chunk_index++;
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
        uint64_t total_origin_size = 0;
        read_from_stream(&total_origin_size, sizeof(total_origin_size));
        if (total_bytes < 184467440737095516ULL && total_origin_size > total_bytes * 100 && total_origin_size > 1024ULL * 1024 * 1024)
        {
            if (!zipbomb_cb || !zipbomb_cb())
            {
                throw std::runtime_error("possible zipbomb detected");
            }
        }
        uint64_t aoz = 0;
        uint64_t extracted_bytes = 0;
        uint32_t num_entries = 0;
        read_from_stream(&num_entries, sizeof(num_entries));
        if (num_entries > 1000000)
        {
            throw std::runtime_error("corrupted archive or too many files (limit: 1,000,000)");
        }
        auto emit_unpack_progress = [&](bool force, uint32_t entry_idx)
        {
            if (!progress_cb && !entry_cb)
                return;
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_emit_time).count();
            if (force || ms >= 50)
            {
                if (entry_cb)
                {
                    entry_cb(std::min<uint64_t>(entry_idx, num_entries), num_entries);
                }
                if (progress_cb)
                {
                    if (total_origin_size > 0)
                    {
                        progress_cb(std::min(extracted_bytes, total_origin_size), total_origin_size, current_file);
                    }
                    else if (num_entries > 0)
                    {
                        progress_cb(std::min<uint64_t>(entry_idx, num_entries), num_entries, current_file);
                    }
                }
                last_emit_time = now;
            }
        };
        emit_unpack_progress(true, 0);
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            uint16_t path_len = 0;
            read_from_stream(&path_len, sizeof(path_len));
            std::string rel_path(path_len, '\0');
            read_from_stream(rel_path.data(), path_len);
            std::string norm_path;
            if (!pk::path::ok(pk::path::validate_archive_path(rel_path, norm_path)))
            {
                throw std::runtime_error("Invalid or malicious path in archive: " + rel_path);
            }
            current_file = norm_path;
            emit_unpack_progress(false, i);
            uint8_t is_dir = 0;
            read_from_stream(&is_dir, sizeof(is_dir));
            uint64_t file_size = 0;
            read_from_stream(&file_size, sizeof(file_size));
            if (is_dir != 0 && file_size != 0)
            {
                throw std::runtime_error("corrupted archive -> directory entry declares non-zero file size");
            }
            if (!is_dir)
            {
                if (file_size > total_origin_size - aoz)
                {
                    throw std::runtime_error("archive corruption or zipbomb bypass; entries size exceeds declared total size");
                }
                aoz += file_size;
            }
            uint64_t ctime = 0, atime = 0, mtime = 0;
            uint32_t attrs = 0;
            if (has_meta)
            {
                read_from_stream(&ctime, sizeof(ctime));
                read_from_stream(&atime, sizeof(atime));
                read_from_stream(&mtime, sizeof(mtime));
                read_from_stream(&attrs, sizeof(attrs));
            }
            if (ext_behavior == 1)
            {
                std::size_t pos = norm_path.find('/');
                if (pos != std::string::npos)
                {
                    norm_path = norm_path.substr(pos + 1);
                }
                else if (is_dir)
                {
                    continue;
                }
                if (norm_path.empty())
                {
                    if (!is_dir && file_size > 0)
                    {
                        uint64_t rem_skip = file_size;
                        std::vector<char> drain_buf(std::min(chunk_size, std::size_t(1024 * 1024)));
                        struct drain_guard
                        {
                            std::vector<char> &d;
                            ~drain_guard()
                            {
                                if (!d.empty())
                                    pk::mem_::secure_wipe(d.data(), d.size());
                            }
                        } dg{drain_buf};
                        while (rem_skip > 0)
                        {
                            if (cancel_cb && cancel_cb())
                                throw std::runtime_error("operation cancelled by user.");
                            std::size_t n = static_cast<std::size_t>(std::min<uint64_t>(rem_skip, drain_buf.size()));
                            read_from_stream(drain_buf.data(), n);
                            rem_skip -= n;
                            extracted_bytes += n;
                        }
                    }
                    continue;
                }
            }
            std::filesystem::path target_path;
            std::error_code fs_error;
            auto v_err = pk::path::resolve_out_path(out_dir, norm_path, target_path, fs_error);
            if (!pk::path::ok(v_err))
            {
                if (v_err == pk::path::validation_error::path_conflict)
                {
                    throw std::runtime_error("path conflict: '" + norm_path + "' is blocked by an existing file.");
                }
                throw std::runtime_error("archive path escapes the output directory -> " + norm_path);
            }
            if (is_dir)
            {
                if (pk::path::is_symlink_or_other_thingy_whatever(target_path))
                {
                    throw std::runtime_error("security error -> cannot create directory because a symlink or reparse point exists: " + norm_path);
                }
                if (std::filesystem::exists(pk::path::to_ext_path(target_path)) && !std::filesystem::is_directory(pk::path::to_ext_path(target_path)))
                {
                    throw std::runtime_error("path conflict -> cannot create directory '" + norm_path + "' because a file with the same name exists.");
                }
                std::error_code ec_mkdir;
                if (!pk::path::mk_dirs_safe(target_path, ec_mkdir))
                    throw std::runtime_error("failed to create destination directory: " + ec_mkdir.message());
            }
            else
            {
                std::error_code ec_exist, ec_isdir;
                if (std::filesystem::exists(pk::path::to_ext_path(target_path), ec_exist))
                {
                    if (std::filesystem::is_directory(pk::path::to_ext_path(target_path), ec_isdir))
                    {
                        throw std::runtime_error("path conflict -> cannot create file '" + norm_path + "' because a directory with the same name exists.");
                    }
                    int action = ext_overwrite;
                    if (action == 0 && overwrite_ask_cb)
                    {
                        action = overwrite_ask_cb(pk::path::display_path_utf8(target_path));
                    }
                    if (action == 0)
                    {
                        throw std::runtime_error("extraction cancelled by user due to file conflict.");
                    }
                    else if (action == 1)
                    {
                        uint64_t remaining = file_size;
                        std::vector<char> dummy(std::min(chunk_size, std::size_t(1024 * 1024)));
                        struct skip_buf_guard
                        {
                            std::vector<char> &d;
                            ~skip_buf_guard()
                            {
                                if (!d.empty())
                                    pk::mem_::secure_wipe(d.data(), d.size());
                            }
                        } sb_guard{dummy};
                        while (remaining > 0)
                        {
                            if (cancel_cb && cancel_cb())
                                throw std::runtime_error("operation cancelled by user.");
                            std::size_t to_read = static_cast<std::size_t>(std::min<uint64_t>(remaining, dummy.size()));
                            read_from_stream(dummy.data(), to_read);
                            remaining -= to_read;
                            extracted_bytes += to_read;
                            emit_unpack_progress(false, i);
                        }
                        continue;
                    }
                }
                if (pk::path::is_symlink_or_other_thingy_whatever(target_path))
                {
                    throw std::runtime_error("security error -> target file is a symlink or reparse point; " + norm_path);
                }
                std::error_code ec_parent;
                if (!pk::path::mk_dirs_safe(target_path.parent_path(), ec_parent))
                    throw std::runtime_error("failed to create destination directory: " + ec_parent.message());
                if (pk::path::is_symlink_or_other_thingy_whatever(target_path.parent_path()))
                {
                    throw std::runtime_error("security error -> parent directory is a symlink or reparse point; " + norm_path);
                }
#ifdef _WIN32
                std::error_code ec_ex;
                if (std::filesystem::exists(pk::path::to_ext_path(target_path), ec_ex))
                {
                    SetFileAttributesW(pk::path::to_ext_win32_path(target_path).c_str(), FILE_ATTRIBUTE_NORMAL);
                }
                std::ofstream out(pk::path::to_ext_path(target_path), std::ios::binary | std::ios::trunc);
#else
                std::error_code ec_ex;
                if (std::filesystem::exists(target_path, ec_ex))
                {
                    (void)::chmod(target_path.c_str(), 0644);
                }
                std::ofstream out(target_path, std::ios::binary | std::ios::trunc);
#endif
                if (!out)
                    throw std::runtime_error("failed to create output file: " + pk::path::display_path_utf8(target_path));
                if (pk::path::is_symlink_or_other_thingy_whatever(target_path))
                {
                    out.close();
                    std::error_code ec_rm;
                    pk::path::rfs___(target_path, ec_rm);
                    throw std::runtime_error("security error -> target file is a symlink or reparse point after open; " + norm_path);
                }
                out.exceptions(std::ios::failbit | std::ios::badbit);
                uint64_t remaining = file_size;
                std::vector<char> write_buf(std::min(chunk_size, std::size_t(1024 * 1024)));
                struct write_buf_guard
                {
                    std::vector<char> &w;
                    ~write_buf_guard()
                    {
                        if (!w.empty())
                        {
                            pk::mem_::secure_wipe(w.data(), w.size());
                            w.clear();
                        }
                    }
                } wb_guard{write_buf};
                while (remaining > 0)
                {
                    if (cancel_cb && cancel_cb())
                        throw std::runtime_error("operation cancelled by user.");
                    std::size_t to_write = static_cast<std::size_t>(std::min<uint64_t>(remaining, write_buf.size()));
                    read_from_stream(write_buf.data(), to_write);
                    out.write(write_buf.data(), to_write);
                    remaining -= to_write;
                    extracted_bytes += to_write;
                    emit_unpack_progress(false, i);
                }
                out.close(); // q
            }
#ifdef _WIN32
            if (has_meta)
            {
                std::wstring wpath = pk::path::to_ext_win32_path(target_path);
                DWORD open_flags = FILE_FLAG_OPEN_REPARSE_POINT | (is_dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL);
                HANDLE hFile = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, open_flags, NULL);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    BY_HANDLE_FILE_INFORMATION bhfi{};
                    if (GetFileInformationByHandle(hFile, &bhfi) && (bhfi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                    {
                        FILETIME ftc, fta, ftw;
                        ftc.dwLowDateTime = (DWORD)(ctime & 0xFFFFFFFF);
                        ftc.dwHighDateTime = (DWORD)(ctime >> 32);
                        fta.dwLowDateTime = (DWORD)(atime & 0xFFFFFFFF);
                        fta.dwHighDateTime = (DWORD)(atime >> 32);
                        ftw.dwLowDateTime = (DWORD)(mtime & 0xFFFFFFFF);
                        ftw.dwHighDateTime = (DWORD)(mtime >> 32);
                        SetFileTime(hFile, (ctime > 0 ? &ftc : nullptr), (atime > 0 ? &fta : nullptr), (mtime > 0 ? &ftw : nullptr));
                    }
                    CloseHandle(hFile);
                }
                if (!pk::path::is_symlink_or_other_thingy_whatever(target_path))
                {
                    DWORD safe_attrs = attrs & (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
                    if (!is_dir)
                    {
                        if (safe_attrs == 0)
                            safe_attrs = FILE_ATTRIBUTE_NORMAL;
                        SetFileAttributesW(wpath.c_str(), safe_attrs);
                    }
                    else if (safe_attrs != 0)
                    {
                        SetFileAttributesW(wpath.c_str(), safe_attrs);
                    }
                }
            }
#else
            // random guy on an old ass forum said this shit works,
            // he better not be lyin'
            if (has_meta && !pk::path::is_symlink_or_other_thingy_whatever(target_path))
            {
                constexpr uint64_t k_win_epoch_diff = 116444736000000000ULL;
                auto ft2timespec = [k_win_epoch_diff](uint64_t ft) -> struct timespec
                {
                    struct timespec ts{};
                    if (ft == 0)
                    {
                        ts.tv_nsec = UTIME_OMIT;
                        return ts;
                    }
                    if (ft < k_win_epoch_diff)
                    {
                        ts.tv_sec = 0;
                        ts.tv_nsec = 0;
                        return ts;
                    }
                    uint64_t diff = ft - k_win_epoch_diff;
                    ts.tv_sec = static_cast<time_t>(diff / 10000000ULL);
                    ts.tv_nsec = static_cast<long>((diff % 10000000ULL) * 100ULL);
                    return ts;
                };
                struct timespec times[2] = {ft2timespec(atime), ft2timespec(mtime)};
                utimensat(AT_FDCWD, target_path.c_str(), times, AT_SYMLINK_NOFOLLOW);
                if (!is_dir && ((attrs & 1) || (attrs > 0xFF && !(attrs & 0222))))
                {
                    int fd = ::open(target_path.c_str(), O_RDONLY
#ifdef O_NOFOLLOW
                                                             | O_NOFOLLOW
#endif
                    );
                    if (fd >= 0)
                    {
                        (void)::fchmod(fd, 0444);
                        ::close(fd);
                    }
                }
            }
#endif
        }
        if (aoz != total_origin_size)
        {
            throw std::runtime_error("archive corruption -> sum of entry file sizes does not match declared total size");
        }
        while (in && in.peek() != EOF)
        {
            fetch_next_chunk();
            if (decompressor && plaintext_pos < plaintext_stream.size())
            {
                std::size_t avail = plaintext_stream.size() - plaintext_pos;
                decompressor->feed(plaintext_stream.data() + plaintext_pos, avail);
                plaintext_pos += avail;
            }
        }
        if (decompressor)
        {
            if (plaintext_pos < plaintext_stream.size())
            {
                std::size_t avail = plaintext_stream.size() - plaintext_pos;
                decompressor->feed(plaintext_stream.data() + plaintext_pos, avail);
                plaintext_pos += avail;
            }
            decompressor->finish_chk();
        }
        else if (plaintext_pos < plaintext_stream.size())
        {
            throw std::runtime_error("corrupted archive -> unexpected trailing plaintext bytes");
        }
        if (ext_behavior == 2)
        {
            std::filesystem::path archive_stem = in_path.stem();
            std::filesystem::path wrapper_dir = out_dir / archive_stem;
            std::error_code ec;
            if (!pk::path::is_symlink_or_other_thingy_whatever(wrapper_dir) &&
                std::filesystem::exists(pk::path::to_ext_path(wrapper_dir), ec) &&
                std::filesystem::is_directory(pk::path::to_ext_path(wrapper_dir), ec))
            {
#ifdef _WIN32
                std::wstring search_pat = pk::path::to_ext_win32_path(wrapper_dir);
                if (!search_pat.empty() && search_pat.back() != L'\\')
                    search_pat.push_back(L'\\');
                search_pat.push_back(L'*');
                WIN32_FIND_DATAW fdata{};
                HANDLE hFind = FindFirstFileW(search_pat.c_str(), &fdata);
                std::size_t child_count = 0;
                std::wstring single_name;
                bool single_is_dir = false;
                if (hFind != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        if (fdata.cFileName[0] == L'.' &&
                            (fdata.cFileName[1] == L'\0' || (fdata.cFileName[1] == L'.' && fdata.cFileName[2] == L'\0')))
                        {
                            continue;
                        }
                        child_count++;
                        if (child_count == 1)
                        {
                            single_name = fdata.cFileName;
                            single_is_dir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                                            (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
                        }
                        else
                        {
                            break;
                        }
                    } while (FindNextFileW(hFind, &fdata));
                    FindClose(hFind);
                }
                if (child_count == 1 && single_is_dir)
                {
                    std::filesystem::path single_dir = wrapper_dir / single_name;
                    std::filesystem::path dest_dir = out_dir / single_name;
                    if (!pk::path::is_symlink_or_other_thingy_whatever(single_dir) &&
                        !std::filesystem::exists(pk::path::to_ext_path(dest_dir), ec))
                    {
                        if (MoveFileExW(pk::path::to_ext_win32_path(single_dir).c_str(), pk::path::to_ext_win32_path(dest_dir).c_str(), 0))
                        {
                            RemoveDirectoryW(pk::path::to_ext_win32_path(wrapper_dir).c_str());
                        }
                    }
                }
#else
                std::vector<std::filesystem::directory_entry> sub_items;
                for (const auto &dir_entry : std::filesystem::directory_iterator(wrapper_dir, ec))
                {
                    sub_items.push_back(dir_entry);
                }
                if (sub_items.size() == 1 && !sub_items[0].is_symlink(ec) && sub_items[0].is_directory(ec))
                {
                    std::filesystem::path single_dir = sub_items[0].path();
                    std::filesystem::path dest_dir = out_dir / single_dir.filename();
                    if (!std::filesystem::exists(dest_dir, ec))
                    {
                        std::filesystem::rename(single_dir, dest_dir, ec);
                        if (!ec)
                        {
                            std::filesystem::remove(wrapper_dir, ec);
                        }
                    }
                }
#endif
            }
        }
        extracted_bytes = total_origin_size;
        emit_unpack_progress(true, num_entries);
    }
}

// end