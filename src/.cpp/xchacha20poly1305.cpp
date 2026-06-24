// xchacha20poly1305.cpp
// last updated: 17/06/2026
#include "../.hpp/xchacha20poly1305.hpp"
#include <stdexcept>
namespace pk::crypto::cipher
{
    void xchacha20poly1305::init(const uint8_t *key, std::size_t key_len)
    {
        if (key_len != crypto_aead_xchacha20poly1305_ietf_KEYBYTES)
            throw std::invalid_argument("XChaCha20-Poly1305 requires a 32-byte key");
        mm_key.assign(key, key + key_len);
    }
    std::vector<uint8_t> xchacha20poly1305::encrypt_chunk(
        const uint8_t *plaintext_chunk, std::size_t pt_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        if (nonce_len != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES)
            throw std::invalid_argument("invalid nonce size");
        std::vector<uint8_t> out(pt_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
        unsigned long long ciphertext_len = 0;
        int ret = crypto_aead_xchacha20poly1305_ietf_encrypt(
            out.data(), &ciphertext_len,
            plaintext_chunk, pt_len,
            ad_len == 0 ? nullptr : ad, ad_len,
            nullptr, // nsec is not used
            nonce,
            mm_key.data());
        if (ret != 0)
            throw std::runtime_error("crypto_aead_xchacha20poly1305_ietf_encrypt failed");
        out.resize(ciphertext_len);
        return out;
    }
    pk::mem_::secure_vector xchacha20poly1305::decrypt_chunk(
        const uint8_t *ciphertext_with_mac, std::size_t ct_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        if (nonce_len != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES)
            throw std::invalid_argument("invalid nonce size");
        if (ct_len < crypto_aead_xchacha20poly1305_ietf_ABYTES)
            throw std::invalid_argument("ciphertext too short to contain MAC");
        std::size_t plaintext_len = ct_len - crypto_aead_xchacha20poly1305_ietf_ABYTES;
        pk::mem_::secure_vector out(plaintext_len);
        unsigned long long decrypted_len = 0;
        int ret = crypto_aead_xchacha20poly1305_ietf_decrypt(
            out.data(), &decrypted_len,
            nullptr, // nsec is not used
            ciphertext_with_mac, ct_len,
            ad_len == 0 ? nullptr : ad, ad_len,
            nonce,
            mm_key.data());
        if (ret != 0)
            throw std::runtime_error("MAC verification failed");
        return out;
    }
}

// end