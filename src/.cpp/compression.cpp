// cmp_e.cpp
// last updated: 17/06/2026
#include "../.hpp/compression.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <zstd.h>
#include <lzma.h>
namespace pk::crypto::cmp_e
{
    // zstd falls under meta's bsd 3-clause license
    // pacman -S mingw-w64-ucrt-x86_64-zstd
    class zstd_compressor : public stream_cmp
    {
        ZSTD_CCtx *ctx;
        std::vector<uint8_t> out_buffer;

    public:
        zstd_compressor(int level)
        {
            ctx = ZSTD_createCCtx();
            if (!ctx)
                throw std::runtime_error("failed to create ZSTD CCtx");
            ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
            out_buffer.resize(ZSTD_CStreamOutSize());
        }
        ~zstd_compressor() override
        {
            if (ctx)
                ZSTD_freeCCtx(ctx);
        }
        std::vector<uint8_t> cmp(const void *data, std::size_t size) override
        {
            std::vector<uint8_t> result;
            result.reserve(ZSTD_compressBound(size)); // avoid repeated reallocation calls
            ZSTD_inBuffer input = {data, size, 0};
            while (input.pos < input.size)
            {
                ZSTD_outBuffer output = {out_buffer.data(), out_buffer.size(), 0};
                std::size_t ret = ZSTD_compressStream2(ctx, &output, &input, ZSTD_e_continue);
                if (ZSTD_isError(ret))
                    throw std::runtime_error("ZSTD cmp_e failed");
                if (output.pos > 0)
                {
                    result.insert(result.end(), out_buffer.begin(), out_buffer.begin() + output.pos);
                }
            }
            return result;
        }
        std::vector<uint8_t> done() override
        {
            std::vector<uint8_t> result;
            ZSTD_inBuffer input = {nullptr, 0, 0};
            std::size_t ret;
            do
            {
                ZSTD_outBuffer output = {out_buffer.data(), out_buffer.size(), 0};
                ret = ZSTD_compressStream2(ctx, &output, &input, ZSTD_e_end);
                if (ZSTD_isError(ret))
                    throw std::runtime_error("ZSTD finalization failed");
                if (output.pos > 0)
                {
                    result.insert(result.end(), out_buffer.begin(), out_buffer.begin() + output.pos);
                }
            } while (ret > 0);
            return result;
        }
    };
    class zstd_decompressor : public stream_dcmp
    {
        ZSTD_DCtx *ctx;
        std::vector<uint8_t> in_buffer;
        std::size_t in_start = 0;
        std::size_t in_end = 0;
        std::vector<uint8_t> out_buffer;
        std::size_t out_pos = 0;
        std::size_t out_avail = 0;

    public:
        zstd_decompressor()
        {
            ctx = ZSTD_createDCtx();
            if (!ctx)
                throw std::runtime_error("failed to create ZSTD DCtx");
            out_buffer.resize(ZSTD_DStreamOutSize());
            in_buffer.resize(256 * 1024);
            in_end = 0;
        }
        ~zstd_decompressor() override
        {
            if (ctx)
                ZSTD_freeDCtx(ctx);
        }
        void feed(const void *data, std::size_t size) override
        {
            const uint8_t *ptr = static_cast<const uint8_t *>(data);
            if (in_start > 0 && in_start > in_buffer.size() / 2)
            {
                std::size_t remaining = in_end - in_start;
                std::memmove(in_buffer.data(), in_buffer.data() + in_start, remaining);
                in_start = 0;
                in_end = remaining;
            }
            if (in_end + size > in_buffer.size())
            {
                in_buffer.resize(in_end + size);
            }
            std::memcpy(in_buffer.data() + in_end, ptr, size);
            in_end += size;
        }
        std::size_t read(void *out_data, std::size_t size) override
        {
            uint8_t *ptr = static_cast<uint8_t *>(out_data);
            std::size_t total_read = 0;
            while (total_read < size)
            {
                if (out_avail > 0)
                {
                    std::size_t to_copy = std::min(size - total_read, out_avail);
                    std::memcpy(ptr + total_read, out_buffer.data() + out_pos, to_copy);
                    total_read += to_copy;
                    out_pos += to_copy;
                    out_avail -= to_copy;
                    if (total_read == size)
                        return total_read;
                }
                if (in_start >= in_end)
                {
                    break;
                }
                ZSTD_inBuffer input = {in_buffer.data() + in_start, in_end - in_start, 0};
                ZSTD_outBuffer output = {out_buffer.data(), out_buffer.size(), 0};
                std::size_t ret = ZSTD_decompressStream(ctx, &output, &input);
                if (ZSTD_isError(ret))
                    throw std::runtime_error("ZSTD decompression failed");
                in_start += input.pos;
                out_pos = 0;
                out_avail = output.pos;
            }
            return total_read;
        }
    };
    // liblzma is provided under the 0bsd clause license
    // liblzma comes pre-installed with msys2 (ucrt64)
    class lzma_compressor : public stream_cmp
    {
        lzma_stream strm = LZMA_STREAM_INIT;
        std::vector<uint8_t> out_buffer;

    public:
        lzma_compressor(int level)
        {
            lzma_ret ret = lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);
            if (ret != LZMA_OK)
                throw std::runtime_error("failed to initialize LZMA encoder");
            out_buffer.resize(256 * 1024);
        }
        ~lzma_compressor() override
        {
            lzma_end(&strm);
        }
        std::vector<uint8_t> cmp(const void *data, std::size_t size) override
        {
            std::vector<uint8_t> result;
            result.reserve(size);
            strm.next_in = static_cast<const uint8_t *>(data);
            strm.avail_in = size;
            while (strm.avail_in > 0)
            {
                strm.next_out = out_buffer.data();
                strm.avail_out = out_buffer.size();

                lzma_ret ret = lzma_code(&strm, LZMA_RUN);
                if (ret != LZMA_OK && ret != LZMA_STREAM_END)
                    throw std::runtime_error("LZMA cmp_e failed");
                std::size_t have = out_buffer.size() - strm.avail_out;
                if (have > 0)
                {
                    result.insert(result.end(), out_buffer.begin(), out_buffer.begin() + have);
                }
            }
            return result;
        }
        std::vector<uint8_t> done() override
        {
            std::vector<uint8_t> result;
            strm.next_in = nullptr;
            strm.avail_in = 0;
            lzma_ret ret;
            do
            {
                strm.next_out = out_buffer.data();
                strm.avail_out = out_buffer.size();

                ret = lzma_code(&strm, LZMA_FINISH);
                if (ret != LZMA_OK && ret != LZMA_STREAM_END)
                    throw std::runtime_error("LZMA finalization failed");
                std::size_t have = out_buffer.size() - strm.avail_out;
                if (have > 0)
                {
                    result.insert(result.end(), out_buffer.begin(), out_buffer.begin() + have);
                }
            } while (ret != LZMA_STREAM_END);
            return result;
        }
    };
    class lzma_decompressor : public stream_dcmp
    {
        lzma_stream strm = LZMA_STREAM_INIT;
        std::vector<uint8_t> in_buffer;
        std::size_t in_start = 0; // Issue #3: track consumed region
        std::size_t in_end = 0;
        std::vector<uint8_t> out_buffer;
        std::size_t out_pos = 0;
        std::size_t out_avail = 0;

    public:
        lzma_decompressor()
        {
            lzma_ret ret = lzma_auto_decoder(&strm, UINT64_MAX, 0);
            if (ret != LZMA_OK)
                throw std::runtime_error("failed to initialize LZMA decoder");
            out_buffer.resize(65536);
            in_buffer.resize(256 * 1024);
            in_end = 0;
        }
        ~lzma_decompressor() override
        {
            lzma_end(&strm);
        }
        void feed(const void *data, std::size_t size) override
        {
            const uint8_t *ptr = static_cast<const uint8_t *>(data);
            if (in_start > 0 && in_start > in_buffer.size() / 2)
            {
                std::size_t remaining = in_end - in_start;
                std::memmove(in_buffer.data(), in_buffer.data() + in_start, remaining);
                in_start = 0;
                in_end = remaining;
            }
            if (in_end + size > in_buffer.size())
            {
                in_buffer.resize(in_end + size);
            }
            std::memcpy(in_buffer.data() + in_end, ptr, size);
            in_end += size;
        }
        std::size_t read(void *out_data, std::size_t size) override
        {
            uint8_t *ptr = static_cast<uint8_t *>(out_data);
            std::size_t total_read = 0;

            while (total_read < size)
            {
                if (out_avail > 0)
                {
                    std::size_t to_copy = std::min(size - total_read, out_avail);
                    std::memcpy(ptr + total_read, out_buffer.data() + out_pos, to_copy);
                    total_read += to_copy;
                    out_pos += to_copy;
                    out_avail -= to_copy;
                    if (total_read == size)
                        return total_read;
                }
                strm.next_in = in_buffer.data() + in_start;
                strm.avail_in = in_end - in_start;
                if (strm.avail_in == 0)
                    break;
                strm.next_out = out_buffer.data();
                strm.avail_out = out_buffer.size();
                lzma_ret ret = lzma_code(&strm, LZMA_RUN);
                if (ret != LZMA_OK && ret != LZMA_STREAM_END)
                    throw std::runtime_error("LZMA decompression failed");
                in_start = in_end - strm.avail_in;
                out_pos = 0;
                out_avail = out_buffer.size() - strm.avail_out;
            }
            return total_read;
        }
    };
    std::unique_ptr<stream_cmp> mk_cmp_e(algorithm algo, int level)
    {
        if (algo == algorithm::zstd)
            return std::make_unique<zstd_compressor>(level);
        if (algo == algorithm::lzma2)
            return std::make_unique<lzma_compressor>(level);
        return nullptr;
    }
    std::unique_ptr<stream_dcmp> mk_dmp_e(algorithm algo)
    {
        if (algo == algorithm::zstd)
            return std::make_unique<zstd_decompressor>();
        if (algo == algorithm::lzma2)
            return std::make_unique<lzma_decompressor>();
        return nullptr;
    }
}

// end