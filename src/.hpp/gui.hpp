// gui.hpp
// last updated: 08/07/2026
#pragma once
#include <QMainWindow>
#include <QPushButton>
namespace pk::ipc
{
    class ipc_server;
}
namespace pk::ui::outs
{
    class cd_mk_archive;
}
namespace pk::ui
{
    class gui : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit gui(pk::ipc::ipc_server *ipc, QWidget *parent = nullptr);
        ~gui() override = default;
        void handle_args(const QString &mode, const QString &path);
    private slots:
        void on_create_archive_clicked();
        void on_decrypt_archive_clicked();
        void on_settings_clicked();
        void on_keybinds_clicked();
        void on_about_clicked();
        // spaces refuse to go here, ok
    private:
        void setup_ui();
        void dark_theme();
        QPushButton *btn_mk;
        QPushButton *btn_decrypt;
        pk::ui::outs::cd_mk_archive *m_mk_archive_dialog = nullptr;
    };
}

// end