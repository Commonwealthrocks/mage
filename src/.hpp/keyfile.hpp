// keyfile.hpp
// last updated: 17/06/2026
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "secure_memory.hpp"
namespace pk::crypto::keyfile
{
    // has to be 512 bytes exactly; we feed the keyfile as the argon2id hash itself
    // therefore the app cannot tell if a keyfile or password was used
    void generate(const std::string &path);
    pk::mem_::secure_string read(const std::string &path);
}

// end