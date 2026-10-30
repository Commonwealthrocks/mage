// keyfile.cpp
// last updated: 30/10/2026
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace pk::crypto::keyfile
{
    void generate(const std::string &path)
    {
        if (!pk::path::ok(pk::path::validate_host_path(path)))
            throw std::runtime_error("invalid path provided for keyfile generation.");
        std::filesystem::path fs_path = pk::path::from_utf8(path);
        if (pk::path::is_symlink_or_other_thingy_whatever(fs_path))
            throw std::runtime_error("security error -> refusing to overwrite symlink or reparse point for keyfile generation.");
        if (fs_path.has_parent_path() && !fs_path.parent_path().empty() &&
            pk::path::is_symlink_or_other_thingy_whatever(fs_path.parent_path()))
        {
            throw std::runtime_error("security error -> keyfile parent directory is a symlink or reparse point.");
        }
        pk::mem_::secure_vector buffer(512);
#ifdef _WIN32
        if (BCryptGenRandom(NULL, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            throw std::runtime_error("failed to generate random data (BCryptGenRandom failed).");
        }
        std::ofstream out(pk::path::to_ext_path(fs_path), std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("failed to create keyfile for writing.");
        if (pk::path::is_symlink_or_other_thingy_whatever(fs_path))
        {
            out.close();
            std::error_code ec_rm;
            pk::path::rfs___(fs_path, ec_rm);
            throw std::runtime_error("security error -> keyfile target became a symlink or reparse point.");
        }
        out.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        if (!out)
            throw std::runtime_error("failed to write 512 bytes to keyfile.");
        out.close();
#else
        std::size_t filled = 0;
        while (filled < buffer.size())
        {
            ssize_t r = getrandom(buffer.data() + filled, buffer.size() - filled, 0);
            if (r < 0)
            {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("failed to generate random data (getrandom failed).");
            }
            filled += static_cast<std::size_t>(r);
        }
        int flags = O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_NOFOLLOW
                    | O_NOFOLLOW
#endif
#ifdef O_CLOEXEC
                    | O_CLOEXEC
#endif
            ;
        int fd = ::open(fs_path.c_str(), flags, S_IRUSR | S_IWUSR);
        if (fd < 0)
            throw std::runtime_error("failed to create keyfile for writing.");
        (void)::fchmod(fd, S_IRUSR | S_IWUSR);
        std::size_t written = 0;
        while (written < buffer.size())
        {
            ssize_t w = ::write(fd, buffer.data() + written, buffer.size() - written);
            if (w < 0)
            {
                if (errno == EINTR)
                    continue;
                ::close(fd);
                std::error_code ec_rm;
                std::filesystem::remove(fs_path, ec_rm);
                throw std::runtime_error("failed to write 512 bytes to keyfile.");
            }
            written += static_cast<std::size_t>(w);
        }
        ::close(fd);
#endif
        pk::mem_::secure_wipe(buffer.data(), buffer.size());
    }
    pk::mem_::secure_string read(const std::string &path)
    {
        if (!pk::path::ok(pk::path::validate_host_path(path)))
            throw std::runtime_error("invalid path provided for keyfile reading.");
        std::filesystem::path fs_path = pk::path::from_utf8(path);
        if (pk::path::is_symlink_or_other_thingy_whatever(fs_path))
            throw std::runtime_error("invalid keyfile; symbolic links and reparse points are not permitted.");
        std::filesystem::path ext_path = pk::path::to_ext_path(fs_path);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(ext_path, ec) || ec)
            throw std::runtime_error("failed to open keyfile for reading.");
        if (std::filesystem::file_size(ext_path, ec) != 512 || ec)
            throw std::runtime_error("invalid keyfile size; must be exactly 512 bytes.");
        std::ifstream in(ext_path, std::ios::binary);
        if (!in)
            throw std::runtime_error("failed to open keyfile for reading.");
        pk::mem_::secure_string buffer(512, '\0');
        if (!in.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || in.gcount() != 512)
            throw std::runtime_error("failed to read 512 bytes from keyfile.");
        return buffer;
    }
}

// end