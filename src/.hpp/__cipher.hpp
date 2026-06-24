// __cipher.hpp
// last updated: 17/06/2026
#pragma once
#include "secure_memory.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <stdexcept>
namespace pk::crypto::cipher
{
    enum class algorithm
    {
        aes_256_gcm = 0,
        xchacha20_poly1305 = 1
    };
    class _cipher
    {
    public:
        virtual ~_cipher() = default;
        virtual void init(const uint8_t *key, std::size_t key_len) = 0;
        virtual std::vector<uint8_t> encrypt_chunk(
            const uint8_t *plaintext_chunk, std::size_t pt_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) = 0;
        virtual pk::mem_::secure_vector decrypt_chunk(
            const uint8_t *ciphertext_with_mac, std::size_t ct_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) = 0;
        [[nodiscard]] virtual std::size_t what_mac_size() const = 0;
        [[nodiscard]] virtual std::size_t nonce_size_abl() const = 0;
    };
    std::unique_ptr<_cipher> mk_cipher(algorithm algo);
}

// End