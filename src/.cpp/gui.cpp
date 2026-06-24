// gui.cpp
// last updated: 24/06/2026
// not to be confused, this isn't where all of the main gui elements live at all
#include "../.hpp/gui.hpp"
#include <QDir>
#include <QCoreApplication>
#include "../.hpp/outs.hpp"
#include "../.hpp/shortcuts.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include "../.hpp/stylesheet.hpp"
#include "../.hpp/settings.hpp"
#include "../.hpp/sfx.hpp"
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif
namespace pk::ui
{
    gui::gui(QWidget *parent)
        : QMainWindow(parent)
    {
        setWindowTitle("MAGE - Make actually good encryption!"); // this title is cutoff; oh well
        setFixedSize(300, 300);
        setup_ui();
        dark_theme();
#ifdef _WIN32
        BOOL dark = TRUE;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(this->winId()), 20, &dark, sizeof(dark));
#endif
        bool settings_changed = false;
        auto &s = pk::cfg::settings::instance();
        QString err_msg;
        if (!pk::ui::outs::valid_path_probable(s.def_output_path(), &err_msg))
        {
            s.ss_def_output_path(".");
            pk::ui::outs::info(this, "Settings", "The default output path from your settings was invalid and has been reset to '.'.\nReason: " + err_msg);
            settings_changed = true;
        }
        if (!pk::ui::outs::r_u_a_valid_filename(s.def_ext(), &err_msg))
        {
            s.ss_def_ext(".mage");
            pk::ui::outs::info(this, "Settings", "The default extension from your settings was invalid and has been reset to '.mage'.\nReason: " + err_msg);
            settings_changed = true;
        }
        if (settings_changed)
        {
            s.save();
        }
    }
    void gui::setup_ui()
    {
        QMenuBar *menu_bar = menuBar();
        QMenu *mode_menu = menu_bar->addMenu("Mode");
        QAction *action_create = mode_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/mk_archive.svg")), "Make archive (encryption)");
        QAction *action_decrypt = mode_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/decrypt.svg")), "Decrypt archive");
        QMenu *action_menu = menu_bar->addMenu("Action");
        QAction *action_settings = action_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/settings.svg")), "Settings");
        QAction *action_quit = action_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/quit.svg")), "Quit");
        QMenu *help_menu = menu_bar->addMenu("Help");
        QAction *action_keybinds = help_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/kb.svg")), "Keybinds");
        QAction *action_about = help_menu->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/about.svg")), "About MAGE");
        connect(action_create, &QAction::triggered, this, &gui::on_create_archive_clicked);
        connect(action_decrypt, &QAction::triggered, this, &gui::on_decrypt_archive_clicked);
        connect(action_settings, &QAction::triggered, this, &gui::on_settings_clicked);
        connect(action_quit, &QAction::triggered, qApp, &QApplication::quit);
        connect(action_keybinds, &QAction::triggered, this, &gui::on_keybinds_clicked);
        connect(action_about, &QAction::triggered, this, &gui::on_about_clicked);
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *layout = new QVBoxLayout(central);
        btn_mk = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/mk_archive.svg")), "      Create Archive", this);
        btn_decrypt = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/decrypt.svg")), "      Decrypt Archive", this);
        btn_mk->setFixedSize(200, 45);
        btn_decrypt->setFixedSize(200, 45);
        layout->addStretch();
        layout->addWidget(btn_mk, 0, Qt::AlignCenter);
        layout->addSpacing(10);
        layout->addWidget(btn_decrypt, 0, Qt::AlignCenter);
        layout->addStretch();
        connect(btn_mk, &QPushButton::clicked, this, &gui::on_create_archive_clicked);
        connect(btn_decrypt, &QPushButton::clicked, this, &gui::on_decrypt_archive_clicked);
    }
    void gui::dark_theme()
    {
        qApp->setStyleSheet(pk::ui::style::use_stylesheet());
    }
    void gui::on_create_archive_clicked()
    {
        this->hide();
        pk::ui::outs::cd_mk_archive dialog(nullptr);
        dialog.exec();
        this->show();
    }
    void gui::on_decrypt_archive_clicked()
    {
        this->hide();
        pk::ui::outs::cd_decrypt_archive dialog(nullptr);
        dialog.exec();
        this->show();
    }
    void gui::on_settings_clicked()
    {
        this->hide();
        pk::ui::outs::cd_settings dialog(nullptr);
        dialog.exec();
        this->show();
    }
    void gui::on_keybinds_clicked()
    {
        pk::ui::sfx::play_info();
        pk::ui::shortcuts::cd_keybinds(this);
    }
    void gui::on_about_clicked()
    {
        pk::ui::sfx::play_info();
        pk::ui::outs::cd_about_mage(this);
    }
}

// end