// gui_worker.cpp
// last updated: 29/10/2026
#include "../.hpp/gui_worker.hpp"
#include "../.hpp/fileformat.hpp"
#include "../.hpp/error_msg.hpp"
#include "../.hpp/keyfile.hpp"
#include "../.hpp/secure_memory.hpp"
#include "../.hpp/outs.hpp"
#include "../.hpp/sfx.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#include "../.hpp/logger.hpp"
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
        const QString &in,
        const QString &out,
        const QString &pwd,
        const QString &kf,
        int ext_beh,
        int ext_ow)
    {
        in_path = in.toStdString();
        output_dir = out.toStdString();
        password = pwd.toStdString();
        keyfile_path = kf.toStdString();
        ext_behavior_v = ext_beh;
        ext_overwrite_v = ext_ow;
    }
    void crypto_worker::ss_def_verify_params(const QString &in, const QString &pwd, const QString &kf)
    {
        in_path = in.toStdString();
        password = pwd.toStdString();
        keyfile_path = kf.toStdString();
    }
    void crypto_worker::run()
    {
        std::string mode_str = "pack.";
        if (__mode == mode::unpack)
            mode_str = "unpack.";
        else if (__mode == mode::am_i_evil)
            mode_str = "am_i_evil (verify).";
        pk::core::logger::log("crypto worker thread started; mode: " + mode_str);
        try
        {
            constexpr uint64_t ENTRY_IO_WEIGHT = 32768ULL;
            uint64_t cur_done_entries = 0;
            uint64_t cur_total_entries = 0;
            auto entry_cb = [this, &cur_done_entries, &cur_total_entries](uint64_t done_e, uint64_t tot_e)
            {
                cur_done_entries = done_e;
                cur_total_entries = tot_e;
                emit file_counts(done_e, tot_e);
            };
            // emit the cc for better progress reporting in the
            // crypto_worker()
            auto cb = [this, &cur_done_entries, &cur_total_entries, ENTRY_IO_WEIGHT](uint64_t processed, uint64_t total, const std::string &cur_file)
            {
                uint64_t w_done = processed + cur_done_entries * ENTRY_IO_WEIGHT;
                uint64_t w_tot = total + cur_total_entries * ENTRY_IO_WEIGHT;
                if (w_tot > 0)
                {
                    double frac = std::clamp(static_cast<double>(w_done) / static_cast<double>(w_tot), 0.0, 1.0);
                    int p = 0;
                    if (__mode == mode::pack)
                    {
                        p = std::clamp(16 + static_cast<int>(frac * 84.0), 16, 100);
                    }
                    else
                    {
                        p = std::clamp(6 + static_cast<int>(frac * 94.0), 6, 100);
                    }
                    emit progress(p);
                    emit work_fraction(frac);
                    emit pr_details(processed, total);
                }
                if (!cur_file.empty())
                {
                    emit current_file(QString::fromStdString(cur_file));
                }
            };
            auto status_cb = [this](const std::string &status)
            {
                pk::core::logger::log(status);
                QString qst = QString::fromStdString(status);
                emit current_ac0(qst);
                if (__mode == mode::pack && qst.startsWith("Encrypting", Qt::CaseInsensitive))
                {
                    emit progress(16);
                }
                else if (__mode == mode::unpack && qst.startsWith("Extracting", Qt::CaseInsensitive))
                {
                    emit progress(6);
                }
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
            if (!password.empty())
            {
                pk::mem_::secure_wipe(password.data(), password.size());
                password.clear();
            }
            if (__mode == mode::pack)
            {
                emit current_ac0("Discovering files...");
                emit progress(0);
                std::vector<pk::crypto::format::archive_entry> entries;
                uint64_t discovered_bytes = 0;
                int last_scan_p = 0;
                std::filesystem::path abn___ = std::filesystem::path(output_path).stem();
                std::error_code ec_out;
                auto abs_out = std::filesystem::weakly_canonical(output_path, ec_out);
                auto last_scan_emit_time = std::chrono::steady_clock::now();
                auto last_pct_step_time = last_scan_emit_time;
                std::size_t root_idx = 0;
                const std::size_t total_roots = roots_f_rooted.size();
                auto emit_scan_tick = [&](const std::string &fname)
                {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scan_emit_time).count() >= 50)
                    {
                        last_scan_emit_time = now;
                        const double n_roots = static_cast<double>(std::max<std::size_t>(1, total_roots));
                        const double log_frac = std::min(1.0, std::log10(1.0 + static_cast<double>(entries.size())) / 6.0);
                        const double root_frac = static_cast<double>(root_idx) / n_roots;
                        const double blended = 0.75 * log_frac + 0.25 * root_frac;
                        int target_p = std::clamp(static_cast<int>(blended * 11.0), 0, 11);
                        if (target_p > last_scan_p && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_pct_step_time).count() >= 150)
                        {
                            last_scan_p++;
                            last_pct_step_time = now;
                        }
                        emit progress(last_scan_p);
                        emit scan_stats(entries.size(), discovered_bytes);
                        emit current_ac0(QString("Discovering files... (%1 found)").arg(QString::fromStdString(pk::crypto::am_i_evil::numbers_with_commas_unlike_in_gta_5(entries.size()))));
                        emit current_file(QString::fromStdString(fname));
                    }
                };
                for (const auto &root_str : roots_f_rooted)
                {
                    if (isInterruptionRequested())
                    {
                        pk::core::logger::log("scanning interrupted by user.");
                        return;
                    }
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
#else
                    if (!_include_hidden)
                    {
                        if (root_path.filename().string().front() == '.')
                            continue;
                    }
#endif
                    if (std::filesystem::is_directory(root_path))
                    {
                        {
                            pk::crypto::format::archive_entry dir_ae;
                            dir_ae.source_path = root_path;
                            dir_ae.is_directory = true;
                            dir_ae.file_size = 0;
                            dir_ae.relative_path = (abn___ / root_path.filename()).generic_string();
                            if (__cp_metadata)
                            {
#ifdef _WIN32
                                WIN32_FILE_ATTRIBUTE_DATA rinfo;
                                if (GetFileAttributesExW(root_path.wstring().c_str(), GetFileExInfoStandard, &rinfo))
                                {
                                    dir_ae.ctime = ((uint64_t)rinfo.ftCreationTime.dwHighDateTime << 32) | rinfo.ftCreationTime.dwLowDateTime;
                                    dir_ae.atime = ((uint64_t)rinfo.ftLastAccessTime.dwHighDateTime << 32) | rinfo.ftLastAccessTime.dwLowDateTime;
                                    dir_ae.mtime = ((uint64_t)rinfo.ftLastWriteTime.dwHighDateTime << 32) | rinfo.ftLastWriteTime.dwLowDateTime;
                                    dir_ae.attrs = rinfo.dwFileAttributes;
                                }
#else
                                struct stat rst;
                                if (stat(root_path.string().c_str(), &rst) == 0)
                                {
                                    auto unix2ft = [](time_t t) -> uint64_t
                                    {
                                        return (static_cast<uint64_t>(t) * 10000000ULL) + 116444736000000000ULL;
                                    };
                                    dir_ae.ctime = unix2ft(rst.st_ctime);
                                    dir_ae.atime = unix2ft(rst.st_atime);
                                    dir_ae.mtime = unix2ft(rst.st_mtime);
                                    dir_ae.attrs = rst.st_mode;
                                }
#endif
                            }
                            entries.push_back(std::move(dir_ae));
                        }
                        auto it = std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied);
                        auto end = std::filesystem::recursive_directory_iterator();
                        while (it != end)
                        {
                            if (isInterruptionRequested())
                            {
                                pk::core::logger::log("directory scanning interrupted by user.");
                                return;
                            }
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
#else
                            if (!_include_hidden && dir_entry.path().filename().string().front() == '.')
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
                            // std::filesystem::relative is VERY slow on windows
                            // we know dir_entry is inside root_path, so just strip the prefix
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
                            ae.relative_path = (abn___ / root_path.filename() / rel_str).generic_string();
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
#else
                                struct stat st;
                                if (stat(dir_entry.path().string().c_str(), &st) == 0)
                                {
                                    auto unix2ft = [](time_t t) -> uint64_t
                                    {
                                        return (static_cast<uint64_t>(t) * 10000000ULL) + 116444736000000000ULL;
                                    };
                                    ae.ctime = unix2ft(st.st_ctime);
                                    ae.atime = unix2ft(st.st_atime);
                                    ae.mtime = unix2ft(st.st_mtime);
                                    ae.attrs = st.st_mode;
                                }
#endif
                            }
                            discovered_bytes += ae.file_size;
                            entries.push_back(std::move(ae));
                            emit_scan_tick(dir_entry.path().filename().string());
                            ++it;
                        }
                    }
                    else
                    {
                        uint64_t fsize = std::filesystem::file_size(root_path);
                        pk::crypto::format::archive_entry ae;
                        ae.source_path = root_path;
                        ae.relative_path = (abn___ / root_path.filename()).generic_string();
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
#else
                            struct stat st;
                            if (stat(ae.source_path.string().c_str(), &st) == 0)
                            {
                                auto unix2ft = [](time_t t) -> uint64_t
                                {
                                    return (static_cast<uint64_t>(t) * 10000000ULL) + 116444736000000000ULL;
                                };
                                ae.ctime = unix2ft(st.st_ctime);
                                ae.atime = unix2ft(st.st_atime);
                                ae.mtime = unix2ft(st.st_mtime);
                                ae.attrs = st.st_mode;
                            }
#endif
                        }
                        discovered_bytes += ae.file_size;
                        entries.push_back(std::move(ae));
                        emit_scan_tick(root_path.filename().string());
                    }
                    root_idx++;
                }
                if (isInterruptionRequested())
                {
                    pk::core::logger::log("scanning interrupted by user before packing.");
                    return;
                }
                cur_total_entries = entries.size();
                emit scan_stats(entries.size(), discovered_bytes);
                emit file_counts(0, entries.size());
                emit progress(12);
                emit current_ac0(QString("Discovered %1 item(s). Deriving encryption key...").arg(QString::fromStdString(pk::crypto::am_i_evil::numbers_with_commas_unlike_in_gta_5(entries.size()))));
                emit current_file(" ");
                pk::crypto::format::pack_archive(entries, output_path, final_password, algo, __kdf_cfg, __cp_metadata, cmp_algo, cmp_lvl, static_cast<std::size_t>(cs_mbs) * 1024 * 1024, cb, status_cb, [this]() -> bool
                                                 { return isInterruptionRequested(); }, entry_cb);
                pk::core::logger::log("worker successfully packed archive.");
            }
            else if (__mode == mode::unpack)
            {
                pk::core::logger::log("starting archive unpack...");
                emit progress(0);
                emit current_ac0("Deriving encryption key...");
                int remembered_choice = -1;
                pk::crypto::format::unpack_archive(
                    in_path, output_dir, final_password, ext_behavior_v, ext_overwrite_v,
                    [this, &remembered_choice](const std::string &___filepath) -> int
                    {
                        if (isInterruptionRequested())
                            return 0;
                        if (remembered_choice != -1)
                        {
                            return remembered_choice;
                        }
                        int choice = 0;
                        QMetaObject::invokeMethod(
                            qApp, [this, &choice, &remembered_choice, ___filepath]()
                            {
                                if (isInterruptionRequested())
                                {
                                    choice = 0;
                                    return;
                                }
                                emit need_fg();
                                pk::ui::sfx::play_info();
                                QMessageBox msgBox(QApplication::activeModalWidget());
                                msgBox.setWindowTitle("File conflict");
                                msgBox.setText(QString("The file already exists:\n%1\n\nWhat would you like to do?").arg(QString::fromStdString(___filepath)));
                                QPushButton *btnOver = msgBox.addButton("Overwrite", QMessageBox::ActionRole);
                                QPushButton *btnOverAll = msgBox.addButton("Overwrite all", QMessageBox::ActionRole);
                                QPushButton *btnSkip = msgBox.addButton("Skip", QMessageBox::ActionRole);
                                QPushButton *btnSkipAll = msgBox.addButton("Skip all", QMessageBox::ActionRole);
                                QPushButton *btnCancel = msgBox.addButton("Cancel", QMessageBox::RejectRole);
                                pk::ui::outs::dont_burn_my_eyes(&msgBox);
                                msgBox.exec();
                                if (msgBox.clickedButton() == btnOver)
                                {
                                    choice = 2;
                                }
                                else if (msgBox.clickedButton() == btnOverAll)
                                {
                                    choice = 2;
                                    remembered_choice = 2;
                                }
                                else if (msgBox.clickedButton() == btnSkip)
                                {
                                    choice = 1;
                                }
                                else if (msgBox.clickedButton() == btnSkipAll)
                                {
                                    choice = 1;
                                    remembered_choice = 1;
                                }
                                else
                                {
                                    choice = 0;
                                } },
                            Qt::BlockingQueuedConnection);
                        return choice;
                    },
                    cb, [this]() -> bool
                    {
                        if (isInterruptionRequested())
                            return false;
                        bool proceed = false;
                        QMetaObject::invokeMethod(
                            qApp, [this, &proceed]()
                            { 
                                if (isInterruptionRequested())
                                {
                                    proceed = false;
                                    return;
                                }
                                emit need_fg();
                                pk::ui::sfx::play_info();
                                proceed = pk::ui::outs::ask(QApplication::activeModalWidget(), "Zipbomb warning", "Yo this shit possibly a zipbomb, do you want to proceed extracting?"); },
                            Qt::BlockingQueuedConnection);
                        return proceed; }, status_cb, [this]() -> bool
                    { return isInterruptionRequested(); }, entry_cb);
                pk::core::logger::log("worker successfully unpacked archive.");
            }
            else if (__mode == mode::am_i_evil)
            {
                pk::core::logger::log("starting archive verification (am_i_evil)...");
                emit progress(0);
                emit current_ac0("Deriving encryption key...");
                verify_rep = pk::crypto::am_i_evil::verify_archive(
                    in_path, final_password,
                    [this, &cur_done_entries, &cur_total_entries, ENTRY_IO_WEIGHT](uint64_t processed, uint64_t total, const std::string &status)
                    {
                        QString qstatus = QString::fromStdString(status);
                        emit current_ac0(qstatus);
                        if (!qstatus.contains("Deriving", Qt::CaseInsensitive))
                        {
                            uint64_t w_done = processed + cur_done_entries * ENTRY_IO_WEIGHT;
                            uint64_t w_tot = total + cur_total_entries * ENTRY_IO_WEIGHT;
                            if (w_tot > 0)
                            {
                                double frac = std::clamp(static_cast<double>(w_done) / static_cast<double>(w_tot), 0.0, 1.0);
                                int p = std::clamp(6 + static_cast<int>(frac * 94.0), 6, 100);
                                emit progress(p);
                                emit work_fraction(frac);
                                emit pr_details(processed, total);
                            }
                            else
                            {
                                emit progress(6);
                            }
                        }
                    },
                    [this]() -> bool
                    {
                        return isInterruptionRequested();
                    },
                    [this, &cur_done_entries, &cur_total_entries](const std::string &cur_file, uint64_t done_e, uint64_t tot_e)
                    {
                        cur_done_entries = done_e;
                        cur_total_entries = tot_e;
                        emit file_counts(done_e, tot_e);
                        if (!cur_file.empty())
                        {
                            emit current_file(QString::fromStdString(cur_file));
                        }
                    });
                if (verify_rep.verdict != pk::crypto::am_i_evil::__vv_::success)
                {
                    std::string err = verify_rep.error_details.empty() ? pk::crypto::am_i_evil::verdict_to_str(verify_rep.verdict) : verify_rep.error_details;
                    throw std::runtime_error(err);
                }
                pk::core::logger::log("worker successfully verified archive.");
            }
            emit success();
        }
        catch (const std::exception &e)
        {
            pk::core::logger::log("exception: " + std::string(e.what()));
            emit error(QString::fromStdString(e.what()));
        }
        catch (...)
        {
            pk::core::logger::log("unknown exception caught.");
            emit error(QString::fromStdString(pk::error::format_idk()));
        }
        pk::core::logger::log("crypto worker thread finished.");
    }
}

// end