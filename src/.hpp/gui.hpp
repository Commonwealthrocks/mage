// gui.hpp
// last updated: 17/06/2026
#pragma once
#include <QMainWindow>
#include <QPushButton>
namespace pk::ui
{
    class gui : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit gui(QWidget *parent = nullptr);
        ~gui() override = default;

    private slots:
        void on_create_archive_clicked();
        void on_decrypt_archive_clicked();
        void on_settings_clicked();
        void on_keybinds_clicked();
        void on_about_clicked();

    private:
        void setup_ui();
        void dark_theme();
        QPushButton *btn_mk;
        QPushButton *btn_decrypt;
    };
}

// end