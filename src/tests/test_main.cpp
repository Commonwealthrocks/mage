// test_main.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    std::vector<test_record> g_results;

    int calculate_col_width(const std::string &header, const std::vector<std::string> &entries, int min_w, int pad = 1)
    {
        int max_len = static_cast<int>(header.length());
        for (const auto &e : entries)
        {
            max_len = std::max(max_len, static_cast<int>(e.length()));
        }
        return std::max(min_w, max_len + (pad * 2));
    }
    std::string center(const std::string &text, int width)
    {
        int len = static_cast<int>(text.length());
        if (len >= width)
            return text;
        int tot = width - len;
        int left = tot / 2;
        int right = tot - left;
        return std::string(left, ' ') + text + std::string(right, ' ');
    }
    void run_case(int id, const std::string &target, const std::string &scope, std::function<void()> fn, const std::string &pass_note)
    {
        test_record rec;
        rec.item_id = id;
        rec.target = target;
        rec.scope = scope;
        rec.note = pass_note;
        auto t0 = std::chrono::high_resolution_clock::now();
        try
        {
            fn();
            rec.passed = true;
        }
        catch (const std::exception &ex)
        {
            rec.passed = false;
            rec.note = std::string("FAIL: ") + ex.what();
        }
        catch (...)
        {
            rec.passed = false;
            rec.note = "FAIL: unknown exception";
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        rec.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        g_results.push_back(std::move(rec));
    }
    std::filesystem::path get_temp_sandbox()
    {
        std::error_code ec;
        auto tmp = std::filesystem::temp_directory_path(ec) / "mage_test_sandbox";
        std::filesystem::remove_all(tmp, ec);
        std::filesystem::create_directories(tmp, ec);
        return tmp;
    }
    void cleanup_temp_sandbox(const std::filesystem::path &p)
    {
        std::error_code ec;
        std::filesystem::remove_all(p, ec);
    }
}
int main()
{
    if (sodium_init() < 0)
    {
        std::cerr << "[-] Error: sodium_init() failed in test runner.\n";
        return 1;
    }
    auto now_time = std::chrono::system_clock::now();
    std::time_t rawtime = std::chrono::system_clock::to_time_t(now_time);
    std::tm local_tm{};
    std::tm utc_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &rawtime);
    gmtime_s(&utc_tm, &rawtime);
    constexpr const char *k_platform_name = "Windows (Win32)";
#else
    localtime_r(&rawtime, &local_tm);
    gmtime_r(&rawtime, &utc_tm);
#if defined(__APPLE__)
    constexpr const char *k_platform_name = "macOS (POSIX)";
#elif defined(__linux__)
    constexpr const char *k_platform_name = "Linux (POSIX)";
#else
    constexpr const char *k_platform_name = "POSIX";
#endif
#endif
    int day_diff = 0;
    if (local_tm.tm_year == utc_tm.tm_year)
        day_diff = local_tm.tm_yday - utc_tm.tm_yday;
    else if (local_tm.tm_year > utc_tm.tm_year)
        day_diff = 1;
    else
        day_diff = -1;
    const int offset_mins = day_diff * 24 * 60 + (local_tm.tm_hour * 60 + local_tm.tm_min) - (utc_tm.tm_hour * 60 + utc_tm.tm_min);
    const char tz_sign = (offset_mins >= 0) ? '+' : '-';
    const int abs_mins = std::abs(offset_mins);
    const int tz_h = abs_mins / 60;
    const int tz_m = abs_mins % 60;
    static const char *const k_months[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};
    const char *month_name = (local_tm.tm_mon >= 0 && local_tm.tm_mon < 12) ? k_months[local_tm.tm_mon] : "unknown";
    char timebuf[128];
    std::snprintf(timebuf, sizeof(timebuf), "%02d. %s %04d, %02d:%02d:%02d (UTC%c%02d:%02d)", local_tm.tm_mday, month_name, local_tm.tm_year + 1900, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec, tz_sign, tz_h, tz_m);
    auto sandbox = pk::test::get_temp_sandbox();
    int cur_id = 1;
    pk::crypto::kdf::kdf_cfg fast_kdf{};
    fast_kdf.memory_cost_kb = 1024;
    fast_kdf.time_cost = 1;
    fast_kdf.parallelism = 1;
    fast_kdf.hash_length = 32;
    pk::test::run_argon2id_tv(cur_id);
    pk::test::run_aes256siv_tv(cur_id);
    pk::test::run_ciphers_tv(cur_id);
    pk::test::run_roundtrip_tests(cur_id, sandbox, fast_kdf);
    pk::test::run_boundary_tests(cur_id, sandbox, fast_kdf);
    pk::test::run_tamper_tests(cur_id, sandbox, fast_kdf);
    pk::test::run_zipslip_tests(cur_id);
    pk::test::run_resource_limits_tests(cur_id, sandbox);
    pk::test::fuzz___inproc_test(cur_id, sandbox);
    pk::test::cleanup_temp_sandbox(sandbox);
    std::cout << "MAGE test suite n' security verification v0.7a (" << k_platform_name << ", build " << __DATE__ << ")\n";
    std::cout << "Test execution log from " << timebuf << "\n\n";
    std::cout << "[!] NOTICE\n";
    std::cout << "    Test suite executes native cryptography, format geometry, zip slip defenses,\n";
    std::cout << "    and 100,000 in-process parser mutations without mock abstractions.\n";
    std::cout << "    These tests do not rule out these attacks for MAGE; it only proves\n";
    std::cout << "    that MAGE's parsers and format is safe from them on paper.\n\n";
    auto pad_field = [](const std::string &field, const std::string &val) -> std::string
    {
        std::ostringstream s;
        s << std::left << std::setw(38) << (field + ":") << val << "\n";
        return s.str();
    };
    int total_cases = static_cast<int>(pk::test::g_results.size());
    int passed_cases = 0;
    double total_runtime_ms = 0.0;
    for (const auto &r : pk::test::g_results)
    {
        if (r.passed)
            passed_cases++;
        total_runtime_ms += r.elapsed_ms;
    }
    std::cout << pad_field("Total test cases in queue", std::to_string(total_cases));
    std::cout << pad_field("Cryptographic algorithm modules", "3 (AES-GCM, XChaCha20, AES-SIV)");
    std::cout << pad_field("Tamper n' negative test vectors", "5");
    std::cout << pad_field("Zip slip n' path traversal vectors", "5 (22 rules verified)");
    std::cout << pad_field("In-process fuzzing mutations", "100,000 (header + path)");
    std::cout << "\n";
    std::vector<std::string> id_strs, target_strs, scope_strs, status_strs, time_strs, res_strs;
    for (const auto &r : pk::test::g_results)
    {
        std::ostringstream sid;
        sid << std::setw(2) << std::setfill('0') << r.item_id;
        id_strs.push_back(sid.str());
        target_strs.push_back(r.target);
        scope_strs.push_back(r.scope);
        status_strs.push_back(r.passed ? "OK" : "FAIL");
        std::ostringstream stime;
        if (r.elapsed_ms < 1.0)
            stime << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms";
        else
            stime << static_cast<int>(std::round(r.elapsed_ms)) << "ms";
        time_strs.push_back(stime.str());
        res_strs.push_back(r.passed ? "PASS" : "FAIL");
    }
    int w_item = pk::test::calculate_col_width("Item", id_strs, 6, 1);
    int w_target = pk::test::calculate_col_width("Module / test target", target_strs, 42, 1);
    int w_scope = pk::test::calculate_col_width("Scope", scope_strs, 14, 1);
    int w_status = pk::test::calculate_col_width("Status", status_strs, 8, 1);
    int w_time = pk::test::calculate_col_width("Time", time_strs, 10, 1);
    int w_res = pk::test::calculate_col_width("Result", res_strs, 8, 1);
    int total_inner_w = w_item + 1 + w_target + 1 + w_scope + 1 + w_status + 1 + w_time + 1 + w_res;
    std::string border_line = "    |" + std::string(total_inner_w, '-') + "|\n";
    std::cout << "Overall test suite summary:\n";
    std::cout << border_line;
    std::cout << "    |" << pk::test::center("Item", w_item)
              << "|" << pk::test::center("Module / Test target", w_target)
              << "|" << pk::test::center("Scope", w_scope)
              << "|" << pk::test::center("Status", w_status)
              << "|" << pk::test::center("Time", w_time)
              << "|" << pk::test::center("Result", w_res)
              << "|\n";
    std::cout << border_line;
    for (std::size_t i = 0; i < pk::test::g_results.size(); ++i)
    {
        std::cout << "    |" << pk::test::center(id_strs[i], w_item)
                  << "|" << pk::test::center(target_strs[i], w_target)
                  << "|" << pk::test::center(scope_strs[i], w_scope)
                  << "|" << pk::test::center(status_strs[i], w_status)
                  << "|" << pk::test::center(time_strs[i], w_time)
                  << "|" << pk::test::center(res_strs[i], w_res)
                  << "|\n";
    }
    std::cout << border_line << "\n";
    double pct = (total_cases > 0) ? (100.0 * passed_cases / total_cases) : 0.0;
    std::cout << "Suite Status:\n";
    std::cout << "     " << std::left << std::setw(33) << "Total tests executed:" << total_cases << "\n";
    std::cout << "     " << std::left << std::setw(33) << "Passed:" << passed_cases << " (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    std::cout << "     " << std::left << std::setw(33) << "Failed:" << (total_cases - passed_cases) << "\n";
    std::cout << "     " << std::left << std::setw(33) << "Total wall clock runtime:" << static_cast<int>(std::round(total_runtime_ms)) << "ms\n";
    if (passed_cases == total_cases)
    {
        std::cout << "     " << std::left << std::setw(33) << "Verdict:" << "OK\n";
    }
    else
    {
        std::cout << "     " << std::left << std::setw(33) << "Verdict:" << "FAIL\n";
        for (const auto &r : pk::test::g_results)
        {
            if (!r.passed)
            {
                std::cout << "     [!] Failure in item " << r.item_id << " (" << r.target << "): " << r.note << "\n";
            }
        }
    }
    return (passed_cases == total_cases) ? 0 : 1;
}

// end