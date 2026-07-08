// outs.hpp
// last updated: 08/07/2026
#pragma once
#include <QString>
#include <QWidget>
#include <QDialog>
#include <QListWidget>
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
#include "../.hpp/gui_worker.hpp"
namespace pk::ui::outs
{
    void info(QWidget *parent, const QString &title, const QString &message);
    void warning(QWidget *parent, const QString &title, const QString &message);
    void error(QWidget *parent, const QString &title, const QString &message);
    bool ask(QWidget *parent, const QString &title, const QString &message);
    void dont_burn_my_eyes(QWidget *window);
    bool r_u_a_valid_filename(const QString &name, QString *err_msg = nullptr);
    bool valid_path_probable(const QString &path, QString *err_msg = nullptr);
    void cd_about_mage(QWidget *parent);
    class progress_dialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit progress_dialog(worker::crypto_worker *worker, QWidget *parent = nullptr);
        ~progress_dialog() override = default;
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
        explicit cd_mk_archive(QWidget *parent = nullptr, const QString &initial_path = "");
        ~cd_mk_archive() override = default;
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
        explicit cd_decrypt_archive(QWidget *parent = nullptr, const QString &initial_path = "");
        ~cd_decrypt_archive() override = default;
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

    private:
        void setup_ui();
        void update_default_path(const QString &path);
        QListWidget *archive_list;
        QLineEdit *output_dir_;
        QLineEdit *password_v;
        QLineEdit *keyfile_path_v;
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
    };
}

// end