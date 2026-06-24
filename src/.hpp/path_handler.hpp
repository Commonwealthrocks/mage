// path_handler.cpp
// last updated: 17/06/2026
#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <system_error>
namespace pk::path
{
    enum class validation_error
    {
        none = 0,
        empty_path,
        absolute_path,
        null_byte,
        c_char,
        parent_traversal,
        empty_segment,
        dot_segment,
        reserved_name,
        trailing_dot_or_space,
        path_too_long, // says enough
        segment_too_long,
        symlink_escape,
        mixed_slashes,
        backslash_in_path,
        path_conflict,
    };
    [[nodiscard]] inline bool
    ok(validation_error err) noexcept
    {
        return err == validation_error::none;
    }
    [[nodiscard]] std::string describe(validation_error err);
    struct validation_options
    {
        std::size_t max_path_length = 4096;
        std::size_t max_segment_length = 255;
        bool case_reserved_chc = true;
    };
    [[nodiscard]] validation_error validate_archive_path(
        std::string_view input,
        std::string &out_norm,
        const validation_options &opts = {});
    [[nodiscard]] validation_error validate_host_path(
        std::string_view input,
        const validation_options &opts = {});
    [[nodiscard]] validation_error resolve_out_path(
        const std::filesystem::path &base_dir,
        std::string_view norm_archive_path,
        std::filesystem::path &out_absolute_path,
        std::error_code &fs_error);
    [[nodiscard]] bool reserved_name(std::string_view segment) noexcept;
}

// end