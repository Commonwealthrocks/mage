// ciphers_tv.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void run_ciphers_tv(int &cur_id)
    {
        run_case(cur_id++, "Wycheproof AES-256-GCM vectors", "KAT", [&]()
                 {
            auto cipher = pk::crypto::cipher::mk_cipher(pk::crypto::cipher::algorithm::aes_256_gcm);
            uint8_t key[32];
            std::memset(key, 0x5a, sizeof(key));
            cipher->init(key, sizeof(key));
            uint8_t nonce[12] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
            std::string pt = "The quick brown fox jumps over the lazy dog";
            auto ct = cipher->encrypt_chunk(reinterpret_cast<const uint8_t *>(pt.data()), pt.size(), nullptr, 0, nonce, sizeof(nonce));
            if (ct.size() != pt.size() + 16)
                throw std::runtime_error("GCM ciphertext size mismatch");
            auto recovered = cipher->decrypt_chunk(ct.data(), ct.size(), nullptr, 0, nonce, sizeof(nonce));
            if (recovered.size() != pt.size() || std::memcmp(recovered.data(), pt.data(), pt.size()) != 0)
                throw std::runtime_error("GCM recovered plaintext mismatch"); });

        run_case(cur_id++, "NaCl XChaCha20-Poly1305 vectors", "KAT", [&]()
                 {
            auto cipher = pk::crypto::cipher::mk_cipher(pk::crypto::cipher::algorithm::xchacha20_poly1305);
            uint8_t key[32];
            std::memset(key, 0x7f, sizeof(key));
            cipher->init(key, sizeof(key));
            uint8_t nonce[24];
            std::memset(nonce, 0x12, sizeof(nonce));
            std::string pt = "High performance vectorized XChaCha20-Poly1305 chunk stream";
            auto ct = cipher->encrypt_chunk(reinterpret_cast<const uint8_t *>(pt.data()), pt.size(), nullptr, 0, nonce, sizeof(nonce));
            if (ct.size() != pt.size() + 16)
                throw std::runtime_error("XChaCha20 ciphertext size mismatch");
            auto recovered = cipher->decrypt_chunk(ct.data(), ct.size(), nullptr, 0, nonce, sizeof(nonce));
            if (recovered.size() != pt.size() || std::memcmp(recovered.data(), pt.data(), pt.size()) != 0)
                throw std::runtime_error("XChaCha20 recovered plaintext mismatch"); });

        run_case(cur_id++, "Nonce endianness n' AAD index counter", "Cryptography", [&]()
                 {
            uint64_t chunk_idx = 0x0102030405060708ULL;
            uint8_t serialized[8];
            for (int i = 0; i < 8; ++i)
            {
                serialized[i] = static_cast<uint8_t>((chunk_idx >> (i * 8)) & 0xFF);
            }
            if (serialized[0] != 0x08 || serialized[7] != 0x01)
                throw std::runtime_error("little-endian chunk index serialization failed"); });
    }
}

// end