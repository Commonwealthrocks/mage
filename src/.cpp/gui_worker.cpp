// gui_worker.cpp
// last updated: 28/06/2026
#include "../.hpp/gui_worker.hpp"
#include "../.hpp/fileformat.hpp"
#include "../.hpp/error_msg.hpp"
#include "../.hpp/keyfile.hpp"
#include "../.hpp/outs.hpp"
#include "../.hpp/sfx.hpp"
#include <QApplication>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace pk::ui::worker
{
    crypto_worker::crypto_worker(mode m, QObject *parent)
        : QThread(parent), __mode(m)
    {
    }
    void crypto_worker::ss_def_pk_params(
        const std::vector<std::string> &roots,
        const QString &out_path,
        const QString &password,
        bool use_keyfile,
        const QString &keyfile_path,
        pk::crypto::cipher::algorithm algo,
        const pk::crypto::kdf::kdf_cfg &kdf_cfg,
        bool include_hidden,
        bool cp_metadata,
        uint8_t compression_algo,
        int8_t compression_level,
        int chunk_size_mb)
    {
        roots_f_rooted = roots;
        output_path = out_path.toUtf8().toStdString();
        this->password = password.toUtf8().toStdString();
        use_keyfile_yeah = use_keyfile;
        this->keyfile_path = keyfile_path.toUtf8().toStdString();
        this->algo = algo;
        __kdf_cfg = kdf_cfg;
        _include_hidden = include_hidden;
        __cp_metadata = cp_metadata;
        cmp_algo = compression_algo;
        cmp_lvl = compression_level;
        cs_mbs = chunk_size_mb;
    }
    void crypto_worker::ss_def_unpk_params(
        const QString &in_path,
        const QString &out_dir,
        const QString &password,
        bool use_keyfile,
        const QString &keyfile_path)
    {
        this->in_path = in_path.toUtf8().toStdString();
        output_dir = out_dir.toUtf8().toStdString();
        this->password = password.toUtf8().toStdString();
        use_keyfile_yeah = use_keyfile;
        this->keyfile_path = keyfile_path.toUtf8().toStdString();
    }
    void crypto_worker::run()
    {
        try
        {
            auto cb = [this](uint64_t processed, uint64_t total)
            {
                if (total > 0)
                {
                    int p = static_cast<int>((processed * 100) / total);
                    emit progress(p);
                    emit progress_details(processed, total);
                }
            };
            pk::mem_::secure_string final_password;
            if (use_keyfile_yeah)
            {
                try
                {
                    final_password = pk::crypto::keyfile::read(keyfile_path);
                }
                catch (const std::exception &e)
                {
                    emit error(QString("Keyfile error: ") + e.what());
                    return;
                }
            }
            else
            {
                final_password = pk::mem_::secure_string(password.begin(), password.end());
            }

            if (__mode == mode::pack)
            {
                std::vector<pk::crypto::format::archive_entry> entries;
                std::filesystem::path archive_base_name = std::filesystem::path(output_path).stem();
                std::error_code ec_out;
                auto abs_out = std::filesystem::weakly_canonical(output_path, ec_out);
                auto is_already_encrypted = [](const std::filesystem::path &path) -> bool
                {
                    std::error_code ec;
                    if (std::filesystem::file_size(path, ec) < 4 || ec)
                        return false;
                    std::ifstream in(path, std::ios::binary);
                    if (!in)
                        return false;
                    char magic[4] = {0};
                    in.read(magic, 4);
                    return (in.gcount() == 4 && std::memcmp(magic, pk::crypto::format::MAGIC_BYTES, 4) == 0);
                };
                for (const auto &root_str : roots_f_rooted)
                {
                    std::filesystem::path root_path = root_str;

                    std::error_code ec_root;
                    auto abs_root = std::filesystem::weakly_canonical(root_path, ec_root);
                    if (!ec_root && !ec_out && abs_root == abs_out)
                    {
                        throw std::runtime_error("the output archive cannot be placed inside a directory that is being archived, please choose a different output location.");
                    }
                    if (!_include_hidden)
                    {
#ifdef _WIN32
                        DWORD root_attrs = GetFileAttributesW(root_path.wstring().c_str());
                        if (root_attrs != INVALID_FILE_ATTRIBUTES && (root_attrs & FILE_ATTRIBUTE_HIDDEN))
                        {
                            continue; // skip hidden root?
                        }
#endif
                    }
                    if (std::filesystem::is_directory(root_path))
                    {
                        auto it = std::filesystem::recursive_directory_iterator(root_path);
                        auto end = std::filesystem::recursive_directory_iterator();
                        while (it != end)
                        {
                            if (!_include_hidden)
                            {
#ifdef _WIN32
                                DWORD attrs = GetFileAttributesW(it->path().wstring().c_str());
                                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN))
                                {
                                    if (it->is_directory())
                                        it.disable_recursion_pending();
                                    ++it;
                                    continue;
                                }
#endif
                            }
                            std::error_code ec_it;
                            auto abs_it = std::filesystem::weakly_canonical(it->path(), ec_it);
                            if (!ec_it && !ec_out && abs_it == abs_out)
                            {
                                throw std::runtime_error("the output archive cannot be placed inside a directory that is being archived, please choose a different output location.");
                            }
                            if (!std::filesystem::is_directory(it->path()) && is_already_encrypted(it->path()))
                            {
                                throw std::runtime_error("cannot encrypt an already encrypted MAGE file -> " + it->path().filename().string());
                            }
                            pk::crypto::format::archive_entry ae;
                            ae.source_path = it->path();
                            std::filesystem::path rel = std::filesystem::relative(it->path(), root_path);
                            if (rel.empty() || rel == ".")
                                rel = it->path().filename();
                            std::filesystem::path new_rel = archive_base_name / rel;
                            ae.relative_path = new_rel.generic_string();
                            ae.is_directory = std::filesystem::is_directory(it->path());
                            ae.file_size = ae.is_directory ? 0 : std::filesystem::file_size(it->path());
                            if (__cp_metadata)
                            {
#ifdef _WIN32
                                WIN32_FILE_ATTRIBUTE_DATA info;
                                if (GetFileAttributesExW(ae.source_path.wstring().c_str(), GetFileExInfoStandard, &info))
                                {
                                    ae.ctime = ((uint64_t)info.ftCreationTime.dwHighDateTime << 32) | info.ftCreationTime.dwLowDateTime;
                                    ae.atime = ((uint64_t)info.ftLastAccessTime.dwHighDateTime << 32) | info.ftLastAccessTime.dwLowDateTime;
                                    ae.mtime = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) | info.ftLastWriteTime.dwLowDateTime;
                                    ae.attrs = info.dwFileAttributes;
                                }
#endif
                            }
                            entries.push_back(ae);
                            ++it;
                        }
                    }
                    else
                    {
                        if (is_already_encrypted(root_path))
                        {
                            throw std::runtime_error("cannot encrypt an already encrypted MAGE file -> " + root_path.filename().string());
                        }
                        pk::crypto::format::archive_entry ae;
                        ae.source_path = root_path;
                        ae.relative_path = (archive_base_name / root_path.filename()).generic_string();
                        ae.is_directory = false;
                        ae.file_size = std::filesystem::file_size(root_path);
                        if (__cp_metadata)
                        {
#ifdef _WIN32
                            WIN32_FILE_ATTRIBUTE_DATA info;
                            if (GetFileAttributesExW(ae.source_path.wstring().c_str(), GetFileExInfoStandard, &info))
                            {
                                ae.ctime = ((uint64_t)info.ftCreationTime.dwHighDateTime << 32) | info.ftCreationTime.dwLowDateTime;
                                ae.atime = ((uint64_t)info.ftLastAccessTime.dwHighDateTime << 32) | info.ftLastAccessTime.dwLowDateTime;
                                ae.mtime = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) | info.ftLastWriteTime.dwLowDateTime;
                                ae.attrs = info.dwFileAttributes;
                            }
#endif
                        }
                        entries.push_back(ae);
                    }
                }
                pk::crypto::format::pack_archive(
                    entries,
                    output_path,
                    final_password,
                    algo,
                    __kdf_cfg,
                    __cp_metadata,
                    cmp_algo,
                    cmp_lvl,
                    static_cast<std::size_t>(cs_mbs) * 1024 * 1024,
                    cb);
            }
            else if (__mode == mode::unpack)
            {
                pk::crypto::format::unpack_archive(in_path, output_dir, final_password, cb,
                                                   []() -> bool
                                                   {
                                                       bool proceed = false;
                                                       QMetaObject::invokeMethod(
                                                           qApp, [&proceed]()
                                                           { 
                                                               pk::ui::sfx::play_info();
                                                               proceed = pk::ui::outs::ask(nullptr, "Zipbomb warning", "Yo this shit possibly a zipbomb, do you want to proceed extracting?"); },
                                                           Qt::BlockingQueuedConnection);
                                                       return proceed;
                                                   });
            }
            emit success();
        }
        catch (const std::exception &e)
        {
            emit error(QString::fromStdString(pk::error::format(e.what())));
        }
        catch (...)
        {
            emit error(QString::fromStdString(pk::error::format_idk()));
        }
    }
}

// end