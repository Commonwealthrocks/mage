// cmp_e.hpp
// last updated: 17/06/2026
#pragma once
#include <vector>
#include <cstdint>
#include <memory>
namespace pk::crypto::cmp_e
{
    enum class algorithm : uint8_t
    {
        none = 0,
        zstd = 1,
        lzma2 = 2
    };

    class stream_cmp
    {
    public:
        virtual ~stream_cmp() = default;
        virtual std::vector<uint8_t> cmp(const void *data, std::size_t size) = 0;
        virtual std::vector<uint8_t> done() = 0;
    };
    class stream_dcmp
    {
    public:
        virtual ~stream_dcmp() = default;
        virtual void feed(const void *data, std::size_t size) = 0;
        virtual std::size_t read(void *out_data, std::size_t size) = 0;
    };
    std::unique_ptr<stream_cmp> mk_cmp_e(algorithm algo, int level);
    std::unique_ptr<stream_dcmp> mk_dmp_e(algorithm algo);
}

// end