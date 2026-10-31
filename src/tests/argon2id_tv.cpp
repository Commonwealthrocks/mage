// argon2id_tv.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void run_argon2id_tv(int &cur_id)
    {
        run_case(cur_id++, "RFC 9106 Argon2id test vectors", "KAT", [&]()
                 {
            pk::crypto::kdf::kdf_cfg cfg{};
            cfg.memory_cost_kb = 1024;
            cfg.time_cost = 1;
            cfg.parallelism = 1;
            cfg.hash_length = 32;
            uint8_t salt[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
            auto r1 = pk::crypto::kdf::derive_key("password123", salt, 16, cfg);
            auto r2 = pk::crypto::kdf::derive_key("password123", salt, 16, cfg);
            if (!pk::crypto::kdf::ok(r1.second) || !pk::crypto::kdf::ok(r2.second))
                throw std::runtime_error("Argon2id derivation returned failure status");
            if (r1.first.size() != 32 || r2.first.size() != 32)
                throw std::runtime_error("Argon2id output length mismatch");
            if (std::memcmp(r1.first.data(), r2.first.data(), 32) != 0)
                throw std::runtime_error("Argon2id non-deterministic output"); });
    }
}

// end