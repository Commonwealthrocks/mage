// gui_worker.cpp
// last updated: 31/10/2026
#include "../.hpp/gui_worker.hpp"
#include "../.hpp/fileformat.hpp"
#include "../.hpp/error_msg.hpp"
#include "../.hpp/keyfile.hpp"
#include "../.hpp/secure_memory.hpp"
#include "../.hpp/outs.hpp"
#include "../.hpp/sfx.hpp"
#include "../.hpp/path_handler.hpp"
#include <QApplication>
#include <QByteArray>
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
    static void assign_qstring_to_secure(const QString &src, pk::mem_::secure_string &dst)
    {
        if (!dst.empty())
        {
            pk::mem_::secure_wipe(dst.data(), dst.size());
            dst.clear();
        }
        if (src.isEmpty())
            return;
        QByteArray utf8 = src.toUtf8();
        dst.assign(utf8.constData(), static_cast<std::size_t>(utf8.size()));
        pk::mem_::secure_wipe(utf8.data(), static_cast<std::size_t>(utf8.size()));
        utf8.clear();
    }
    crypto_worker::crypto_worker(mode m, QObject *parent)
        : QThread(parent), __mode(m)
    {
    }
    crypto_worker::~crypto_worker()
    {
        if (!password.empty())
        {
            pk::mem_::secure_wipe(password.data(), password.size());
            password.clear();
        }
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
        assign_qstring_to_secure(password, this->password);
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
        in_path = in.toUtf8().toStdString();
        output_dir = out.toUtf8().toStdString();
        assign_qstring_to_secure(pwd, this->password);
        keyfile_path = kf.toUtf8().toStdString();
        ext_behavior_v = ext_beh;
        ext_overwrite_v = ext_ow;
    }
    void crypto_worker::ss_def_verify_params(const QString &in, const QString &pwd, const QString &kf)
    {
        in_path = in.toUtf8().toStdString();
        assign_qstring_to_secure(pwd, this->password);
        keyfile_path = kf.toUtf8().toStdString();
    }
    void crypto_worker::run()
    {
        std::string mode_str = "pack.";
        if (__mode == mode::unpack)
            mode_str = "unpack.";
        else if (__mode == mode::am_i_evil)
            mode_str = "am_i_evil (verify).";
        pk::core::logger::log("crypto worker thread started; mode: " + mode_str);
        struct pwd_scope_wipe
        {
            pk::mem_::secure_string &p;
            ~pwd_scope_wipe()
            {
                if (!p.empty())
                {
                    pk::mem_::secure_wipe(p.data(), p.size());
                    p.clear();
                }
            }
        } pwd_guard{password};
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
            struct final_pwd_wipe
            {
                pk::mem_::secure_string &fp;
                ~final_pwd_wipe()
                {
                    if (!fp.empty())
                    {
                        pk::mem_::secure_wipe(fp.data(), fp.size());
                        fp.clear();
                    }
                }
            } fp_guard{final_password};
            if (!keyfile_path.empty())
            {
                try
                {
                    auto kf = pk::crypto::keyfile::read(keyfile_path);
                    if (!password.empty())
                    {
                        final_password = pk::mem_::secure_string(password.begin(), password.end());
                        final_password.append(kf);
                        pk::mem_::secure_wipe(kf.data(), kf.size());
                        kf.clear();
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
                std::filesystem::path out_fs_path = pk::path::from_utf8(output_path);
                std::filesystem::path abn___ = out_fs_path.stem();
                const std::string abn_utf8 = pk::path::to_utf8(abn___);
                std::error_code ec_out;
                auto abs_out = pk::path::weakly_canonical_safe(out_fs_path, ec_out);
#ifdef _WIN32
                const std::wstring abs_out_ext = ec_out ? std::wstring() : pk::path::to_ext_win32_path(abs_out);
                const std::wstring abs_out_fname = ec_out ? std::wstring() : abs_out.filename().wstring();
#else
                auto unix2ft = [](time_t t) -> uint64_t
                {
                    constexpr int64_t k_win_epoch_sec = 11644473600LL;
                    const int64_t sec = static_cast<int64_t>(t);
                    if (sec <= -k_win_epoch_sec)
                        return 0ULL;
                    return static_cast<uint64_t>(sec + k_win_epoch_sec) * 10000000ULL;
                };
#endif
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
                    std::filesystem::path root_path = pk::path::from_utf8(root_str);
                    root_path.make_preferred();
                    if (!root_path.has_filename() && root_path.has_parent_path() && root_path.parent_path() != root_path)
                    {
                        root_path = root_path.parent_path();
                    }
                    std::error_code ec_root;
                    auto abs_root = pk::path::weakly_canonical_safe(root_path, ec_root);
#ifdef _WIN32
                    if (!ec_root && !ec_out && _wcsicmp(pk::path::to_ext_win32_path(abs_root).c_str(), abs_out_ext.c_str()) == 0)
#else
                    if (!ec_root && !ec_out && abs_root == abs_out)
#endif
                    {
                        throw std::runtime_error("the output archive cannot be placed inside a directory that is being archived, please choose a different output location.");
                    }
                    const std::string root_fname_utf8 = pk::path::to_utf8(root_path.filename());
                    const std::string root_rel_prefix = abn_utf8.empty() ? root_fname_utf8 : (abn_utf8 + "/" + root_fname_utf8);
#ifdef _WIN32
                    WIN32_FILE_ATTRIBUTE_DATA root_info{};
                    if (!GetFileAttributesExW(pk::path::to_ext_win32_path(root_path).c_str(), GetFileExInfoStandard, &root_info))
                    {
                        throw std::runtime_error("failed to open input file: " + pk::path::to_utf8(root_path));
                    }
                    if (!_include_hidden && (root_info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
                    {
                        root_idx++;
                        continue; // skipp
                    }
                    if (root_info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
                    {
                        root_idx++;
                        continue;
                    }
                    const bool root_is_dir = (root_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
                    std::error_code ec_rst;
                    auto root_sym_st = std::filesystem::symlink_status(root_path, ec_rst);
                    if (ec_rst)
                    {
                        throw std::runtime_error("failed to open input file: " + pk::path::to_utf8(root_path));
                    }
                    if (std::filesystem::is_symlink(root_sym_st))
                    {
                        root_idx++;
                        continue;
                    }
                    if (!_include_hidden)
                    {
                        if (!root_fname_utf8.empty() && root_fname_utf8.front() == '.')
                        {
                            root_idx++;
                            continue;
                        }
                    }
                    const bool root_is_dir = std::filesystem::is_directory(root_sym_st);
                    const bool root_is_reg = std::filesystem::is_regular_file(root_sym_st);
                    if (!root_is_dir && !root_is_reg)
                    {
                        root_idx++;
                        continue;
                    }
#endif
                    if (root_is_dir)
                    {
                        {
                            pk::crypto::format::archive_entry dir_ae;
                            dir_ae.source_path = root_path;
                            dir_ae.is_directory = true;
                            dir_ae.file_size = 0;
                            dir_ae.relative_path = root_rel_prefix;
                            if (__cp_metadata)
                            {
#ifdef _WIN32
                                dir_ae.ctime = ((uint64_t)root_info.ftCreationTime.dwHighDateTime << 32) | root_info.ftCreationTime.dwLowDateTime;
                                dir_ae.atime = ((uint64_t)root_info.ftLastAccessTime.dwHighDateTime << 32) | root_info.ftLastAccessTime.dwLowDateTime;
                                dir_ae.mtime = ((uint64_t)root_info.ftLastWriteTime.dwHighDateTime << 32) | root_info.ftLastWriteTime.dwLowDateTime;
                                dir_ae.attrs = root_info.dwFileAttributes;
#else
                                struct stat rst;
                                if (lstat(root_path.c_str(), &rst) == 0)
                                {
                                    dir_ae.ctime = unix2ft(rst.st_ctime);
                                    dir_ae.atime = unix2ft(rst.st_atime);
                                    dir_ae.mtime = unix2ft(rst.st_mtime);
                                    dir_ae.attrs = rst.st_mode;
                                }
#endif
                            }
                            entries.push_back(std::move(dir_ae));
                        }
#ifdef _WIN32
                        struct scan_frame
                        {
                            std::filesystem::path fs_dir;
                            std::string rel_prefix;
                        };
                        std::vector<scan_frame> dir_stack;
                        dir_stack.push_back({root_path, root_rel_prefix});
                        while (!dir_stack.empty())
                        {
                            if (isInterruptionRequested())
                            {
                                pk::core::logger::log("directory scanning interrupted by user.");
                                return;
                            }
                            scan_frame cur_frame = std::move(dir_stack.back());
                            dir_stack.pop_back();
                            std::wstring search_pat = pk::path::to_ext_win32_path(cur_frame.fs_dir);
                            if (!search_pat.empty() && search_pat.back() != L'\\')
                                search_pat.push_back(L'\\');
                            search_pat.push_back(L'*');

                            WIN32_FIND_DATAW fdata{};
                            HANDLE hFind = FindFirstFileExW(search_pat.c_str(), FindExInfoBasic, &fdata, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
                            if (hFind == INVALID_HANDLE_VALUE)
                            {
                                continue;
                            }
                            std::vector<scan_frame> local_subdirs;
                            {
                                struct find_guard
                                {
                                    HANDLE h;
                                    ~find_guard()
                                    {
                                        if (h != INVALID_HANDLE_VALUE)
                                            FindClose(h);
                                    }
                                } fg{hFind};
                                do
                                {
                                    if (isInterruptionRequested())
                                    {
                                        pk::core::logger::log("directory scanning interrupted by user.");
                                        return;
                                    }
                                    if (fdata.cFileName[0] == L'.' &&
                                        (fdata.cFileName[1] == L'\0' || (fdata.cFileName[1] == L'.' && fdata.cFileName[2] == L'\0')))
                                    {
                                        continue;
                                    }
                                    if (fdata.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
                                    {
                                        continue;
                                    }
                                    if (!_include_hidden && (fdata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
                                    {
                                        continue;
                                    }
                                    std::filesystem::path entry_p = cur_frame.fs_dir / fdata.cFileName;
                                    if (!ec_out && !abs_out_fname.empty() && _wcsicmp(fdata.cFileName, abs_out_fname.c_str()) == 0)
                                    {
                                        if (_wcsicmp(pk::path::to_ext_win32_path(entry_p).c_str(), abs_out_ext.c_str()) == 0)
                                        {
                                            continue;
                                        }
                                    }
                                    const bool is_dir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                                    std::string name_utf8 = pk::path::to_utf8(std::filesystem::path(fdata.cFileName));
                                    pk::crypto::format::archive_entry ae;
                                    ae.source_path = std::move(entry_p);
                                    ae.is_directory = is_dir;
                                    ae.file_size = is_dir ? 0ULL : ((static_cast<uint64_t>(fdata.nFileSizeHigh) << 32) | fdata.nFileSizeLow);
                                    ae.relative_path = cur_frame.rel_prefix + "/" + name_utf8;
                                    if (__cp_metadata)
                                    {
                                        ae.ctime = (static_cast<uint64_t>(fdata.ftCreationTime.dwHighDateTime) << 32) | fdata.ftCreationTime.dwLowDateTime;
                                        ae.atime = (static_cast<uint64_t>(fdata.ftLastAccessTime.dwHighDateTime) << 32) | fdata.ftLastAccessTime.dwLowDateTime;
                                        ae.mtime = (static_cast<uint64_t>(fdata.ftLastWriteTime.dwHighDateTime) << 32) | fdata.ftLastWriteTime.dwLowDateTime;
                                        ae.attrs = fdata.dwFileAttributes;
                                    }
                                    discovered_bytes += ae.file_size;
                                    if (is_dir)
                                    {
                                        local_subdirs.push_back({ae.source_path, ae.relative_path});
                                    }
                                    entries.push_back(std::move(ae));
                                    emit_scan_tick(name_utf8);
                                } while (FindNextFileW(hFind, &fdata));
                            }
                            for (auto it_sub = local_subdirs.rbegin(); it_sub != local_subdirs.rend(); ++it_sub)
                            {
                                dir_stack.push_back(std::move(*it_sub));
                            }
                        }
#else
                        std::error_code ec_it;
                        auto it = std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied, ec_it);
                        auto end = std::filesystem::recursive_directory_iterator();
                        while (!ec_it && it != end)
                        {
                            if (isInterruptionRequested())
                            {
                                pk::core::logger::log("directory scanning interrupted by user.");
                                return;
                            }
                            const auto &dir_entry = *it;
                            std::filesystem::path entry_p = dir_entry.path();
                            entry_p.make_preferred();
                            std::error_code ec_de;
                            auto sym_st = dir_entry.symlink_status(ec_de);
                            if (ec_de || std::filesystem::is_symlink(sym_st))
                            {
                                it.increment(ec_it);
                                continue;
                            }
                            bool is_dir = std::filesystem::is_directory(sym_st);
                            bool is_reg = std::filesystem::is_regular_file(sym_st);
                            const std::string fname_str = pk::path::to_utf8(entry_p.filename());
                            if (!_include_hidden && !fname_str.empty() && fname_str.front() == '.')
                            {
                                if (is_dir)
                                    it.disable_recursion_pending();
                                it.increment(ec_it);
                                continue;
                            }
                            if (!is_dir && !is_reg)
                            {
                                it.increment(ec_it);
                                continue;
                            }
                            if (!ec_out)
                            {
                                std::error_code ec_ent;
                                if (std::filesystem::weakly_canonical(entry_p, ec_ent) == abs_out && !ec_ent)
                                {
                                    it.increment(ec_it);
                                    continue;
                                }
                            }
                            uint64_t fsize = 0;
                            if (!is_dir)
                            {
                                fsize = dir_entry.file_size(ec_de);
                                if (ec_de)
                                {
                                    it.increment(ec_it);
                                    continue;
                                }
                            }
                            pk::crypto::format::archive_entry ae;
                            ae.source_path = entry_p;
                            ae.is_directory = is_dir;
                            ae.file_size = fsize;
                            std::string rel_str = pk::path::to_utf8(entry_p);
                            std::string root_str_gen = pk::path::to_utf8(root_path);
                            if (rel_str.length() > root_str_gen.length() && rel_str.compare(0, root_str_gen.length(), root_str_gen) == 0)
                            {
                                rel_str = rel_str.substr(root_str_gen.length());
                                if (!rel_str.empty() && rel_str[0] == '/')
                                {
                                    rel_str = rel_str.substr(1);
                                }
                            }
                            if (rel_str.empty() || rel_str == ".")
                                rel_str = fname_str;
                            ae.relative_path = root_rel_prefix + "/" + rel_str;
                            if (__cp_metadata)
                            {
                                struct stat st;
                                if (lstat(entry_p.c_str(), &st) == 0)
                                {
                                    ae.ctime = unix2ft(st.st_ctime);
                                    ae.atime = unix2ft(st.st_atime);
                                    ae.mtime = unix2ft(st.st_mtime);
                                    ae.attrs = st.st_mode;
                                }
                            }
                            discovered_bytes += ae.file_size;
                            entries.push_back(std::move(ae));
                            emit_scan_tick(fname_str);
                            it.increment(ec_it);
                        }
#endif
                    }
                    else
                    {
#ifdef _WIN32
                        uint64_t fsize = ((static_cast<uint64_t>(root_info.nFileSizeHigh) << 32) | root_info.nFileSizeLow);
#else
                        std::error_code ec_sz;
                        uint64_t fsize = std::filesystem::file_size(root_path, ec_sz);
                        if (ec_sz)
                            throw std::runtime_error("failed to determine size of input file: " + pk::path::to_utf8(root_path));
#endif
                        pk::crypto::format::archive_entry ae;
                        ae.source_path = root_path;
                        ae.relative_path = root_rel_prefix;
                        ae.is_directory = false;
                        ae.file_size = fsize;
                        if (__cp_metadata)
                        {
#ifdef _WIN32
                            ae.ctime = ((uint64_t)root_info.ftCreationTime.dwHighDateTime << 32) | root_info.ftCreationTime.dwLowDateTime;
                            ae.atime = ((uint64_t)root_info.ftLastAccessTime.dwHighDateTime << 32) | root_info.ftLastAccessTime.dwLowDateTime;
                            ae.mtime = ((uint64_t)root_info.ftLastWriteTime.dwHighDateTime << 32) | root_info.ftLastWriteTime.dwLowDateTime;
                            ae.attrs = root_info.dwFileAttributes;
#else
                            struct stat st;
                            if (lstat(ae.source_path.c_str(), &st) == 0)
                            {
                                ae.ctime = unix2ft(st.st_ctime);
                                ae.atime = unix2ft(st.st_atime);
                                ae.mtime = unix2ft(st.st_mtime);
                                ae.attrs = st.st_mode;
                            }
#endif
                        }
                        discovered_bytes += ae.file_size;
                        entries.push_back(std::move(ae));
                        emit_scan_tick(root_fname_utf8);
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
                pk::crypto::format::pack_archive(entries, out_fs_path, final_password, algo, __kdf_cfg, __cp_metadata, cmp_algo, cmp_lvl, static_cast<std::size_t>(cs_mbs) * 1024 * 1024, cb, status_cb, [this]() -> bool
                                                 { return isInterruptionRequested(); }, entry_cb);
                pk::core::logger::log("worker successfully packed archive.");
            }
            else if (__mode == mode::unpack)
            {
                pk::core::logger::log("starting archive unpack...");
                emit progress(0);
                emit current_ac0("Deriving encryption key...");
                int remembered_choice = (shared_cnflct_choice && *shared_cnflct_choice != -1)
                                            ? *shared_cnflct_choice
                                            : -1;
                pk::crypto::format::unpack_archive(
                    pk::path::from_utf8(in_path), pk::path::from_utf8(output_dir), final_password, ext_behavior_v, ext_overwrite_v,
                    [this, &remembered_choice](const std::string &___filepath) -> int
                    {
                        if (isInterruptionRequested())
                            return 0;
                        if (shared_cnflct_choice && *shared_cnflct_choice != -1)
                        {
                            return *shared_cnflct_choice;
                        }
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
                                QWidget *parent = QApplication::activeModalWidget();
                                if (!parent)
                                    parent = QApplication::activeWindow();
                                bool apply_all = false;
                                choice = pk::ui::outs::ask_file_conflict(parent, QString::fromStdString(___filepath), &apply_all);
                                if (apply_all)
                                {
                                    remembered_choice = choice;
                                    if (shared_cnflct_choice)
                                    {
                                        *shared_cnflct_choice = choice;
                                    }
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
                    pk::path::from_utf8(in_path), final_password,
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
                if (verify_rep.verdict == pk::crypto::am_i_evil::__vv_::cancelled)
                {
                    throw std::runtime_error("operation cancelled by user.");
                }
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