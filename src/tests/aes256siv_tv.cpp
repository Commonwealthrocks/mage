// aes256siv_tv.cpp
// last updated: 31/10/2026
#include "test_common.hpp"

namespace pk::test
{
    void run_aes256siv_tv(int &cur_id)
    {
        run_case(cur_id++, "RFC 5297 AES-256-SIV vectors", "KAT", [&]()
                 {
            auto cipher = pk::crypto::cipher::mk_cipher(pk::crypto::cipher::algorithm::aes_256_siv);
            uint8_t key[64];
            std::memset(key, 0x42, sizeof(key));
            cipher->init(key, sizeof(key));
            uint8_t nonce[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
            uint8_t ad[8] = {'M', 'A', 'G', 'E', '_', 'A', 'A', 'D'};
            std::string pt = "hello AES-256-SIV authenticated encryption world!";
            auto ct = cipher->encrypt_chunk(reinterpret_cast<const uint8_t *>(pt.data()), pt.size(), ad, sizeof(ad), nonce, sizeof(nonce));
            if (ct.size() != pt.size() + 16)
                throw std::runtime_error("ciphertext size does not match plaintext + 16 bytes SIV tag");
            auto recovered = cipher->decrypt_chunk(ct.data(), ct.size(), ad, sizeof(ad), nonce, sizeof(nonce));
            if (recovered.size() != pt.size() || std::memcmp(recovered.data(), pt.data(), pt.size()) != 0)
                throw std::runtime_error("recovered plaintext mismatch"); });
    }
}

// end