// aes256siv.hpp
// last updated: 03/08/2026
#pragma once
#include "__cipher.hpp"
#include <openssl/evp.h>
namespace pk::crypto::cipher
{
    class aes256siv : public _cipher
    {
    public:
        aes256siv();
        ~aes256siv() override;
        void init(const uint8_t *key, std::size_t key_len) override;
        std::vector<uint8_t> encrypt_chunk(
            const uint8_t *plaintext_chunk, std::size_t pt_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) override;
        pk::mem_::secure_vector decrypt_chunk(
            const uint8_t *ciphertext_with_mac, std::size_t ct_len,
            const uint8_t *ad, std::size_t ad_len,
            const uint8_t *nonce, std::size_t nonce_len) override;
        [[nodiscard]] std::size_t what_mac_size() const override { return 16; }
        [[nodiscard]] std::size_t nonce_size_abl() const override { return 12; }

    private:
        pk::mem_::secure_vector mm_key;
        EVP_CIPHER_CTX *ctx;
        EVP_CIPHER *cipher;
    };
}

// end