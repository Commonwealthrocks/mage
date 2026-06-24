// aes256gcm.cpp
// last updated: 17/06/2026
#include "../.hpp/aes256gcm.hpp"
namespace pk::crypto::cipher
{
    aes256gcm::aes256gcm()
    {
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    }
    aes256gcm::~aes256gcm()
    {
        if (ctx)
            EVP_CIPHER_CTX_free(ctx);
    }
    void aes256gcm::init(const uint8_t *key, std::size_t key_len)
    {
        if (key_len != 32)
            throw std::invalid_argument("AES-256-GCM requires a 32-byte key");
        mm_key.assign(key, key + key_len);
    }
    std::vector<uint8_t> aes256gcm::encrypt_chunk(
        const uint8_t *plaintext_chunk, std::size_t pt_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        if (nonce_len != 12)
            throw std::invalid_argument("invalid nonce size");
        std::vector<uint8_t> out(pt_len + 16);
        int len = 0;
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
            throw std::runtime_error("EVP_EncryptInit_ex failed");
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl failed");
        if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr, mm_key.data(), nonce))
            throw std::runtime_error("EVP_EncryptInit_ex key/nonce failed");
        if (ad_len > 0)
        {
            if (1 != EVP_EncryptUpdate(ctx, nullptr, &len, ad, static_cast<int>(ad_len)))
                throw std::runtime_error("EVP_EncryptUpdate AD failed");
        }
        if (pt_len > 0)
        {
            if (1 != EVP_EncryptUpdate(ctx, out.data(), &len, plaintext_chunk, static_cast<int>(pt_len)))
                throw std::runtime_error("EVP_EncryptUpdate plaintext failed");
        }
        if (1 != EVP_EncryptFinal_ex(ctx, out.data() + len, &len))
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out.data() + pt_len))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl get tag failed");
        return out;
    }
    pk::mem_::secure_vector aes256gcm::decrypt_chunk(
        const uint8_t *ciphertext_with_mac, std::size_t ct_len,
        const uint8_t *ad, std::size_t ad_len,
        const uint8_t *nonce, std::size_t nonce_len)
    {
        if (mm_key.empty())
            throw std::logic_error("cipher not initialized");
        if (nonce_len != 12)
            throw std::invalid_argument("invalid nonce size");
        if (ct_len < 16)
            throw std::invalid_argument("ciphertext too short");
        std::size_t plaintext_len = ct_len - 16;
        pk::mem_::secure_vector out(plaintext_len);
        int len = 0;
        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
            throw std::runtime_error("EVP_DecryptInit_ex failed");
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl failed");
        if (1 != EVP_DecryptInit_ex(ctx, nullptr, nullptr, mm_key.data(), nonce))
            throw std::runtime_error("EVP_DecryptInit_ex key/nonce failed");
        if (ad_len > 0)
        {
            if (1 != EVP_DecryptUpdate(ctx, nullptr, &len, ad, static_cast<int>(ad_len)))
                throw std::runtime_error("EVP_DecryptUpdate AD failed");
        }
        if (plaintext_len > 0)
        {
            if (1 != EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext_with_mac, static_cast<int>(plaintext_len)))
                throw std::runtime_error("EVP_DecryptUpdate ciphertext failed");
        }
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t *>(ciphertext_with_mac + plaintext_len)))
            throw std::runtime_error("EVP_CIPHER_CTX_ctrl set tag failed");
        int ret = EVP_DecryptFinal_ex(ctx, out.data() + len, &len);
        if (ret <= 0)
        {
            throw std::runtime_error("MAC verification failed");
        }
        return out;
    }
}

// end