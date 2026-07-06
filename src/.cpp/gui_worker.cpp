// gui_worker.cpp
// last updated: 06/07/2026
// haha 67 guys get it?
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
        const QString &keyfile_path)
    {
        this->in_path = in_path.toUtf8().toStdString();
        output_dir = out_dir.toUtf8().toStdString();
        this->password = password.toUtf8().toStdString();
        this->keyfile_path = keyfile_path.toUtf8().toStdString();
    }
    void crypto_worker::run()
    {
        try
        {
            // emit the cc for better progress reporting in the
            // crypto_worker()
            auto cb = [this](uint64_t processed, uint64_t total, const std::string &cur_file)
            {
                if (total > 0)
                {
                    int p = static_cast<int>((processed * 100) / total);
                    emit progress(p);
                    emit pr_details(processed, total);
                }
                if (!cur_file.empty())
                {
                    emit current_file(QString::fromStdString(cur_file));
                }
            };
            auto status_cb = [this](const std::string &status)
            {
                emit current_ac0(QString::fromStdString(status));
            };
            pk::mem_::secure_string final_password;
            if (!keyfile_path.empty())
            {
                try
                {
                    auto kf = pk::crypto::keyfile::read(keyfile_path);
                    if (!password.empty())
                    {
                        final_password = pk::mem_::secure_string(password.begin(), password.end());
                        final_password.append(kf);
                    }
                    else
                    {
                        final_password = std::move(kf);
                    }
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
                emit current_ac0("Scanning files...");
                std::vector<pk::crypto::format::archive_entry> entries;
                std::filesystem::path archive_base_name = std::filesystem::path(output_path).stem();
                std::error_code ec_out;
                auto abs_out = std::filesystem::weakly_canonical(output_path, ec_out);
                auto is_already_encrypted = [](const std::filesystem::path &path, uint64_t fsize) -> bool
                {
                    auto ext = path.extension().string();
                    for (auto &c : ext)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (ext == ".mage")
                        return true;
                    if (fsize < 4)
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
#ifdef _WIN32
                    if (!_include_hidden)
                    {
                        DWORD root_attrs = GetFileAttributesW(root_path.wstring().c_str());
                        if (root_attrs != INVALID_FILE_ATTRIBUTES && (root_attrs & FILE_ATTRIBUTE_HIDDEN))
                        {
                            continue; // skipp
                        }
                    }
#endif
                    if (std::filesystem::is_directory(root_path))
                    {
                        auto it = std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied);
                        auto end = std::filesystem::recursive_directory_iterator();
                        while (it != end)
                        {
                            const auto &dir_entry = *it;
#ifdef _WIN32
                            // win32 has a call for this kind of stuff, microsoft knows better than me
                            WIN32_FILE_ATTRIBUTE_DATA winfo;
                            bool have_winfo = GetFileAttributesExW(dir_entry.path().wstring().c_str(), GetFileExInfoStandard, &winfo) != 0;
                            if (!_include_hidden && have_winfo && (winfo.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
                            {
                                if (dir_entry.is_directory())
                                    it.disable_recursion_pending();
                                ++it;
                                continue;
                            }
#endif
                            // skip the output file itself if it's inside the scanned dir
                            if (!ec_out)
                            {
                                auto entry_path = dir_entry.path().wstring();
                                auto out_str = abs_out.wstring();
                                if (entry_path == out_str)
                                {
                                    ++it;
                                    continue;
                                }
                            }
                            bool is_dir = dir_entry.is_directory();
                            pk::crypto::format::archive_entry ae;
                            ae.source_path = dir_entry.path();
                            ae.is_directory = is_dir;
#ifdef _WIN32
                            ae.file_size = is_dir ? 0 : (have_winfo ? (((uint64_t)winfo.nFileSizeHigh << 32) | winfo.nFileSizeLow) : std::filesystem::file_size(dir_entry.path()));
#else
                            ae.file_size = is_dir ? 0 : dir_entry.file_size();
#endif
                            if (!is_dir && is_already_encrypted(dir_entry.path(), ae.file_size))
                            {
                                throw std::runtime_error("cannot encrypt an already encrypted MAGE file -> " + dir_entry.path().filename().string());
                            }

                            // std::filesystem::relative is VERY slow on Windows.
                            // We know dir_entry is inside root_path, so just strip the prefix.
                            std::string rel_str = dir_entry.path().generic_string();
                            std::string root_str_gen = root_path.generic_string();
                            if (rel_str.length() > root_str_gen.length() && rel_str.compare(0, root_str_gen.length(), root_str_gen) == 0)
                            {
                                rel_str = rel_str.substr(root_str_gen.length());
                                if (!rel_str.empty() && rel_str[0] == '/')
                                {
                                    rel_str = rel_str.substr(1);
                                }
                            }
                            if (rel_str.empty() || rel_str == ".")
                                rel_str = dir_entry.path().filename().generic_string();

                            ae.relative_path = (archive_base_name / rel_str).generic_string();
                            if (__cp_metadata)
                            {
#ifdef _WIN32
                                if (have_winfo)
                                {
                                    ae.ctime = ((uint64_t)winfo.ftCreationTime.dwHighDateTime << 32) | winfo.ftCreationTime.dwLowDateTime;
                                    ae.atime = ((uint64_t)winfo.ftLastAccessTime.dwHighDateTime << 32) | winfo.ftLastAccessTime.dwLowDateTime;
                                    ae.mtime = ((uint64_t)winfo.ftLastWriteTime.dwHighDateTime << 32) | winfo.ftLastWriteTime.dwLowDateTime;
                                    ae.attrs = winfo.dwFileAttributes;
                                }
#endif
                            }
                            entries.push_back(std::move(ae));
                            ++it;
                        }
                    }
                    else
                    {
                        uint64_t fsize = std::filesystem::file_size(root_path);
                        if (is_already_encrypted(root_path, fsize))
                        {
                            throw std::runtime_error("cannot encrypt an already encrypted MAGE file -> " + root_path.filename().string());
                        }
                        pk::crypto::format::archive_entry ae;
                        ae.source_path = root_path;
                        ae.relative_path = (archive_base_name / root_path.filename()).generic_string();
                        ae.is_directory = false;
                        ae.file_size = fsize;
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
                        entries.push_back(std::move(ae));
                    }
                }
                emit current_ac0("Scanning " + QString::number(entries.size()) + " files...");
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
                    cb,
                    status_cb);
            }
            else if (__mode == mode::unpack)
            {
                emit current_ac0("Reading archive...");
                pk::crypto::format::unpack_archive(in_path, output_dir, final_password, cb, []() -> bool
                                                   {
                                                       bool proceed = false;
                                                       QMetaObject::invokeMethod(
                                                           qApp, [&proceed]()
                                                           { 
                                                               pk::ui::sfx::play_info();
                                                               proceed = pk::ui::outs::ask(nullptr, "Zipbomb warning", "Yo this shit possibly a zipbomb, do you want to proceed extracting?"); },
                                                           Qt::BlockingQueuedConnection);
                                                       return proceed; }, status_cb);
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