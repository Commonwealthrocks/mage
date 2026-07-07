// sfx.cpp
// last updated: 07/07/2026
#include "../.hpp/sfx.hpp"
#include "../.hpp/settings.hpp"
#include <QDir>
#include <QCoreApplication>
#include <QString>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
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
        return "";
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

        if (!QFileInfo::exists(path))
            return;
#ifdef _WIN32
        if (waveOutGetNumDevs() == 0)
            return;
        DWORD dwVol = (DWORD)((vol / 100.0) * 0xFFFF);
        waveOutSetVolume(NULL, (dwVol & 0xFFFF) | (dwVol << 16));
        QString nativePath = QDir::toNativeSeparators(path);
        PlaySoundW((LPCWSTR)nativePath.utf16(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#else
        if (QStandardPaths::findExecutable("paplay").isEmpty())
            return;
        int linux_vol = (vol * 65536) / 100;
        QStringList args;
        args << "--volume=" + QString::number(linux_vol) << path;
        QProcess::startDetached("paplay", args);
#endif
    }
    void play_success()
    {
#ifdef _WIN32
        play_sound(QDir(get_media_dir()).absoluteFilePath("notify.wav"));
#else
        play_sound("/usr/share/sounds/freedesktop/stereo/complete.oga");
#endif
    }
    void play_error()
    {
#ifdef _WIN32
        play_sound(QDir(get_media_dir()).absoluteFilePath("Windows Critical Stop.wav"));
#else
        play_sound("/usr/share/sounds/freedesktop/stereo/dialog-error.oga");
#endif
    }
    void play_info()
    {
#ifdef _WIN32
        play_sound(QDir(get_media_dir()).absoluteFilePath("Speech On.wav"));
#else
        play_sound("/usr/share/sounds/freedesktop/stereo/dialog-information.oga");
#endif
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