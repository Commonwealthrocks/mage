// argon2id_hashing.hpp
// last updated: 17/06/2026
#pragma once
#include <cstdint>
#include <vector>
#include <string_view>
#include <string>
#include <utility>
#include "secure_memory.hpp"
namespace pk::crypto::kdf
{
    struct kdf_cfg
    {
        uint32_t memory_cost_kb;
        uint32_t time_cost;
        uint32_t parallelism;
        uint32_t hash_length;
    };
    enum class kdf_err
    {
        none = 0,
        invalid_memory_cost,
        invalid_time_cost,
        invalid_parallelism,
        invalid_hash_length,
        salt_too_short,
        internal_argon2_error,
        rand_generation_failed
    };
    constexpr std::size_t k_min_salt_length = 8;
    constexpr std::size_t k_min_hash_length = 4;
    [[nodiscard]] bool ok(kdf_err err) noexcept;
    [[nodiscard]] std::string describe(kdf_err err);
    [[nodiscard]] std::pair<pk::mem_::secure_vector, kdf_err> mk_salt(std::size_t length = 16);
    [[nodiscard]] std::pair<pk::mem_::secure_vector, kdf_err> derive_key(
        std::string_view password,
        const uint8_t *salt, std::size_t salt_len,
        const kdf_cfg &cfg);
}

// end