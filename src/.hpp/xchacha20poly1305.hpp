// xchacha20poly1305.hpp
// last updated: 17/06/2026
#pragma once
#include "__cipher.hpp"
#include <sodium.h>
namespace pk::crypto::cipher
{
    class xchacha20poly1305 : public _cipher
    {
    public:
        xchacha20poly1305() = default;
        ~xchacha20poly1305() override = default;
        void init(const uint8_t *key, std::size_t key_len) override;
        std::vector<uint8_t> encrypt_chunk(
            const uint8_t *plaintext_chunk, std::size_t pt_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) override;
        pk::mem_::secure_vector decrypt_chunk(
            const uint8_t *ciphertext_with_mac, std::size_t ct_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) override;
        [[nodiscard]] std::size_t what_mac_size() const override { return crypto_aead_xchacha20poly1305_ietf_ABYTES; }
        [[nodiscard]] std::size_t nonce_size_abl() const override { return crypto_aead_xchacha20poly1305_ietf_NPUBBYTES; }

    private:
        pk::mem_::secure_vector mm_key;
    };
}

// end