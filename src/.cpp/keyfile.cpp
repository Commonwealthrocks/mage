// keyfile.cpp
// last updated: 17/06/2026
#include "../.hpp/keyfile.hpp"
#include "../.hpp/path_handler.hpp"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#endif
namespace pk::crypto::keyfile
{
    void generate(const std::string &path)
    {
        if (!pk::path::ok(pk::path::validate_host_path(path)))
            throw std::runtime_error("invalid path provided for keyfile generation.");

        std::vector<uint8_t> buffer(512);
#ifdef _WIN32
        if (BCryptGenRandom(NULL, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            throw std::runtime_error("failed to generate random data (BCryptGenRandom failed).");
        }
#else
        if (getrandom(buffer.data(), buffer.size(), 0) != static_cast<ssize_t>(buffer.size()))
        {
            throw std::runtime_error("failed to generate random data (getrandom failed).");
        }
#endif
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("failed to create keyfile for writing.");
        out.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        out.close();
    }
    pk::mem_::secure_string read(const std::string &path)
    {
        if (!pk::path::ok(pk::path::validate_host_path(path)))
            throw std::runtime_error("invalid path provided for keyfile reading.");
        if (std::filesystem::file_size(path) != 512)
            throw std::runtime_error("invalid keyfile size; must be exactly 512 bytes.");
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("failed to open keyfile for reading.");
        pk::mem_::secure_string buffer(512, '\0');
        if (!in.read(buffer.data(), buffer.size()))
            throw std::runtime_error("failed to read 512 bytes from keyfile.");
        return buffer;
    }
}

// end