// argon2id_hashing.cpp
// last updated: 17/06/2026
#include "../.hpp/argon2id_hashing.hpp"
#include <argon2.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
#else
#include <fstream>
#endif
namespace pk::crypto::kdf
{
    bool ok(kdf_err err) noexcept
    {
        return err == kdf_err::none;
    }
    std::string describe(kdf_err err)
    {
        switch (err)
        {
        case kdf_err::none:
            return "OK?"; // not sure how this would be used
        case kdf_err::invalid_memory_cost:
            return "mem_ cost is too low for the requested parallelism (minimum is 8 * parallelism KB).";
        case kdf_err::invalid_time_cost:
            return "time cost must be at least 1.";
        case kdf_err::invalid_parallelism:
            return "parallelism must be at least 1.";
        case kdf_err::invalid_hash_length:
            return "hash length must be at least 4 bytes.";
        case kdf_err::salt_too_short:
            return "salt is too short (minimum 8 bytes).";
        case kdf_err::internal_argon2_error:
            return "An internal Argon2 error occurred during key derivation.";
        case kdf_err::rand_generation_failed:
            return "failed to generate cryptographically secure random bytes.";
        }
        return "unknown key derivation error.";
    }
    std::pair<pk::mem_::secure_vector, kdf_err> mk_salt(std::size_t length)
    {
        if (length < k_min_salt_length)
            return {{}, kdf_err::salt_too_short};
        pk::mem_::secure_vector salt(length);
#ifdef _WIN32
        NTSTATUS status = BCryptGenRandom(NULL, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status != 0)
        {
            return {{}, kdf_err::rand_generation_failed};
        }
#else
        std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary); // linux makes this very ez
        if (urandom)
        {
            urandom.read(reinterpret_cast<char *>(salt.data()), salt.size());
            if (!urandom)
            {
                return {{}, kdf_err::rand_generation_failed};
            }
        }
        else
        {
            return {{}, kdf_err::rand_generation_failed};
        }
#endif
        return {std::move(salt), kdf_err::none};
    }
    std::pair<pk::mem_::secure_vector, kdf_err> derive_key(
        std::string_view password,
        const uint8_t *salt, std::size_t salt_len,
        const kdf_cfg &cfg)
    {
        if (cfg.parallelism < 1)
            return {{}, kdf_err::invalid_parallelism};
        if (cfg.memory_cost_kb < 8 * cfg.parallelism)
            return {{}, kdf_err::invalid_memory_cost};
        if (cfg.time_cost < 1)
            return {{}, kdf_err::invalid_time_cost};
        if (cfg.hash_length < k_min_hash_length)
            return {{}, kdf_err::invalid_hash_length};
        if (salt_len < k_min_salt_length)
            return {{}, kdf_err::salt_too_short};
        pk::mem_::secure_vector hash(cfg.hash_length);
        int result = argon2id_hash_raw(cfg.time_cost, cfg.memory_cost_kb, cfg.parallelism, password.data(), password.size(), salt, salt_len, hash.data(), hash.size());
        if (result != ARGON2_OK)
        {
            return {{}, kdf_err::internal_argon2_error};
        }
        return {std::move(hash), kdf_err::none};
    }
}

// end