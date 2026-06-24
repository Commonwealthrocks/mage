// sfx.cpp
// last updated: 24/06/2026
#include "../.hpp/sfx.hpp"
#include "../.hpp/settings.hpp"
#include <QDir>
#include <QCoreApplication>
#include <QString>
#include <string>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
namespace pk::ui::sfx
{
    static QString get_media_dir()
    {
#ifdef _WIN32
        char windir[MAX_PATH];
        if (GetWindowsDirectoryA(windir, MAX_PATH) == 0)
            return "C:\\Windows\\Media";
        return QDir(QString::fromLocal8Bit(windir)).absoluteFilePath("Media");
#else
        return "C:\\Windows\\Media";
#endif
    }

    void preload()
    {
        // not needed, for the native api
    }
    void play_sound(const QString &path)
    {
        if (pk::cfg::settings::instance().mute_sfx())
            return;
        int vol = pk::cfg::settings::instance().sfx_volume();
        if (vol <= 0)
            return;
#ifdef _WIN32
        DWORD dwVol = (DWORD)((vol / 100.0) * 0xFFFF);
        waveOutSetVolume(NULL, (dwVol & 0xFFFF) | (dwVol << 16));
        QString nativePath = QDir::toNativeSeparators(path);
        PlaySoundW((LPCWSTR)nativePath.utf16(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#endif
    }
    void play_success()
    {
        play_sound(QDir(get_media_dir()).absoluteFilePath("notify.wav"));
    }
    void play_error()
    {
        play_sound(QDir(get_media_dir()).absoluteFilePath("Windows Critical Stop.wav"));
    }
    void play_info()
    {
        play_sound(QDir(get_media_dir()).absoluteFilePath("Speech On.wav"));
    }
    void play_by_name(const std::string &name)
    {
        for (char c : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-')
            {
                return; // reject invalid characters
            }
        }
        if (name.find("..") != std::string::npos)
        {
            return;
        }
        QString qname = QString::fromStdString(name);
        QString path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/sfx/" + qname);
        play_sound(path);
    }
}

// end