// sfx.hpp
// last updated: 17/06/2026
#pragma once
#include <string>
namespace pk::ui::sfx
{
    void preload();
    void play_success();
    void play_error();
    void play_info();
    void play_by_name(const std::string &name);
}

// end