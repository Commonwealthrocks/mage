// aes256siv.cpp
// last updated: 03/08/2026
#include "../.hpp/aes256siv.hpp"
namespace pk::crypto::cipher
{
    aes256siv::aes256siv()
    {
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            throw std::runtime_error("EVP_CIPHER_CTX_new failed");
        cipher = EVP_CIPHER_fetch(nullptr, "AES-256-SIV", nullptr);
        if (!cipher)
        {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_CIPHER_fetch failed for AES-256-SIV");
        }
    }
    aes256siv::~aes256siv()
    {
        if (ctx)
            EVP_CIPHER_CTX_free(ctx);
        if (cipher)
            EVP_CIPHER_free(cipher);
    }
    void aes256siv::init(const uint8_t *key, std::size_t key_len)
    {
        if (key_len != 64)
            throw std::invalid_argument("AES-256-SIV requires a 64-byte key");
        mm_key.assign(key, key + key_len);
    }
    std::vector<uint8_t> aes256siv::encrypt_chunk(
        const uint8_t *plaintext_chunk, std::size_t pt_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        std::vector<uint8_t> out(pt_len + 16);
        int len = 0;
        if (1 != EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr))
            throw std::runtime_error("EVP_EncryptInit_ex failed");
        if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr, mm_key.data(), nullptr))
            throw std::runtime_error("EVP_EncryptInit_ex key failed");
        if (ad_len > 0)
        {
            if (1 != EVP_EncryptUpdate(ctx, nullptr, &len, ad, static_cast<int>(ad_len)))
                throw std::runtime_error("EVP_EncryptUpdate AD failed");
        }
        if (nonce_len > 0)
        {
            if (1 != EVP_EncryptUpdate(ctx, nullptr, &len, nonce, static_cast<int>(nonce_len)))
                throw std::runtime_error("EVP_EncryptUpdate nonce failed");
        }
        if (pt_len > 0)
        {
            if (1 != EVP_EncryptUpdate(ctx, out.data(), &len, plaintext_chunk, static_cast<int>(pt_len)))
                throw std::runtime_error("EVP_EncryptUpdate plaintext failed");
        }
        if (1 != EVP_EncryptFinal_ex(ctx, out.data() + len, &len))
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, out.data() + pt_len))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl get tag failed");
        return out;
    }
    pk::mem_::secure_vector aes256siv::decrypt_chunk(
        const uint8_t *ciphertext_with_mac, std::size_t ct_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        if (ct_len < 16)
            throw std::invalid_argument("ciphertext too short");
        std::size_t plaintext_len = ct_len - 16;
        pk::mem_::secure_vector out(plaintext_len);
        int len = 0;
        if (1 != EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr))
            throw std::runtime_error("EVP_DecryptInit_ex failed");
        if (1 != EVP_DecryptInit_ex(ctx, nullptr, nullptr, mm_key.data(), nullptr))
            throw std::runtime_error("EVP_DecryptInit_ex key failed");
        if (ad_len > 0)
        {
            if (1 != EVP_DecryptUpdate(ctx, nullptr, &len, ad, static_cast<int>(ad_len)))
                throw std::runtime_error("EVP_DecryptUpdate AD failed");
        }
        if (nonce_len > 0)
        {
            if (1 != EVP_DecryptUpdate(ctx, nullptr, &len, nonce, static_cast<int>(nonce_len)))
                throw std::runtime_error("EVP_DecryptUpdate nonce failed");
        }
        
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, const_cast<uint8_t*>(ciphertext_with_mac + plaintext_len)))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl set tag failed");
            
        if (plaintext_len > 0)
        {
            if (1 != EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext_with_mac, static_cast<int>(plaintext_len)))
                throw std::runtime_error("EVP_DecryptUpdate ciphertext failed");
        }
        int ret = EVP_DecryptFinal_ex(ctx, out.data() + len, &len);
        if (ret <= 0)
            throw std::runtime_error("MAC verification failed");
        return out;
    }
}

// end