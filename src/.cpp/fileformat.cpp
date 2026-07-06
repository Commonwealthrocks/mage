// fileformat.cpp
// last updated: 06/07/2026
#include "../.hpp/fileformat.hpp"
#include "../.hpp/compression.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <chrono>
#include "../.hpp/path_handler.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
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
        std::function<void(const std::string &)> status_cb)
    {
        uint64_t total_bytes = sizeof(uint64_t) + sizeof(uint32_t);
        for (const auto &e : entries)
        {
            total_bytes += sizeof(uint16_t) + e.relative_path.size() + sizeof(uint8_t) + sizeof(e.file_size);
            if (include_metadata)
            {
                total_bytes += sizeof(e.ctime) + sizeof(e.atime) + sizeof(e.mtime) + sizeof(e.attrs);
            }
            if (!e.is_directory)
                total_bytes += e.file_size;
        }
        uint64_t processed_bytes = 0;
        auto last_emit_time = std::chrono::steady_clock::now();
        std::string current_file;
        std::filesystem::path p_out(out_path);
        if (p_out.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(p_out.parent_path(), ec);
        }
        std::ofstream out(out_path.c_str(), std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("failed to open output file");
        out.exceptions(std::ios::failbit | std::ios::badbit);
        auto salt_res = pk::crypto::kdf::mk_salt(16);
        if (!pk::crypto::kdf::ok(salt_res.second))
            throw std::runtime_error("salt generation failed");
        auto nonce_res = pk::crypto::kdf::mk_salt(24);
        if (!pk::crypto::kdf::ok(nonce_res.second))
            throw std::runtime_error("nonce generation failed");
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
            status_cb("Deriving encryption key (Argon2id)...");
        auto key_res = pk::crypto::kdf::derive_key(password, salt_res.first.data(), salt_res.first.size(), kdf_cfg);
        if (!pk::crypto::kdf::ok(key_res.second))
            throw std::runtime_error("key derivation failed");
        if (status_cb)
            status_cb("Encrypting " + std::to_string(entries.size()) + " files...");
        auto cipher = cipher::mk_cipher(algo);
        cipher->init(key_res.first.data(), key_res.first.size());
        std::vector<uint8_t> buffer(chunk_size);
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
            std::memcpy(ad.data() + sizeof(header), &chunk_index, sizeof(uint64_t));
            std::vector<uint8_t> ciphertext = cipher->encrypt_chunk(buffer.data(), buffer_pos, ad.data(), ad.size(), current_nonce, nonce_size);
            out.write(reinterpret_cast<const char *>(ciphertext.data()), ciphertext.size());
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
            if (compressor)
            {
                auto compressed = compressor->cmp(data, size);
                if (!compressed.empty())
                {
                    write_to_encrypt_buffer(compressed.data(), compressed.size());
                }
            }
            else
            {
                write_to_encrypt_buffer(data, size);
            }
            processed_bytes += size;
            if (progress_cb)
            {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_emit_time).count();
                if (ms >= 100 || processed_bytes == total_bytes)
                {
                    progress_cb(processed_bytes, total_bytes, current_file);
                    last_emit_time = now;
                }
            }
        };
        uint64_t total_origin_size = 0;
        for (const auto &entry : entries)
        {
            total_origin_size += entry.file_size;
        }
        write_stream(&total_origin_size, sizeof(total_origin_size));
        uint32_t num_entries = static_cast<uint32_t>(entries.size());
        write_stream(&num_entries, sizeof(num_entries));
        for (const auto &entry : entries)
        {
            current_file = entry.relative_path;
            if (progress_cb)
            {
                progress_cb(processed_bytes, total_bytes, current_file);
            }
            uint16_t path_len = static_cast<uint16_t>(entry.relative_path.size());
            write_stream(&path_len, sizeof(path_len));
            write_stream(entry.relative_path.data(), path_len);
            uint8_t is_dir = entry.is_directory ? 1 : 0;
            write_stream(&is_dir, sizeof(is_dir));
            write_stream(&entry.file_size, sizeof(entry.file_size));
            // likewise; we permit only safe metadata
            if (header.has_metadata)
            {
                write_stream(&entry.ctime, sizeof(entry.ctime));
                write_stream(&entry.atime, sizeof(entry.atime));
                write_stream(&entry.mtime, sizeof(entry.mtime));
                write_stream(&entry.attrs, sizeof(entry.attrs));
            }
            if (!entry.is_directory && entry.file_size > 0)
            {
                std::ifstream in(entry.source_path, std::ios::binary);
                if (!in)
                    throw std::runtime_error("failed to open input file: " + entry.source_path.string());
                std::vector<char> file_buf(std::min(chunk_size, std::size_t(1024 * 1024)));
                uint64_t remaining = entry.file_size;
                while (remaining > 0 && in)
                {
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
                    std::vector<char> zeros(std::min<uint64_t>(remaining, 1024 * 1024), 0);
                    while (remaining > 0)
                    {
                        std::size_t to_write = static_cast<std::size_t>(std::min<uint64_t>(remaining, zeros.size()));
                        write_stream(zeros.data(), to_write);
                        remaining -= to_write;
                    }
                }
            }
        }
        if (compressor)
        {
            auto compressed = compressor->done();
            if (!compressed.empty())
            {
                write_to_encrypt_buffer(compressed.data(), compressed.size());
            }
        }
        flush_chunk();
        if (progress_cb)
            progress_cb(total_bytes, total_bytes, current_file);
    }
    void unpack_archive(
        const std::filesystem::path &in_path,
        const std::filesystem::path &out_dir,
        std::string_view password,
        std::function<void(uint64_t, uint64_t, const std::string &)> progress_cb,
        std::function<bool()> zipbomb_cb,
        std::function<void(const std::string &)> status_cb)
    {
        uint64_t total_bytes = std::filesystem::file_size(in_path);
        uint64_t processed_bytes = 0;

        std::ifstream in(in_path, std::ios::binary);
        if (!in)
            throw std::runtime_error("failed to open input file");
        header_f header{};
        if (!in.read(reinterpret_cast<char *>(&header), sizeof(header)))
            throw std::runtime_error("file too small for header");
        if (std::memcmp(header.magic, MAGIC_BYTES, 4) != 0)
            throw std::runtime_error("invalid magic bytes");
        if (header.version != 1)
            throw std::runtime_error("unsupported archive version");
        bool has_meta = (header.has_metadata == 1);
        pk::crypto::kdf::kdf_cfg kdf_cfg{};
        kdf_cfg.memory_cost_kb = header.memory_cost_kb;
        if (kdf_cfg.memory_cost_kb < 1024 || kdf_cfg.memory_cost_kb > 4194304)
            throw std::runtime_error("corrupted archive -> invalid mem_ cost");
        kdf_cfg.time_cost = header.time_cost;
        if (kdf_cfg.time_cost < 1 || kdf_cfg.time_cost > 255)
            throw std::runtime_error("corrupted archive -> invalid time cost");
        kdf_cfg.parallelism = header.parallelism;
        if (kdf_cfg.parallelism < 1 || kdf_cfg.parallelism > 64)
            throw std::runtime_error("corrupted archive -> invalid parallelism");
        kdf_cfg.hash_length = 32;
        if (status_cb)
            status_cb("Deriving encryption key (Argon2id)...");
        auto key_res = pk::crypto::kdf::derive_key(password, header.salt, 16, kdf_cfg);
        if (!pk::crypto::kdf::ok(key_res.second))
            throw std::runtime_error("key derivation failed");
        if (status_cb)
            status_cb("Extracting files...");
        std::size_t chunk_size = header.chunk_size_bytes;
        if (chunk_size < 1024 * 1024 || chunk_size > 64 * 1024 * 1024)
            throw std::runtime_error("corrupted archive -> invalid chunk size");
        cipher::algorithm algo = static_cast<cipher::algorithm>(header.algo);
        auto cipher = cipher::mk_cipher(algo);
        cipher->init(key_res.first.data(), key_res.first.size());
        std::size_t block_size = chunk_size + cipher->what_mac_size();
        std::vector<uint8_t> buffer(block_size);
        uint64_t chunk_index = 0;
        std::vector<uint8_t> plaintext_stream;
        std::size_t plaintext_pos = 0;
        const std::size_t nonce_size = cipher->nonce_size_abl();
        uint8_t current_nonce[24]{};
        std::vector<uint8_t> ad(sizeof(header) + sizeof(uint64_t));
        std::memcpy(ad.data(), &header, sizeof(header));
        std::string current_file;
        auto decompressor = pk::crypto::cmp_e::mk_dmp_e(
            static_cast<pk::crypto::cmp_e::algorithm>(header.compression_algo));
        auto fetch_next_chunk = [&]()
        {
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
            std::memcpy(ad.data() + sizeof(header), &chunk_index, sizeof(uint64_t));
            auto pt = cipher->decrypt_chunk(buffer.data(), bytes_read, ad.data(), ad.size(), current_nonce, nonce_size);
            plaintext_stream.assign(pt.begin(), pt.end());
            plaintext_pos = 0;
            chunk_index++;
            processed_bytes += bytes_read;
            if (progress_cb)
                progress_cb(processed_bytes, total_bytes, current_file);
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
        if (total_origin_size > total_bytes * 100 && total_origin_size > 1024ULL * 1024 * 1024)
        {
            if (!zipbomb_cb || !zipbomb_cb())
            {
                throw std::runtime_error("possible zipbomb detected");
            }
        }
        uint32_t num_entries = 0;
        read_from_stream(&num_entries, sizeof(num_entries));
        if (num_entries > 1000000)
        {
            throw std::runtime_error("corrupted archive or too many files (limit: 1,000,000)");
        }
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            uint16_t path_len = 0;
            read_from_stream(&path_len, sizeof(path_len));
            std::string rel_path(path_len, '\0');
            read_from_stream(rel_path.data(), path_len);
            current_file = rel_path;
            if (progress_cb)
            {
                progress_cb(processed_bytes, total_bytes, current_file);
            }
            uint8_t is_dir = 0;
            read_from_stream(&is_dir, sizeof(is_dir));
            uint64_t file_size = 0;
            read_from_stream(&file_size, sizeof(file_size));
            uint64_t ctime = 0, atime = 0, mtime = 0;
            uint32_t attrs = 0;
            if (has_meta)
            {
                read_from_stream(&ctime, sizeof(ctime));
                read_from_stream(&atime, sizeof(atime));
                read_from_stream(&mtime, sizeof(mtime));
                read_from_stream(&attrs, sizeof(attrs));
            }
            std::filesystem::path target_path;
            std::error_code fs_error;
            std::string norm_path;
            if (!pk::path::ok(pk::path::validate_archive_path(rel_path, norm_path)))
            {
                throw std::runtime_error("Invalid or malicious path in archive: " + rel_path);
            }
            auto v_err = pk::path::resolve_out_path(out_dir.string(), norm_path, target_path, fs_error);
            if (!pk::path::ok(v_err))
            {
                if (v_err == pk::path::validation_error::path_conflict)
                {
                    throw std::runtime_error("path conflict: '" + rel_path + "' is blocked by an existing file.");
                }
                throw std::runtime_error("archive path escapes the output directory -> " + rel_path);
            }
            if (is_dir)
            {
                if (std::filesystem::exists(target_path) && !std::filesystem::is_directory(target_path))
                {
                    throw std::runtime_error("path conflict -> cannot create directory '" + rel_path + "' because a file with the same name exists.");
                }
                std::filesystem::create_directories(target_path);
            }
            else
            {
                if (std::filesystem::exists(target_path) && std::filesystem::is_directory(target_path))
                {
                    throw std::runtime_error("path conflict -> cannot create file '" + rel_path + "' because a directory with the same name exists.");
                }
                std::filesystem::create_directories(target_path.parent_path());
                std::ofstream out(target_path, std::ios::binary | std::ios::trunc);
                if (!out)
                    throw std::runtime_error("failed to create output file: " + target_path.string());
                out.exceptions(std::ios::failbit | std::ios::badbit);
                uint64_t remaining = file_size;
                std::vector<char> write_buf(std::min(chunk_size, std::size_t(1024 * 1024)));
                while (remaining > 0)
                {
                    std::size_t to_write = static_cast<std::size_t>(std::min<uint64_t>(remaining, write_buf.size()));
                    read_from_stream(write_buf.data(), to_write);
                    out.write(write_buf.data(), to_write);
                    remaining -= to_write;
                }
                out.close(); // q
            }
#ifdef _WIN32
            if (has_meta)
            {
                std::wstring wpath = target_path.wstring();
                HANDLE hFile = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, is_dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    FILETIME ftc, fta, ftw;
                    ftc.dwLowDateTime = (DWORD)(ctime & 0xFFFFFFFF);
                    ftc.dwHighDateTime = (DWORD)(ctime >> 32);
                    fta.dwLowDateTime = (DWORD)(atime & 0xFFFFFFFF);
                    fta.dwHighDateTime = (DWORD)(atime >> 32);
                    ftw.dwLowDateTime = (DWORD)(mtime & 0xFFFFFFFF);
                    ftw.dwHighDateTime = (DWORD)(mtime >> 32);
                    SetFileTime(hFile, &ftc, &fta, &ftw);
                    CloseHandle(hFile);
                }
                DWORD safe_attrs = attrs & ~(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY);
                if (safe_attrs == 0)
                {
                    safe_attrs = FILE_ATTRIBUTE_NORMAL;
                }
                SetFileAttributesW(wpath.c_str(), safe_attrs);
            }
#endif
        }
    }
}

// end