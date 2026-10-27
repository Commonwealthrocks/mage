// error_msg.hpp
// last updated: 27/10/2026
#pragma once
#include <string>
namespace pk::error
{
    struct error_desc
    {
        std::string title;
        std::string message;
        std::string suggestion;
        std::string technical;
    };
    error_desc parse(const std::string &what);
    error_desc parse_idk();
    std::string format(const std::string &what);
    std::string format_idk();
}

// end