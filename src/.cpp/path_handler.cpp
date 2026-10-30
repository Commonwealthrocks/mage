// path_handler.cpp
// last updated: 30/10/2026
#include "../.hpp/path_handler.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
// bars?
#endif
#include <windows.h>
#endif
namespace pk::path
{
    namespace
    {
        constexpr std::array<std::string_view, 22> k_reserved_names = {
            // why accept reserved names? hell even con.txt is just as bad security wise as con
            "CON",
            "PRN",
            "AUX",
            "NUL",
            "COM1",
            "COM2",
            "COM3",
            "COM4",
            "COM5",
            "COM6",
            "COM7",
            "COM8",
            "COM9",
            "LPT1",
            "LPT2",
            "LPT3",
            "LPT4",
            "LPT5",
            "LPT6",
            "LPT7",
            "LPT8",
            "LPT9",
        };
        [[nodiscard]] std::string to_upper_ascii(std::string_view s)
        {
            std::string out(s);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
                           { return static_cast<char>(std::toupper(c)); });
            return out;
        }
        [[nodiscard]] std::vector<std::string_view> split_segments(std::string_view input)
        {
            std::vector<std::string_view> segments;
            std::size_t start = 0;
            for (std::size_t i = 0; i <= input.size(); ++i)
            {
                if (i == input.size() || input[i] == '/')
                {
                    segments.push_back(input.substr(start, i - start));
                    start = i + 1;
                }
            }
            return segments;
        }
        [[nodiscard]] bool if_backslash(std::string_view s) noexcept
        {
            return s.find('\\') != std::string_view::npos;
        }
        [[nodiscard]] bool if_null_bs(std::string_view s) noexcept
        {
            return s.find('\0') != std::string_view::npos;
        }
        [[nodiscard]] bool if_c_chars(std::string_view s) noexcept
        {
            for (unsigned char c : s)
            {
                if ((c >= 0x01 && c <= 0x1F) || c == 0x7F)
                {
                    return true;
                }
            }
            return false;
        }
        [[nodiscard]] bool absolute_or_rooted(std::string_view s) noexcept
        // "\\\\?\\C:\\..." to check any and all win32 based paths
        {
            if (s.empty())
                return false;
            if (s.front() == '/')
                return true;
            if (s.front() == '\\')
                return true;
            if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':')
            {
                return true;
            }
            return false;
        }
    }
    std::string describe(validation_error err)
    {
        switch (err)
        {
        case validation_error::none:
            return "OK?";
        case validation_error::empty_path:
            return "path is empty.";
        case validation_error::absolute_path:
            return "path is absolute or contains a drive / UNC prefix.";
        case validation_error::null_byte:
            return "path contains an embedded null byte.";
        case validation_error::c_char:
            return "path contains a control character.";
        case validation_error::parent_traversal:
            return "path attempts to traverse outside the base directory.";
        case validation_error::empty_segment:
            return "path contains an empty segment (double separator or leading / trailing separator).";
        case validation_error::dot_segment:
            return "path contains a '.' segment.";
        case validation_error::reserved_name:
            return "path contains a Windows reserved device name.";
        case validation_error::trailing_dot_or_space:
            return "a path segment ends with '.' or ' '; which is invalid on Windows.";
        case validation_error::path_too_long:
            return "path exceeds the maximum allowed length.";
        case validation_error::segment_too_long:
            return "a path segment exceeds the maximum allowed length.";
        case validation_error::symlink_escape:
            return "resolved path escapes the base directory via a symlink.";
        case validation_error::mixed_slashes:
            return "path contains a messy mixture of slashes (e.g. '\\/') or repeated slashes.";
        case validation_error::backslash_in_path:
            return "path contains a backslash '\\' (please use forward slashes '/' instead).";
        case validation_error::path_conflict:
            return "path conflicts with an existing file or directory.";
        }
        return "i dunno really";
    }
    bool reserved_name(std::string_view segment) noexcept
    {
        if (segment.empty())
            return false;
        std::string_view base = segment;
        if (auto dot = base.find('.'); dot != std::string_view::npos)
        {
            base = base.substr(0, dot);
        }
        if (base.empty() || base.size() > 4)
            return false;
        std::string upper = to_upper_ascii(base);
        return std::find(k_reserved_names.begin(), k_reserved_names.end(), upper) != k_reserved_names.end();
    }
    validation_error validate_archive_path(
        std::string_view input,
        std::string &out_norm,
        const validation_options &opts)
    {
        if (input.empty())
        {
            return validation_error::empty_path;
        }
        if (if_null_bs(input))
        {
            return validation_error::null_byte;
        }
        if (if_c_chars(input))
        {
            return validation_error::c_char;
        }
        if (input.size() > opts.max_path_length)
        {
            return validation_error::path_too_long;
        }
        if (if_backslash(input))
        {
            return validation_error::backslash_in_path;
        }
        if (absolute_or_rooted(input))
        {
            return validation_error::absolute_path;
        }
        std::vector<std::string_view> raw_segments = split_segments(input);
        std::vector<std::string_view> resolved_stack;
        resolved_stack.reserve(raw_segments.size());
        for (std::string_view seg : raw_segments)
        {
            if (seg.empty())
            {
                return validation_error::empty_segment;
            }
            if (seg == ".")
            {
                return validation_error::dot_segment;
            }
            if (seg == "..")
            {
                if (resolved_stack.empty())
                {
                    return validation_error::parent_traversal;
                }
                resolved_stack.pop_back();
                continue;
            }
            if (seg.size() > opts.max_segment_length)
            {
                return validation_error::segment_too_long;
            }
            if (seg.back() == '.' || seg.back() == ' ')
            {
                return validation_error::trailing_dot_or_space;
            }
            if (seg.front() == ' ')
            {
                return validation_error::trailing_dot_or_space;
            }
            if (opts.case_reserved_chc && reserved_name(seg))
            {
                return validation_error::reserved_name;
            }
            resolved_stack.push_back(seg);
        }
        if (resolved_stack.empty())
        {
            return validation_error::empty_path;
        }
        std::ostringstream oss;
        for (std::size_t i = 0; i < resolved_stack.size(); ++i)
        {
            if (i != 0)
                oss << '/';
            oss << resolved_stack[i];
        }
        std::string normalized = oss.str();
        if (normalized.size() > opts.max_path_length)
        {
            return validation_error::path_too_long;
        }
        out_norm = std::move(normalized);
        return validation_error::none;
    }
    // this is a pretty relevant issue i'd say that took me THIS long to fix
    // but on Windows the MAX_PATH len is around 260 characters so anything
    // over causes a fail and for MAGE to kill itself; therefore due to the
    // criss-cross-apple-sauce of my compiler toolchain (MSYS2 UCRT64) it can
    // conflict when appending a PATH, so we'd get paths mixed with slashes (/)
    // and backslashes (\) so the PATHS themselves would look messy and really
    // fucking buggy and possibly another exploitable angle for the MAGE1
    // fileformat
    std::filesystem::path from_utf8(std::string_view s)
    {
        if (s.empty())
            return {};
#ifdef _WIN32
        int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (needed <= 0)
            return std::filesystem::path(std::string(s));
        std::wstring ws(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), needed);
        return std::filesystem::path(std::move(ws));
#else
        return std::filesystem::path(s);
#endif
    }
    std::string to_utf8(const std::filesystem::path &p)
    {
#ifdef _WIN32
        const std::wstring &ws = p.native();
        if (ws.empty())
            return {};
        int needed = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
            return {};
        std::string out(static_cast<std::size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
        return out;
#else
        return p.string();
#endif
    }
    std::string display_path_utf8(const std::filesystem::path &p)
    {
        if (p.empty())
            return {};
#ifdef _WIN32
        std::wstring ws = p.native();
        if (ws.rfind(L"\\\\?\\UNC\\", 0) == 0 || ws.rfind(L"\\\\?\\unc\\", 0) == 0)
        {
            ws = L"\\\\" + ws.substr(8);
        }
        else if (ws.rfind(L"\\\\?\\", 0) == 0)
        {
            ws = ws.substr(4);
        }
        for (auto &ch : ws)
        {
            if (ch == L'/')
                ch = L'\\';
        }
        return to_utf8(std::filesystem::path(std::move(ws)));
#else
        std::filesystem::path norm = p;
        norm.make_preferred();
        return norm.string();
#endif
    }
    validation_error validate_host_path(
        std::string_view input,
        const validation_options &opts)
    {
        if (input.empty())
            return validation_error::empty_path;
        if (if_null_bs(input))
            return validation_error::null_byte;
        if (if_c_chars(input))
            return validation_error::c_char;
#ifdef _WIN32
        const std::size_t max_len = std::max<std::size_t>(opts.max_path_length, 32767);
#else
        const std::size_t max_len = opts.max_path_length;
#endif
        if (input.size() > max_len)
            return validation_error::path_too_long;

#ifdef _WIN32
        const bool has_fwd = input.find('/') != std::string_view::npos;
        const bool has_back = input.find('\\') != std::string_view::npos;
        if (has_fwd && has_back)
        {
            return validation_error::mixed_slashes;
        }
        if (input.find("//") != std::string_view::npos)
        {
            return validation_error::mixed_slashes;
        }
        std::string_view body = input;
        if (body.rfind("\\\\?\\UNC\\", 0) == 0 || body.rfind("\\\\?\\unc\\", 0) == 0)
            body.remove_prefix(8);
        else if (body.rfind("\\\\?\\", 0) == 0 || body.rfind("\\\\.\\", 0) == 0)
            body.remove_prefix(4);
        else if (body.rfind("\\\\", 0) == 0)
            body.remove_prefix(2);
        if (body.find("\\\\") != std::string_view::npos)
        {
            return validation_error::mixed_slashes;
        }
#else
        if (if_backslash(input))
            return validation_error::backslash_in_path;
        if (input.find("//") != std::string_view::npos)
        {
            return validation_error::mixed_slashes;
        }
        std::string_view body = input;
#endif
        std::size_t start = 0;
        for (std::size_t i = 0; i <= body.size(); ++i)
        {
            if (i == body.size() || body[i] == '/' || body[i] == '\\')
            {
                std::string_view seg = body.substr(start, i - start);
                start = i + 1;
                if (seg.empty())
                    continue;
#ifdef _WIN32
                if (seg.size() == 2 && std::isalpha(static_cast<unsigned char>(seg[0])) && seg[1] == ':')
                    continue;
#endif
                if (seg == "." || seg == "..")
                    continue;
#ifdef _WIN32
                if (from_utf8(seg).native().size() > opts.max_segment_length)
                    return validation_error::segment_too_long;
#else
                if (seg.size() > opts.max_segment_length)
                    return validation_error::segment_too_long;
#endif
                if (seg.back() == '.' || seg.back() == ' ')
                    return validation_error::trailing_dot_or_space;
                if (seg.front() == ' ')
                    return validation_error::trailing_dot_or_space;
                if (opts.case_reserved_chc && reserved_name(seg))
                    return validation_error::reserved_name;
            }
        }
        return validation_error::none;
    }
#ifdef _WIN32
    namespace
    {
        [[nodiscard]] std::wstring strip_extended_prefix(std::wstring_view ws)
        {
            if (ws.rfind(L"\\\\?\\UNC\\", 0) == 0 || ws.rfind(L"\\\\?\\unc\\", 0) == 0)
            {
                return L"\\\\" + std::wstring(ws.substr(8));
            }
            if (ws.rfind(L"\\\\?\\", 0) == 0)
            {
                return std::wstring(ws.substr(4));
            }
            return std::wstring(ws);
        }
        [[nodiscard]] std::wstring normalize_win32_fp_pls(const std::filesystem::path &p, DWORD *out_err = nullptr)
        {
            if (out_err)
                *out_err = ERROR_SUCCESS;
            std::wstring ws = strip_extended_prefix(p.native());
            if (ws.empty())
                return {};
            for (auto &ch : ws)
            {
                if (ch == L'/')
                    ch = L'\\';
            }
            if (ws.rfind(L"\\\\.\\", 0) == 0)
                return ws;
            DWORD needed = GetFullPathNameW(ws.c_str(), 0, nullptr, nullptr);
            if (needed == 0)
            {
                if (out_err)
                    *out_err = GetLastError();
                return ws;
            }
            std::wstring full(static_cast<std::size_t>(needed), L'\0');
            DWORD written = GetFullPathNameW(ws.c_str(), needed, full.data(), nullptr);
            if (written == 0 || written >= needed)
            {
                if (out_err)
                    *out_err = GetLastError();
                return ws;
            }
            full.resize(static_cast<std::size_t>(written));
            return full;
        }
    }
    std::wstring to_ext_win32_path(const std::filesystem::path &p)
    {
        if (p.empty())
            return {};
        const std::wstring &raw = p.native();
        if (raw.rfind(L"\\\\?\\", 0) == 0 || raw.rfind(L"\\\\.\\", 0) == 0)
        {
            std::wstring copy = raw;
            for (auto &ch : copy)
            {
                if (ch == L'/')
                    ch = L'\\';
            }
            return copy;
        }
        std::wstring full = normalize_win32_fp_pls(p);
        if (full.empty())
            return {};
        if (full.rfind(L"\\\\?\\", 0) == 0 || full.rfind(L"\\\\.\\", 0) == 0)
            return full;
        if (full.size() >= 2 && full[0] == L'\\' && full[1] == L'\\')
            return L"\\\\?\\UNC\\" + full.substr(2);
        return L"\\\\?\\" + full;
    }

    std::filesystem::path to_ext_path(const std::filesystem::path &p)
    {
        return std::filesystem::path(to_ext_win32_path(p));
    }
#endif
    std::filesystem::path weakly_canonical_safe(const std::filesystem::path &p, std::error_code &ec)
    {
        ec.clear();
        if (p.empty())
            return {};
#ifdef _WIN32
        DWORD full_err = ERROR_SUCCESS;
        std::wstring full = normalize_win32_fp_pls(p, &full_err);
        if (full.empty() || full_err != ERROR_SUCCESS)
        {
            ec = std::error_code(static_cast<int>(full_err != ERROR_SUCCESS ? full_err : ERROR_INVALID_NAME), std::system_category());
            return {};
        }
        while (full.size() > 3 && (full.back() == L'\\' || full.back() == L'/'))
        {
            full.pop_back();
        }
        std::vector<std::wstring> tail_parts;
        std::filesystem::path cur(full);
        while (true)
        {
            std::wstring ext_cur = to_ext_win32_path(cur);
            DWORD attrs = GetFileAttributesW(ext_cur.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES)
            {
                std::filesystem::path resolved_base;
                HANDLE h = CreateFileW(ext_cur.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                if (h != INVALID_HANDLE_VALUE)
                {
                    DWORD needed = GetFinalPathNameByHandleW(h, nullptr, 0, VOLUME_NAME_DOS);
                    if (needed > 0)
                    {
                        std::wstring buf(static_cast<std::size_t>(needed), L'\0');
                        DWORD written = GetFinalPathNameByHandleW(h, buf.data(), needed, VOLUME_NAME_DOS);
                        if (written > 0 && written < needed)
                        {
                            buf.resize(static_cast<std::size_t>(written));
                            resolved_base = std::filesystem::path(strip_extended_prefix(buf));
                        }
                    }
                    CloseHandle(h);
                }
                if (resolved_base.empty())
                {
                    if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    {
                        ec = std::error_code(ERROR_ACCESS_DENIED, std::system_category());
                        return {};
                    }
                    resolved_base = cur;
                }
                for (auto it = tail_parts.rbegin(); it != tail_parts.rend(); ++it)
                {
                    resolved_base /= *it;
                }
                return resolved_base.lexically_normal();
            }
            std::filesystem::path parent = cur.parent_path();
            if (parent.empty() || parent == cur)
            {
                ec = std::error_code(ERROR_PATH_NOT_FOUND, std::system_category());
                return {};
            }
            std::wstring fname = cur.filename().wstring();
            if (!fname.empty())
                tail_parts.push_back(std::move(fname));
            cur = std::move(parent);
        }
#else
        return std::filesystem::weakly_canonical(p, ec);
#endif
    }
    bool mk_dirs_safe(const std::filesystem::path &p, std::error_code &ec)
    {
        ec.clear();
        if (p.empty())
            return true;
#ifdef _WIN32
        std::wstring ext = to_ext_win32_path(p);
        while (ext.size() > 7 && ext.back() == L'\\')
        {
            ext.pop_back();
        }
        DWORD attrs = GetFileAttributesW(ext.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES)
        {
            if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                return true;
            ec = std::error_code(ERROR_ALREADY_EXISTS, std::system_category());
            return false;
        }
        std::size_t start = 0;
        if (ext.rfind(L"\\\\?\\UNC\\", 0) == 0 || ext.rfind(L"\\\\?\\unc\\", 0) == 0)
        {
            std::size_t s1 = ext.find(L'\\', 8);
            if (s1 != std::wstring::npos)
            {
                std::size_t s2 = ext.find(L'\\', s1 + 1);
                start = (s2 != std::wstring::npos) ? (s2 + 1) : ext.size();
            }
            else
            {
                start = ext.size();
            }
        }
        else if (ext.rfind(L"\\\\?\\", 0) == 0 && ext.size() >= 6 && ext[5] == L':')
        {
            start = (ext.size() >= 7 && ext[6] == L'\\') ? 7 : 6;
        }
        for (std::size_t pos = start; pos <= ext.size(); ++pos)
        {
            if (pos == ext.size() || ext[pos] == L'\\')
            {
                if (pos <= start)
                    continue;
                std::wstring sub = ext.substr(0, pos);
                DWORD sub_attrs = GetFileAttributesW(sub.c_str());
                if (sub_attrs == INVALID_FILE_ATTRIBUTES)
                {
                    if (!CreateDirectoryW(sub.c_str(), nullptr))
                    {
                        DWORD err = GetLastError();
                        if (err != ERROR_ALREADY_EXISTS)
                        {
                            ec = std::error_code(static_cast<int>(err), std::system_category());
                            return false;
                        }
                    }
                }
                else if ((sub_attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    ec = std::error_code(ERROR_ALREADY_EXISTS, std::system_category());
                    return false;
                }
            }
        }
        return true;
#else
        std::filesystem::create_directories(p, ec);
        return !ec;
#endif
    }
    bool rfs___(const std::filesystem::path &p, std::error_code &ec) noexcept
    {
        ec.clear();
        if (p.empty())
            return false;
#ifdef _WIN32
        std::wstring ext = to_ext_win32_path(p);
        if (DeleteFileW(ext.c_str()))
            return true;
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            return false;
        ec = std::error_code(static_cast<int>(err), std::system_category());
        return false;
#else
        return std::filesystem::remove(p, ec);
#endif
    }
    validation_error resolve_out_path(
        const std::filesystem::path &base_dir,
        std::string_view norm_archive_path,
        std::filesystem::path &out_absolute_path,
        std::error_code &fs_error)
    {
        fs_error.clear();
        std::string recheck;
        if (auto err = validate_archive_path(norm_archive_path, recheck);
            !ok(err))
        {
            return err;
        }
        if (recheck != norm_archive_path)
        {
            return validation_error::parent_traversal;
        }
        std::filesystem::path base_canonical = weakly_canonical_safe(base_dir, fs_error);
        if (fs_error || base_canonical.empty())
        {
            return validation_error::symlink_escape;
        }
        fs_error.clear();
        std::filesystem::path current = base_canonical;
        std::vector<std::string_view> segments = split_segments(norm_archive_path);
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            current /= from_utf8(segments[i]);
            bool exists = std::filesystem::exists(to_ext_path(current), fs_error);
            if (fs_error)
            {
                return validation_error::symlink_escape;
            }
            if (!exists)
            {
                continue;
            }
            if (is_symlink_or_other_thingy_whatever(current, fs_error))
            {
                return validation_error::symlink_escape;
            }

            if (i < segments.size() - 1)
            {
                bool is_dir = std::filesystem::is_directory(to_ext_path(current), fs_error);
                if (fs_error)
                    return validation_error::symlink_escape;
                if (!is_dir)
                {
                    return validation_error::path_conflict;
                }
            }
        }
        std::filesystem::path candidate = current.lexically_normal();
        auto base_str = base_canonical.lexically_normal().native();
        auto cand_str = candidate.native();
        if (cand_str.size() < base_str.size() ||
            cand_str.compare(0, base_str.size(), base_str) != 0)
        {
            return validation_error::parent_traversal;
        }
        if (cand_str.size() > base_str.size())
        {
            const bool base_has_sep = !base_str.empty() &&
                                      (base_str.back() == std::filesystem::path::preferred_separator
#ifdef _WIN32
                                       || base_str.back() == L'/'
#endif
                                      );
            if (!base_has_sep)
            {
                auto next_char = cand_str[base_str.size()];
                if (next_char != std::filesystem::path::preferred_separator)
                {
                    return validation_error::parent_traversal;
                }
            }
        }
        out_absolute_path = std::move(candidate);
        return validation_error::none;
    }
    bool is_symlink_or_other_thingy_whatever(const std::filesystem::path &p, std::error_code &ec) noexcept
    {
        ec.clear();
        std::error_code status_ec;
        auto st = std::filesystem::symlink_status(to_ext_path(p), status_ec);
        if (status_ec || st.type() == std::filesystem::file_type::not_found)
        {
            return false;
        }
        if (st.type() == std::filesystem::file_type::symlink)
        {
            return true;
        }
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(to_ext_win32_path(p).c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            return true;
        }
#endif
        return false;
    }
    bool is_symlink_or_other_thingy_whatever(const std::filesystem::path &p) noexcept
    {
        std::error_code ec;
        return is_symlink_or_other_thingy_whatever(p, ec);
    }
}

// end