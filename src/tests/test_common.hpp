// test_common.hpp
// last updated: 31/10/2026
#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cmath>
#include <random>
#include <algorithm>
#include "../.hpp/fileformat.hpp"
#include "../.hpp/am_i_evil.hpp"
#include "../.hpp/path_handler.hpp"
#include "../.hpp/argon2id_hashing.hpp"
#include "../.hpp/secure_memory.hpp"
#include "../.hpp/keyfile.hpp"
#include "../.hpp/compression.hpp"
#include "../.hpp/aes_ni.hpp"
#include <sodium.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
namespace pk::test
{
    struct test_record
    {
        int item_id = 0;
        std::string target;
        std::string scope;
        bool passed = false;
        double elapsed_ms = 0.0;
        std::string note;
    };
    extern std::vector<test_record> g_results;
    void run_case(int id, const std::string &target, const std::string &scope, std::function<void()> fn, const std::string &pass_note = "PASS");
    std::filesystem::path get_temp_sandbox();
    void cleanup_tmp_sandbox(const std::filesystem::path &p);
    void run_argon2id_tv(int &cur_id);
    void run_aes256siv_tv(int &cur_id);
    void run_ciphers_tv(int &cur_id);
    void run_roundtrip_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf);
    void run_boundary_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf);
    void run_tamper_tests(int &cur_id, const std::filesystem::path &sandbox, const pk::crypto::kdf::kdf_cfg &fast_kdf);
    void run_zipslip_tests(int &cur_id);
    void run_resource_limits_tests(int &cur_id, const std::filesystem::path &sandbox);
    void fuzz___inproc_test(int &cur_id, const std::filesystem::path &sandbox);
}
