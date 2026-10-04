// gui.hpp
// last updated: 04/10/2026
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
    class cd_decrypt_archive;
    class cd_am_i_evil;
}
namespace pk::ui
{
    class gui : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit gui(pk::ipc::ipc_server *ipc, QWidget *parent = nullptr);
        ~gui() override = default;
        void handle_args(const QString &mode, const QString &path, bool quit_on_close = false);
    private slots:
        void on_create_archive_clicked();
        void on_decrypt_archive_clicked();
        void on_verify_archive_clicked();
        void on_settings_clicked();
        void on_keybinds_clicked();
        void on_about_clicked();
        // spaces refuse to go here, ok
    private:
        void setup_ui();
        void dark_theme();
        QPushButton *btn_mk;
        QPushButton *btn_decrypt;
        QPushButton *btn_verify;
        pk::ui::outs::cd_mk_archive *m_mk_archive_dialog = nullptr;
        pk::ui::outs::cd_decrypt_archive *m_decrypt_archive_dialog = nullptr;
        pk::ui::outs::cd_am_i_evil *m_verify_archive_dialog = nullptr;
        QByteArray m_saved_geom;
    };
}

// end