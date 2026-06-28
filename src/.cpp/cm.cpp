// cm.cpp
// last updated: 28/06/2026
#include "../.hpp/cm.hpp"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#ifdef _WIN32
#include <windows.h>
#endif
namespace pk::os::cm
{
    void remove();
#ifdef _WIN32
    static void write_empty_sz(HKEY parent, const wchar_t *subkey, const wchar_t *value_name)
    {
        HKEY hk;
        if (RegCreateKeyExW(parent, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
        {
            RegSetValueExW(hk, value_name, 0, REG_SZ, reinterpret_cast<const BYTE *>(L""), sizeof(wchar_t));
            RegCloseKey(hk);
        }
    }
    static void nuke_key(HKEY root, const wchar_t *subkey)
    {
        RegDeleteTreeW(root, subkey);
        RegDeleteKeyW(root, subkey);
    }
#endif
    void install()
    {
#ifdef _WIN32
        remove();
        QString exe_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        QString icon = "\"" + exe_path + "\"";
        auto setup_cascade = [&](const QString &qsettings_key, const wchar_t *reg_subkey)
        {
            QSettings reg(qsettings_key, QSettings::NativeFormat);
            reg.setValue("MUIVerb", "MAGE...");
            reg.setValue("Icon", icon);
            reg.setValue("shell/1_encrypt/.", "Encrypt with MAGE...");
            reg.setValue("shell/1_encrypt/Icon", icon);
            reg.setValue("shell/1_encrypt/command/.", "\"" + exe_path + "\" --encrypt \"%1\"");
            reg.setValue("shell/2_decrypt/.", "Decrypt with MAGE...");
            reg.setValue("shell/2_decrypt/Icon", icon);
            reg.setValue("shell/2_decrypt/command/.", "\"" + exe_path + "\" --decrypt \"%1\"");
            reg.sync();
            write_empty_sz(HKEY_CURRENT_USER, reg_subkey, L"SubCommands");
        };
        setup_cascade("HKEY_CURRENT_USER\\Software\\Classes\\*\\shell\\MAGE", L"Software\\Classes\\*\\shell\\MAGE");
        setup_cascade("HKEY_CURRENT_USER\\Software\\Classes\\Directory\\shell\\MAGE", L"Software\\Classes\\Directory\\shell\\MAGE");
        QSettings reg_ext("HKEY_CURRENT_USER\\Software\\Classes\\.mage", QSettings::NativeFormat);
        reg_ext.setValue(".", "MAGE.Archive");
        QSettings reg_prog("HKEY_CURRENT_USER\\Software\\Classes\\MAGE.Archive", QSettings::NativeFormat);
        reg_prog.setValue(".", "MAGE Encrypted Archive");
        reg_prog.setValue("DefaultIcon/.", "\"" + exe_path + "\",0");
        reg_prog.setValue("shell/open/.", "Decrypt with MAGE");
        reg_prog.setValue("shell/open/Icon", icon);
        reg_prog.setValue("shell/open/command/.", "\"" + exe_path + "\" --decrypt \"%1\"");
        setup_cascade("HKEY_CURRENT_USER\\Software\\Classes\\MAGE.Archive\\shell\\MAGE", L"Software\\Classes\\MAGE.Archive\\shell\\MAGE");
#endif
    }
    void remove()
    {
#ifdef _WIN32
        nuke_key(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\MAGE");
        nuke_key(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\MAGE");
        nuke_key(HKEY_CURRENT_USER, L"Software\\Classes\\.mage");
        nuke_key(HKEY_CURRENT_USER, L"Software\\Classes\\MAGE.Archive");
#endif
    }
}

// end