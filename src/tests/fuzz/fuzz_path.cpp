// fuzz_path.cpp
// last updated: 31/10/2026
// compile with:
//   clang++ -O1 -g -fsanitize=fuzzer,address,undefined -Isrc/.hpp tests/fuzz/fuzz_path.cpp src/.cpp/path_handler.cpp -o fuzz_path
//////////////////////////////
// sudo apt update && sudo apt install -y clang libsodium-dev libssl-dev libzstd-dev liblzma-dev libargon2-dev
// mkdir -p tests/fuzz/corpus_path tests/fuzz/corpus_header
// clang++ -O1 -g -fsanitize=fuzzer,address,undefined -I.hpp tests/fuzz/fuzz_path.cpp .cpp/path_handler.cpp -o fuzz_path ./fuzz_path -max_len=512 -timeout=5 -rss_limit_mb=1024 -workers=4 -jobs=4
#include <cstdint>
#include <cstddef>
#include <string>
#include "../../.hpp/path_handler.hpp"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 4096)
        return 0;
    std::string input(reinterpret_cast<const char *>(data), size);
    std::string normalized;
    bool ok = pk::path::ok(pk::path::validate_archive_path(input, normalized));
    if (ok)
    {
        if (normalized.empty() || normalized.front() == '/' || normalized.front() == '\\' ||
            normalized.find(':') != std::string::npos || normalized.find('\\') != std::string::npos)
        {
            __builtin_trap();
        }
        std::size_t seg_start = 0;
        for (std::size_t i = 0; i <= normalized.size(); ++i)
        {
            if (i == normalized.size() || normalized[i] == '/')
            {
                std::string_view seg(normalized.data() + seg_start, i - seg_start);
                seg_start = i + 1;
                if (seg == "." || seg == "..")
                {
                    __builtin_trap();
                }
            }
        }
    }

    return 0;
}

// end