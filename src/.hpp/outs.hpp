// outs.hpp
// last updated: 04/10/2026
#pragma once
#include <QString>
#include <QWidget>
#include <QDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QStackedWidget>
#include <QMap>
#include <QPair>
#include <QTabWidget>
#include "../.hpp/gui_worker.hpp"
namespace pk::ui::outs
{
    struct custom_entropy
    {
        bool has_password = false;
        QString password;
        bool has_keyfile = false;
        QString keyfile_path;

        bool has_any() const { return has_password || has_keyfile; }
        QString badge_tag() const
        {
            if (has_password && has_keyfile)
                return "[P] [K]";
            if (has_password)
                return "[P]";
            if (has_keyfile)
                return "[K]";
            return "";
        }
        void wipe();
    };

    class cd_gib_entropy : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_gib_entropy(const QList<QPair<QString, custom_entropy>> &targets, QWidget *parent = nullptr);
        ~cd_gib_entropy() override;
        QList<custom_entropy> get_results() const { return m_results; }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    private slots:
        void on_apply_to_all();
        void on_save();

    private:
        struct archive_tab_ui
        {
            QString file_path;
            QCheckBox *chk_custom_pwd = nullptr;
            QLineEdit *txt_pwd = nullptr;
            QCheckBox *chk_custom_kf = nullptr;
            QLineEdit *txt_kf = nullptr;
            QPushButton *btn_browse_kf = nullptr;
            QPushButton *btn_clear_kf = nullptr;
        };

        QWidget *create_page(const QString &file_path, const custom_entropy &entropy, archive_tab_ui &ui_ref);

        QList<archive_tab_ui> m_tab_uis;
        QTabWidget *m_tabs = nullptr;
        QList<custom_entropy> m_results;
    };
    void info(QWidget *parent, const QString &title, const QString &message);
    void warning(QWidget *parent, const QString &title, const QString &message);
    void error(QWidget *parent, const QString &title, const QString &message);
    bool ask(QWidget *parent, const QString &title, const QString &message);
    void dont_burn_my_eyes(QWidget *window);
    bool r_u_a_valid_filename(const QString &name, QString *err_msg = nullptr);
    bool valid_path_probable(const QString &path, QString *err_msg = nullptr);
    void cd_about_mage(QWidget *parent);
    void toggle_internal_console();
    class cd_prog_dialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_prog_dialog(worker::crypto_worker *worker, QWidget *parent = nullptr);
        ~cd_prog_dialog() override;
        void reject() override;
        QString what_err_msg() const { return current_err; }
    private slots:
        void on_progress(int percentage);
        void on_pr_details(uint64_t processed, uint64_t total);
        void on_success();
        void on_error(const QString &msg);
        void on_update_stats();
        void on_current_ac0(const QString &action);
        void on_current_file(const QString &file);

    private:
        worker::crypto_worker *m2_worker;
        QProgressBar *progress___;
        QLabel *lbl_overall;
        QLabel *lbl_file;
        QLabel *lbl_proc;
        QLabel *lbl_sped;
        QLabel *lbl_elapsed;
        QLabel *lbl_eta;
        QLabel *lbl_file_count;
        QTimer *timer;
        QElapsedTimer elapsed;
        uint64_t last_processed;
        uint64_t current_proc;
        uint64_t tot_bytes;
        int current_pct;
        bool indeterminate;
        bool _success;
        QString current_err;
        QString current_ac0_text;
        QString current_file_text;
    };
    class cd_keyfile : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_keyfile(QWidget *parent = nullptr);
        ~cd_keyfile() override = default;
        QString what_gen_path() const { return gen_path_; }
    private slots:
        void on_generate();
        void on_cancel();
        void on_browse();

    private:
        void setup_ui();
        QLineEdit *path_input;
        QString gen_path_;
    };
    class cd_mk_archive : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_mk_archive(QWidget *parent = nullptr, const QString &ini_path = "");
        ~cd_mk_archive() override;
        void add_path(const QString &path);

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;
    private slots:
        void on_browse_output();
        void on_add_files();
        void on_add_folders();
        void on_rm_selected();
        void on_clear_all();
        void on_create();
        void on_cancel();

    private:
        void setup_ui();
        void update_default_path(const QString &path);
        QLineEdit *output_dir;
        QLineEdit *output_name;
        QListWidget *file_list;
        QComboBox *algo_combo;
        QSpinBox *s_tc;
        QSpinBox *s_mem_cost;
        QSpinBox *s_cores;
        QLineEdit *password_v;
        QLineEdit *keyfile_path_v;
        QCheckBox *_include_hidden;
        QCheckBox *__cp_metadata;
        QComboBox *cmp_algo_combo;
        QComboBox *cmp_preset_combo;
        QCheckBox *cmp_use_raw;
        QSpinBox *_cmp_raw_lvl;
        QSpinBox *chunk_size_used;
    };
    class cd_decrypt_archive : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_decrypt_archive(QWidget *parent = nullptr, const QString &ini_path = "");
        ~cd_decrypt_archive() override;
        void add_path(const QString &path);

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;
    private slots:
        void on_add_files();
        void on_remove_files();
        void on_clear_all();
        void on_browse_output();
        void on_decrypt();
        void on_cancel();
        void on_ls_cm(const QPoint &pos);
        void on_src_ce();
        void on_rm_cp();
        void on_rm_ck();
        void on_rm_all();

    private:
        void setup_ui();
        void update_default_path(const QString &path);
        void refresh_item_display(QListWidgetItem *item, const QString &path);
        QListWidget *archive_list;
        QLineEdit *output_dir_;
        QLineEdit *password_v;
        QLineEdit *keyfile_path_v;
        QComboBox *ext_behavior;
        QCheckBox *ext_open;
        QComboBox *ext_overwrite;
        QMap<QString, custom_entropy> __custom_entropy_;
    };
    class cd_am_i_evil : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_am_i_evil(QWidget *parent = nullptr, const QString &ini_path = "");
        ~cd_am_i_evil() override;
        void add_path(const QString &path);
        void add_paths(const QStringList &paths);

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;

    private slots:
        void on_add_files();
        void on_remove_files();
        void on_clear_all();
        void on_selection_changed();
        void on_copy_path();
        void on_verify_integrity();
        void on_export_log();
        void on_close();
        void on_tree_cm(const QPoint &pos);
        void on_src_ce();
        void on_rm_cp();
        void on_rm_ck();
        void on_rm_all();

    private:
        void setup_ui();
        void view_archive_index(int index);
        void update_qs();
        void refresh_properties();
        void adjust_qc();
        void adjust_pc();
        void refresh_item_display(QTreeWidgetItem *item, const QString &path);

        QTreeWidget *queue_tree;
        QLabel *lbl_queue_summary;
        QLineEdit *txt_current_path;
        QPushButton *btn_copy_path;
        QTreeWidget *pt;
        QLineEdit *password_v;
        QLineEdit *keyfile_path_v;
        std::vector<pk::crypto::am_i_evil::verification_report> m_reports;
        QMap<QString, custom_entropy> __custom_entropy_;
    };
    class cd_settings : public QDialog
    {
        Q_OBJECT
    public:
        explicit cd_settings(QWidget *parent = nullptr);
        ~cd_settings() override = default;
    private slots:
        void on_save();
        void on_cancel();

    private:
        void setup_ui();
        QLineEdit *def_output;
        QLineEdit *def_ext__;
        QCheckBox *mute_sfx_v;
        QSlider *sfx_vol;
        QCheckBox *_include_hidden;
        QCheckBox *__cp_metadata;
        QComboBox *algo_combo;
        QSpinBox *s_tc;
        QSpinBox *s_mem_cost;
        QSpinBox *s_cores;
        QComboBox *cmp_algo_combo;
        QComboBox *cmp_preset_combo;
        QCheckBox *cmp_use_raw;
        QSpinBox *_cmp_raw_lvl;
        QSpinBox *chunk_size_used;
        QComboBox *ext_behavior;
        QCheckBox *ext_open;
        QComboBox *ext_overwrite;
    };
}

// end