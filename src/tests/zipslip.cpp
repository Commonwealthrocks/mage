// zipslip.cpp
// last updated: 31/10/2026
#include "test_common.hpp"
namespace pk::test
{
    void run_zipslip_tests(int &cur_id)
    {
        run_case(cur_id++, "Zip slip: telative traversal (../, ..\\)", "Security", [&]()
                 {
            std::string norm;
            if (pk::path::ok(pk::path::validate_archive_path("../evil.txt", norm)))
                throw std::runtime_error("../ traversal allowed");
            if (pk::path::ok(pk::path::validate_archive_path("foo/../../bar.txt", norm)))
                throw std::runtime_error("nested ../ traversal allowed");
            if (pk::path::ok(pk::path::validate_archive_path("..\\win.exe", norm)))
                throw std::runtime_error("..\\ backslash traversal allowed"); });
        run_case(cur_id++, "Zip slip: Windows drive letters (C:\\)", "Security", [&]()
                 {
            std::string norm;
            if (pk::path::ok(pk::path::validate_archive_path("C:\\Windows\\System32\\calc.exe", norm)))
                throw std::runtime_error("Drive letter C:\\ allowed");
            if (pk::path::ok(pk::path::validate_archive_path("D:/evil.bat", norm)))
                throw std::runtime_error("Drive letter D:/ allowed"); });
        run_case(cur_id++, "Zip slip: UNC network paths (\\\\server)", "Security", [&]()
                 {
            std::string norm;
            if (pk::path::ok(pk::path::validate_archive_path("\\\\attacker_server\\share\\evil.exe", norm)))
                throw std::runtime_error("Windows UNC path allowed");
            if (pk::path::ok(pk::path::validate_archive_path("//server/share/file.txt", norm)))
                throw std::runtime_error("POSIX-style network UNC path allowed"); });
        run_case(cur_id++, "Zip slip: DOS reserved (CON, NUL, COM)", "Security", [&]()
                 {
            std::string norm;
            if (pk::path::ok(pk::path::validate_archive_path("CON", norm)))
                throw std::runtime_error("CON device allowed");
            if (pk::path::ok(pk::path::validate_archive_path("NUL", norm)))
                throw std::runtime_error("NUL device allowed");
            if (pk::path::ok(pk::path::validate_archive_path("sub/COM1", norm)))
                throw std::runtime_error("COM1 device allowed in subdir"); });
        run_case(cur_id++, "Zip slip: NTFS streams (file:stream)", "Security", [&]()
                 {
            std::string norm;
            if (pk::path::ok(pk::path::validate_archive_path("file.txt:hidden_stream", norm)))
                throw std::runtime_error("NTFS alternate data stream colon allowed"); });
    }
}

// end