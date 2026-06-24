// path_handler.cpp
// last updated: 17/06/2026
#include "../.hpp/path_handler.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <vector>
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
        if (input.size() > opts.max_path_length)
            return validation_error::path_too_long;
        if (if_backslash(input))
            return validation_error::backslash_in_path;

        if (input.find("\\/") != std::string_view::npos || input.find("/\\") != std::string_view::npos)
        {
            return validation_error::mixed_slashes;
        }
        if (input.find("//") != std::string_view::npos)
        {
            return validation_error::mixed_slashes;
        }
        std::filesystem::path p(input);
        for (auto it = p.begin(); it != p.end(); ++it)
        {
            std::string seg = it->string();
            if (seg == "/" || seg == "\\")
                continue;
            if (seg.size() >= 2 && seg[1] == ':')
                continue;
            if (seg == "." || seg == "..")
                continue;
            if (seg.size() > opts.max_segment_length)
                return validation_error::segment_too_long;
            if (seg.back() == '.' || seg.back() == ' ')
                return validation_error::trailing_dot_or_space;
            if (seg.front() == ' ')
                return validation_error::trailing_dot_or_space;
            if (opts.case_reserved_chc && reserved_name(seg))
                return validation_error::reserved_name;
        }
        return validation_error::none;
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
        std::filesystem::path base_canonical = std::filesystem::weakly_canonical(base_dir, fs_error);
        if (fs_error)
        {
            return validation_error::symlink_escape;
        }
        fs_error.clear();
        std::filesystem::path current = base_canonical;
        std::vector<std::string_view> segments = split_segments(norm_archive_path);
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            current /= std::filesystem::path(std::string(segments[i]));
            bool exists = std::filesystem::exists(current, fs_error);
            if (fs_error)
            {
                return validation_error::symlink_escape;
            }
            if (!exists)
            {
                continue;
            }
            bool is_symlink = std::filesystem::is_symlink(current, fs_error);
            if (fs_error)
            {
                return validation_error::symlink_escape;
            }

            if (is_symlink)
            {
                return validation_error::symlink_escape;
            }

            if (i < segments.size() - 1)
            {
                bool is_dir = std::filesystem::is_directory(current, fs_error);
                if (fs_error)
                    return validation_error::symlink_escape;
                if (!is_dir)
                {
                    return validation_error::path_conflict;
                }
            }
        }
        std::filesystem::path candidate = (base_canonical / std::filesystem::path(std::string(norm_archive_path))).lexically_normal();
        auto base_str = base_canonical.lexically_normal().native();
        auto cand_str = candidate.native();
        if (cand_str.size() < base_str.size() ||
            cand_str.compare(0, base_str.size(), base_str) != 0)
        {
            return validation_error::parent_traversal;
        }
        if (cand_str.size() > base_str.size())
        {
            auto next_char = cand_str[base_str.size()];
            if (next_char != std::filesystem::path::preferred_separator)
            {
                return validation_error::parent_traversal;
            }
        }
        out_absolute_path = std::move(candidate);
        return validation_error::none;
    }
}

// end