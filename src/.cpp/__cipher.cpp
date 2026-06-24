// __cipher.cpp
// last updated: 17/06/2026
#include "../.hpp/__cipher.hpp"
#include "../.hpp/aes256gcm.hpp"
#include "../.hpp/xchacha20poly1305.hpp"
namespace pk::crypto::cipher
{
    std::unique_ptr<_cipher> mk_cipher(algorithm algo)
    {
        switch (algo)
        {
        case algorithm::aes_256_gcm:
            return std::make_unique<aes256gcm>();
        case algorithm::xchacha20_poly1305:
            return std::make_unique<xchacha20poly1305>();
        default:
            throw std::invalid_argument("unknown algorithm");
        }
    }
}

// end