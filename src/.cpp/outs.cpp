// outs.cpp
// last updated: 24/06/2026
#include "../.hpp/outs.hpp"
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QElapsedTimer>
#include <QShortcut>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include "../.hpp/keyfile.hpp"
#include <QStackedWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QGroupBox>
#include <QApplication>
#include <QTextEdit>
#include <QGridLayout>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QPixmap>
#include <QListView>
#include <QTreeView>
#include <QFileInfo>
#include <QIcon>
#include <QRegularExpression>
#include "../.hpp/error_msg.hpp"
#include "../.hpp/aes_ni.hpp"
#include "../.hpp/path_handler.hpp"
#include "../.hpp/settings.hpp"
#include "../.hpp/sfx.hpp"
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif
#include <QMap>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
namespace pk::ui::outs
{
    static QMap<QString, QString> parse_tooltips(const QString &file_path)
    {
        QMap<QString, QString> tooltips;
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return tooltips;
        QTextStream in(&file);
        QString current_key;
        QString current_text;
        bool in_value = false;
        while (!in.atEnd())
        {
            QString line = in.readLine();
            if (!in_value)
            {
                line = line.trimmed();
                if (line.isEmpty() || line.startsWith("##"))
                    continue;
                if (line.endsWith("=\"\"\""))
                {
                    current_key = line.left(line.length() - 4).trimmed();
                    in_value = true;
                    current_text.clear();
                }
            }
            else
            {
                if (line.trimmed() == "\"\"\"")
                {
                    tooltips[current_key] = current_text.trimmed();
                    in_value = false;
                }
                else
                {
                    current_text += line + "\n";
                }
            }
        }
        return tooltips;
    }
    bool r_u_a_valid_filename(const QString &name, QString *err_msg)
    {
        std::string out_norm;
        auto err = pk::path::validate_archive_path(name.toStdString(), out_norm);
        if (err != pk::path::validation_error::none)
        {
            if (err_msg)
                *err_msg = QString("Filename validation failed; ") + QString::fromStdString(pk::path::describe(err));
            return false;
        }
        return true;
    }
    bool valid_path_probable(const QString &path, QString *err_msg)
    {
        std::string p = path.toUtf8().toStdString();
        auto err = pk::path::validate_host_path(p);
        if (err != pk::path::validation_error::none)
        {
            if (err_msg)
                *err_msg = QString("Path validation failed; ") + QString::fromStdString(pk::path::describe(err));
            return false;
        }
        return true;
    }
    void dont_burn_my_eyes(QWidget *window)
    {
#ifdef _WIN32
        if (window)
        {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), 20, &dark, sizeof(dark));
        }
#endif
    }
    class custom_msg_dialog : public QDialog
    {
    public:
        custom_msg_dialog(const QString &title, const QString &message, bool is_ask = false, QWidget *parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle(title);
            setFixedSize(400, 200);
            setWindowFlags(windowFlags() | Qt::Window);
            setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
            dont_burn_my_eyes(this);
            QVBoxLayout *layout = new QVBoxLayout(this);
            QLabel *lbl_title = new QLabel(title, this);
            lbl_title->setStyleSheet("font-weight: bold; font-size: 12pt;");
            layout->addWidget(lbl_title);
            QTextEdit *text_edit = new QTextEdit(this);
            text_edit->setPlainText(message);
            text_edit->setReadOnly(true);
            text_edit->setStyleSheet("QTextEdit { background-color: #3C3C3C; color: #E0E0E0; border: 1px solid #5A5A5A; }");
            layout->addWidget(text_edit);
            QHBoxLayout *btn_layout = new QHBoxLayout();
            btn_layout->addStretch();
            if (is_ask)
            {
                QPushButton *btn_yes = new QPushButton("Yes", this);
                QPushButton *btn_no = new QPushButton("No", this);
                connect(btn_yes, &QPushButton::clicked, this, &QDialog::accept);
                connect(btn_no, &QPushButton::clicked, this, &QDialog::reject);
                btn_layout->addWidget(btn_yes);
                btn_layout->addWidget(btn_no);
            }
            else
            {
                QPushButton *btn_ok = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " OK", this);
                connect(btn_ok, &QPushButton::clicked, this, &QDialog::accept);
                btn_layout->addWidget(btn_ok);
            }
            layout->addLayout(btn_layout);
        }
    };
    void info(QWidget *parent, const QString &title, const QString &message)
    {
        pk::ui::sfx::play_info();
        custom_msg_dialog dlg(title, message, false, parent);
        dlg.exec();
    }
    void warning(QWidget *parent, const QString &title, const QString &message)
    {
        pk::ui::sfx::play_error();
        custom_msg_dialog dlg(title, message, false, parent);
        dlg.exec();
    }
    void error(QWidget *parent, const QString &title, const QString &message)
    {
        pk::ui::sfx::play_error();
        custom_msg_dialog dlg(title, message, false, parent);
        dlg.exec();
    }
    bool ask(QWidget *parent, const QString &title, const QString &message)
    {
        custom_msg_dialog dlg(title, message, true, parent);
        return dlg.exec() == QDialog::Accepted;
    }
    progress_dialog::progress_dialog(worker::crypto_worker *worker, QWidget *parent) : QDialog(parent), m2_worker(worker), _success(false), last_processed(0), current_proc(0), tot_bytes(0)
    {
        setWindowTitle(m2_worker->what_mode() == worker::crypto_worker::mode::pack ? "Creating archive..." : "Decrypting archive...");
        setFixedSize(550, 285);
        setWindowFlags(windowFlags() | Qt::Window);
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
        setModal(true);
        dont_burn_my_eyes(this);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSpacing(12);
        QGroupBox *overall_group = new QGroupBox("Overall progress", this);
        QVBoxLayout *overall_layout = new QVBoxLayout(overall_group);
        lbl_overall = new QLabel("Files: processing...", this);
        lbl_overall->setStyleSheet("font-weight: bold; font-size: 10pt;");
        overall_layout->addWidget(lbl_overall);
        progress___ = new QProgressBar(this);
        progress___->setRange(0, 100);
        progress___->setValue(0);
        overall_layout->addWidget(progress___);
        layout->addWidget(overall_group);
        QGroupBox *stats_group = new QGroupBox("Stats", this);
        QGridLayout *stats_layout = new QGridLayout(stats_group);
        lbl_proc = new QLabel("Size: 0 B / 0 B", this);
        lbl_sped = new QLabel("Speed: 0 MB/s", this);
        lbl_elapsed = new QLabel("Elapsed: 00:00:00", this);
        stats_layout->addWidget(new QLabel("Processed:", this), 0, 0);
        stats_layout->addWidget(lbl_proc, 0, 1);
        stats_layout->addWidget(new QLabel("Speed:", this), 1, 0);
        stats_layout->addWidget(lbl_sped, 1, 1);
        stats_layout->addWidget(new QLabel("Time elapsed:", this), 2, 0);
        stats_layout->addWidget(lbl_elapsed, 2, 1);
        layout->addWidget(stats_group);
        QHBoxLayout *btn_layout = new QHBoxLayout();
        btn_layout->addStretch();
        QPushButton *btn_cancel = new QPushButton("Cancel", this);
        connect(btn_cancel, &QPushButton::clicked, this, [this]()
                { reject(); });
        btn_layout->addWidget(btn_cancel);
        layout->addLayout(btn_layout);
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &progress_dialog::on_update_stats);
        timer->start(1000);
        elapsed.start();
        connect(m2_worker, &worker::crypto_worker::success, this, &progress_dialog::on_success);
        connect(m2_worker, &worker::crypto_worker::error, this, &progress_dialog::on_error);
        connect(m2_worker, &worker::crypto_worker::progress, this, &progress_dialog::on_progress);
        connect(m2_worker, &worker::crypto_worker::progress_details, this, &progress_dialog::on_progress_details);
        connect(m2_worker, &QThread::finished, m2_worker, &QObject::deleteLater);
        m2_worker->start();
    }
    void progress_dialog::on_progress(int percentage)
    {
        progress___->setValue(percentage);
    }
    void progress_dialog::on_progress_details(uint64_t processed, uint64_t total)
    {
        current_proc = processed;
        tot_bytes = total;
    }
    void progress_dialog::on_update_stats()
    {
        uint64_t bytes_this_sec = current_proc - last_processed;
        last_processed = current_proc;
        double speed_mb_s = (double)bytes_this_sec / (1024.0 * 1024.0);
        lbl_sped->setText(QString("Speed: %1 MB/s").arg(speed_mb_s, 0, 'f', 2));
        auto format_size = [](uint64_t s) -> QString
        {
            if (s > 1024 * 1024 * 1024)
                return QString("%1 GBs").arg((double)s / (1024 * 1024 * 1024), 0, 'f', 2);
            if (s > 1024 * 1024)
                return QString("%1 MBs").arg((double)s / (1024 * 1024), 0, 'f', 2);
            if (s > 1024)
                return QString("%1 KBs").arg((double)s / 1024, 0, 'f', 2);
            return QString("%1 Bs").arg(s);
        };
        lbl_proc->setText(QString("Size: %1 / %2").arg(format_size(current_proc)).arg(format_size(tot_bytes)));

        qint64 secs = elapsed.elapsed() / 1000;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        lbl_elapsed->setText(QString("Elapsed: %1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
    }
    void progress_dialog::on_success()
    {
        _success = true;
        pk::ui::sfx::play_success();
        accept();
    }
    void progress_dialog::on_error(const QString &msg)
    {
        _success = false;
        current_err = msg;
        reject();
    }
    cd_keyfile::cd_keyfile(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Generate keyfile");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(400, 180);
        setup_ui();
        dont_burn_my_eyes(this);
    }
    void cd_keyfile::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QLabel *warning_lbl = new QLabel("<b>[ WARNING ]</b> Losing this keyfile = losing your data. I won't clarify that any further.");
        warning_lbl->setWordWrap(true);
        warning_lbl->setStyleSheet("color: #ff5555; margin-bottom: 10px;");
        main_layout->addWidget(warning_lbl);
        QFormLayout *form = new QFormLayout();
        QHBoxLayout *path_layout = new QHBoxLayout();
        path_input = new QLineEdit(this);
        path_input->setContextMenuPolicy(Qt::NoContextMenu);
        QPushButton *btn_browse = new QPushButton("Browse...", this);
        path_layout->addWidget(path_input);
        path_layout->addWidget(btn_browse);
        form->addRow("Output path:", path_layout);
        main_layout->addLayout(form);
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_gen = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/mk_archive.svg")), " Generate", this);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Cancel", this);
        action_layout->addWidget(btn_gen);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        connect(btn_browse, &QPushButton::clicked, this, &cd_keyfile::on_browse);
        connect(btn_gen, &QPushButton::clicked, this, &cd_keyfile::on_generate);
        connect(btn_cancel, &QPushButton::clicked, this, &cd_keyfile::on_cancel);
    }
    void cd_keyfile::on_browse()
    {
        QString path = QFileDialog::getSaveFileName(this, "Save keyfile", "keyfile.mgkx", "MAGE keyfiles (*.mgkx)");
        if (!path.isEmpty())
            path_input->setText(path);
    }
    void cd_keyfile::on_generate()
    {
        if (path_input->text().isEmpty())
        {
            warning(this, "ERROR", "Path cannot be empty.");
            return;
        }
        try
        {
            pk::crypto::keyfile::generate(path_input->text().toStdString());
            gen_path_ = path_input->text();
            info(this, "Success", "Keyfile successfully generated.");
            accept();
        }
        catch (const std::exception &e)
        {
            error(this, "Error", QString("Failed to generate keyfile: ") + e.what());
        }
    }
    void cd_keyfile::on_cancel()
    {
        reject();
    }
    cd_mk_archive::cd_mk_archive(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Create archive");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(500, 600);
        setAcceptDrops(true);
        setup_ui();
        dont_burn_my_eyes(this);
    }
    void cd_mk_archive::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QGroupBox *group_out = new QGroupBox("Archive output", this);
        QFormLayout *out_layout = new QFormLayout(group_out);
        QHBoxLayout *dir_layout = new QHBoxLayout();
        output_dir = new QLineEdit(this);
        output_dir->setText(pk::cfg::settings::instance().def_output_path());
        output_dir->setContextMenuPolicy(Qt::NoContextMenu);
        QPushButton *btn_browse_out = new QPushButton("Browse", this);
        dir_layout->addWidget(output_dir);
        dir_layout->addWidget(btn_browse_out);
        output_name = new QLineEdit(this);
        QString ext = pk::cfg::settings::instance().def_ext();
        if (!ext.startsWith("."))
            ext = "." + ext;
        output_name->setText("archive" + ext);
        output_name->setContextMenuPolicy(Qt::NoContextMenu);
        out_layout->addRow("Output directory:", dir_layout);
        out_layout->addRow("Archive name:", output_name);
        main_layout->addWidget(group_out);
        QGroupBox *group_files = new QGroupBox("Files to archive (drag n' drop here)", this);
        QVBoxLayout *files_layout = new QVBoxLayout(group_files);
        file_list = new QListWidget(this);
        file_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        files_layout->addWidget(file_list);
        QHBoxLayout *file_btns = new QHBoxLayout();
        QPushButton *btn_add_file = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add file(s)", this);
        QPushButton *btn_add_dir = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add folder(s)", this);
        QPushButton *btn_rem_sel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), " Remove selected", this);
        QPushButton *btn_clear = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm_all.svg")), " Clear all", this);
        file_btns->addWidget(btn_add_file);
        file_btns->addWidget(btn_add_dir);
        file_btns->addWidget(btn_rem_sel);
        file_btns->addWidget(btn_clear);
        files_layout->addLayout(file_btns);
        main_layout->addWidget(group_files);
        QTabWidget *tabs = new QTabWidget(this);
        QWidget *tab_enc = new QWidget();
        QFormLayout *form_enc = new QFormLayout(tab_enc);
        algo_combo = new QComboBox(this);
        algo_combo->addItem("AES-256-GCM");
        algo_combo->addItem("XChaCha20-Poly1305");
        algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cipher());
        form_enc->addRow("Cipher:", algo_combo);
        QLabel *aes_ni_label = new QLabel(this);
        if (pk::crypto::aes_ni_there())
        {
            aes_ni_label->setText("AES-NI: yeah");
            aes_ni_label->setStyleSheet("QLabel { color: #a0dca0; font-size: 11px; }");
            aes_ni_label->setToolTip("Your CPU supports AES-NI, making AES-256-GCM extremely fast.");
        }
        else
        {
            aes_ni_label->setText("AES-NI: nah");
            aes_ni_label->setStyleSheet("QLabel { color: #e0d060; font-size: 11px; }");
            aes_ni_label->setToolTip("Your CPU does not support AES-NI. XChaCha20-Poly1305 is recommended over AES-256-GCM for better performance and security on this system.");
        }
        form_enc->addRow("", aes_ni_label);
        tabs->addTab(tab_enc, "Encryption");
        QWidget *tab_kdf = new QWidget();
        QFormLayout *form_kdf = new QFormLayout(tab_kdf);
        s_tc = new QSpinBox(this);
        s_tc->setRange(1, 100);
        s_tc->setValue(pk::cfg::settings::instance().def_time_cost());
        s_mem_cost = new QSpinBox(this);
        s_mem_cost->setRange(1, 4096);
        s_mem_cost->setValue(pk::cfg::settings::instance().def_mem_cost() / 1024);
        s_mem_cost->setSuffix(" MBs");
        s_cores = new QSpinBox(this);
        s_cores->setRange(1, 64);
        s_cores->setValue(pk::cfg::settings::instance().def_cores());
        s_tc->setContextMenuPolicy(Qt::NoContextMenu);
        s_mem_cost->setContextMenuPolicy(Qt::NoContextMenu);
        s_cores->setContextMenuPolicy(Qt::NoContextMenu);
        form_kdf->addRow("Argon2ID time cost:", s_tc);
        form_kdf->addRow("Argon2ID mem_:", s_mem_cost);
        form_kdf->addRow("Argon2ID parallelism:", s_cores);
        tabs->addTab(tab_kdf, "KDF");
        QWidget *tab_gen = new QWidget();
        QFormLayout *form_gen = new QFormLayout(tab_gen);
        _include_hidden = new QCheckBox("Include hidden files / directories", this);
        _include_hidden->setChecked(pk::cfg::settings::instance().include_hidden());
        __cp_metadata = new QCheckBox("Copy properties (creation / modified dates, permissions)", this);
        __cp_metadata->setChecked(pk::cfg::settings::instance().cp_metadata());
        chunk_size_used = new QSpinBox(this);
        chunk_size_used->setRange(1, 64);
        chunk_size_used->setSuffix(" MBs");
        chunk_size_used->setValue(pk::cfg::settings::instance().def_chunk_size());
        chunk_size_used->setContextMenuPolicy(Qt::NoContextMenu);
        form_gen->addRow(_include_hidden);
        form_gen->addRow(__cp_metadata);
        form_gen->addRow("Chunk size:", chunk_size_used);
        tabs->addTab(tab_gen, "General options");
        QWidget *tab_comp = new QWidget();
        QFormLayout *form_comp = new QFormLayout(tab_comp);
        cmp_algo_combo = new QComboBox(this);
        cmp_algo_combo->addItem("None");
        cmp_algo_combo->addItem("ZSTD");
        cmp_algo_combo->addItem("LZMA2");
        cmp_algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_algorithm());
        form_comp->addRow("Algorithm:", cmp_algo_combo);
        cmp_preset_combo = new QComboBox(this);
        cmp_preset_combo->addItem("None");
        cmp_preset_combo->addItem("Normal");
        cmp_preset_combo->addItem("Good");
        cmp_preset_combo->addItem("ULTRAKILL");
        cmp_preset_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_preset());
        form_comp->addRow("Preset:", cmp_preset_combo);
        cmp_use_raw = new QCheckBox("Use raw levels instead", this);
        cmp_use_raw->setChecked(pk::cfg::settings::instance().ss_raw_cmp());
        form_comp->addRow(cmp_use_raw);
        _cmp_raw_lvl = new QSpinBox(this);
        _cmp_raw_lvl->setRange(0, 22);
        _cmp_raw_lvl->setValue(pk::cfg::settings::instance().def_cmp_lvl());
        _cmp_raw_lvl->setContextMenuPolicy(Qt::NoContextMenu);
        form_comp->addRow("Raw level:", _cmp_raw_lvl);
        auto update_comp_ui = [this, form_comp]()
        {
            int algo = cmp_algo_combo->currentIndex();
            bool use_raw = cmp_use_raw->isChecked();
            auto set_enabled = [&](QWidget *w, bool enabled)
            {
                w->setEnabled(enabled);
                if (QWidget *label = form_comp->labelForField(w))
                {
                    label->setEnabled(enabled);
                }
            };
            if (algo == 0)
            {
                set_enabled(cmp_preset_combo, false);
                set_enabled(cmp_use_raw, false);
                set_enabled(_cmp_raw_lvl, false);
            }
            else
            {
                set_enabled(cmp_use_raw, true);
                if (use_raw)
                {
                    set_enabled(cmp_preset_combo, false);
                    set_enabled(_cmp_raw_lvl, true);
                    if (algo == 1)
                        _cmp_raw_lvl->setRange(0, 22);
                    else if (algo == 2)
                        _cmp_raw_lvl->setRange(0, 9);
                }
                else
                {
                    set_enabled(cmp_preset_combo, true);
                    set_enabled(_cmp_raw_lvl, false);
                }
            }
        };
        connect(cmp_algo_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update_comp_ui);
        connect(cmp_use_raw, &QCheckBox::checkStateChanged, this, update_comp_ui);
        update_comp_ui();
        tabs->addTab(tab_comp, "Compression");
        main_layout->addWidget(tabs);
        QGroupBox *group_pass = new QGroupBox("Archive password (CTRL + SHIFT + K to toggle)", this);
        QVBoxLayout *pass_layout = new QVBoxLayout(group_pass);
        password_v = new QLineEdit(this);
        password_v->setEchoMode(QLineEdit::Password);
        password_v->setPlaceholderText("Enter password...");
        password_v->setContextMenuPolicy(Qt::NoContextMenu);
        QAction *warn_action = password_v->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/warn.svg")), QLineEdit::TrailingPosition);
        warn_action->setToolTip("CAPS lock is enabled, if you weren't aware.");
        warn_action->setVisible(false);
        QTimer *caps_timer1 = new QTimer(password_v);
        connect(caps_timer1, &QTimer::timeout, password_v, [warn_action]()
                {
#ifdef _WIN32
                    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    warn_action->setVisible(caps);
#endif
                });
        caps_timer1->start(100);
        QAction *toggle_action = password_v->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")), QLineEdit::TrailingPosition);
        connect(toggle_action, &QAction::triggered, this, [this, toggle_action]()
                {
            if (password_v->echoMode() == QLineEdit::Password) {
                password_v->setEchoMode(QLineEdit::Normal);
                toggle_action->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/hide_password.svg")));
            } else {
                password_v->setEchoMode(QLineEdit::Password);
                toggle_action->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")));
            } });
        input_stack = new QStackedWidget(this);
        input_stack->addWidget(password_v);
        QWidget *keyfile_page = new QWidget();
        QHBoxLayout *keyfile_layout = new QHBoxLayout(keyfile_page);
        keyfile_layout->setContentsMargins(0, 0, 0, 0);
        keyfile_path_v = new QLineEdit(this);
        keyfile_path_v->setPlaceholderText("Select a keyfile...");
        keyfile_path_v->setReadOnly(true);
        QPushButton *btn_browse_keyfile = new QPushButton("Browse", this);
        QPushButton *btn_generate_keyfile = new QPushButton("Generate keyfile", this);
        keyfile_layout->addWidget(keyfile_path_v);
        keyfile_layout->addWidget(btn_browse_keyfile);
        keyfile_layout->addWidget(btn_generate_keyfile);
        input_stack->addWidget(keyfile_page);
        pass_layout->addWidget(input_stack);
        main_layout->addWidget(group_pass);
        QShortcut *shortcut_keyfile = new QShortcut(QKeySequence("Ctrl+Shift+K"), this);
        connect(shortcut_keyfile, &QShortcut::activated, this, [this, group_pass]()
                {
            m_use_keyfile_mode = !m_use_keyfile_mode;
            if (m_use_keyfile_mode) {
                input_stack->setCurrentIndex(1);
                group_pass->setTitle("Archive keyfile (CTRL + SHIFT + K to toggle)");
            } else {
                input_stack->setCurrentIndex(0);
                group_pass->setTitle("Archive password (CTRL + SHIFT + K to toggle)");
            } });
        connect(btn_browse_keyfile, &QPushButton::clicked, this, [this]()
                {
            QString path = QFileDialog::getOpenFileName(this, "Select keyfile", "", "MAGE keyfiles (*.mgkx);;All files (*)");
            if (!path.isEmpty()) keyfile_path_v->setText(path); });
        connect(btn_generate_keyfile, &QPushButton::clicked, this, [this]()
                {
            cd_keyfile dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                keyfile_path_v->setText(dlg.what_gen_path());
            } });
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_create = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/mk_archive.svg")), " Create archive", this);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Cancel", this);
        action_layout->addWidget(btn_create);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        // for some reason i can't put the connectors in main so this'll do
        connect(btn_browse_out, &QPushButton::clicked, this, &cd_mk_archive::on_browse_output);
        connect(btn_add_file, &QPushButton::clicked, this, &cd_mk_archive::on_add_files);
        connect(btn_add_dir, &QPushButton::clicked, this, &cd_mk_archive::on_add_folders);
        connect(btn_rem_sel, &QPushButton::clicked, this, &cd_mk_archive::on_rm_selected);
        connect(btn_clear, &QPushButton::clicked, this, &cd_mk_archive::on_clear_all);
        connect(btn_create, &QPushButton::clicked, this, &cd_mk_archive::on_create);
        connect(btn_cancel, &QPushButton::clicked, this, &cd_mk_archive::on_cancel);
    }
    void cd_mk_archive::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    void cd_mk_archive::dropEvent(QDropEvent *event)
    {
        for (const QUrl &url : event->mimeData()->urls())
        {
            QString path = url.toLocalFile();
            if (!path.isEmpty() && file_list->findItems(path, Qt::MatchExactly).isEmpty())
            {
                file_list->addItem(path);
                update_default_path(path);
            }
        }
    }
    void cd_mk_archive::on_browse_output()
    {
        QString path = QFileDialog::getExistingDirectory(this, "Select output directory");
        if (!path.isEmpty())
            output_dir->setText(path);
    }
    void cd_mk_archive::on_add_files()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select files");
        for (const QString &f : files)
        {
            if (file_list->findItems(f, Qt::MatchExactly).isEmpty())
            {
                file_list->addItem(f);
                update_default_path(f);
            }
        }
    }
    void cd_mk_archive::on_add_folders()
    {
        QFileDialog dialog(this, "Select folder(s)");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        QListView *l = dialog.findChild<QListView *>("listView");
        if (l)
            l->setSelectionMode(QAbstractItemView::ExtendedSelection);
        QTreeView *t = dialog.findChild<QTreeView *>();
        if (t)
            t->setSelectionMode(QAbstractItemView::ExtendedSelection);

        if (dialog.exec() == QDialog::Accepted)
        {
            QStringList dirs = dialog.selectedFiles();
            for (const QString &d : dirs)
            {
                if (!d.isEmpty() && file_list->findItems(d, Qt::MatchExactly).isEmpty())
                {
                    file_list->addItem(d);
                    update_default_path(d);
                }
            }
        }
    }
    void cd_mk_archive::update_default_path(const QString &path)
    {
        if (output_dir->text() == ".")
        {
            QFileInfo fi(path);
            output_dir->setText(fi.absolutePath());
        }
    }
    void cd_mk_archive::on_rm_selected()
    {
        qDeleteAll(file_list->selectedItems());
    }
    void cd_mk_archive::on_clear_all()
    {
        file_list->clear();
    }
    void cd_mk_archive::on_create()
    {
        if (output_dir->text().isEmpty() || output_name->text().isEmpty())
        {
            warning(this, "ERROR", "Archive output directory or name is empty.");
            return;
        }
        QString resolved_out = output_dir->text();
        if (resolved_out == "." && file_list->count() > 0)
        {
            QFileInfo fi(file_list->item(0)->text());
            resolved_out = fi.absolutePath();
            output_dir->setText(resolved_out);
        }

        QString err_msg;
        if (!pk::ui::outs::valid_path_probable(resolved_out, &err_msg))
        {
            warning(this, "ERROR", err_msg);
            return;
        }
        if (!QFileInfo::exists(resolved_out) || !QFileInfo(resolved_out).isDir())
        {
            warning(this, "ERROR", "Output directory does not exist -> " + resolved_out);
            return;
        }
        if (!pk::ui::outs::r_u_a_valid_filename(output_name->text(), &err_msg))
        {
            warning(this, "ERROR", err_msg);
            return;
        }
        if (file_list->count() == 0)
        {
            warning(this, "ERROR", "No files selected.");
            return;
        }
        if (!m_use_keyfile_mode && password_v->text().isEmpty())
        {
            warning(this, "ERROR", "Password is empty.");
            return;
        }
        if (m_use_keyfile_mode && keyfile_path_v->text().isEmpty())
        {
            warning(this, "ERROR", "Keyfile is not selected.");
            return;
        }
        pk::crypto::kdf::kdf_cfg kdf_cfg{};
        kdf_cfg.time_cost = s_tc->value();
        kdf_cfg.memory_cost_kb = s_mem_cost->value() * 1024;
        kdf_cfg.parallelism = s_cores->value();
        kdf_cfg.hash_length = 32;
        pk::crypto::cipher::algorithm algo = (algo_combo->currentIndex() == 0) ? pk::crypto::cipher::algorithm::aes_256_gcm : pk::crypto::cipher::algorithm::xchacha20_poly1305;
        worker::crypto_worker *worker = new worker::crypto_worker(worker::crypto_worker::mode::pack);
        std::vector<std::string> roots;
        for (int i = 0; i < file_list->count(); ++i)
        {
            roots.push_back(file_list->item(i)->text().toUtf8().toStdString());
        }
        uint8_t comp_algo = cmp_algo_combo->currentIndex();
        int8_t comp_level = 0;
        if (comp_algo != 0)
        {
            if (cmp_use_raw->isChecked())
            {
                comp_level = _cmp_raw_lvl->value();
            }
            else
            {
                int preset = cmp_preset_combo->currentIndex();
                if (comp_algo == 1)
                {
                    if (preset == 1)
                        comp_level = 3;
                    else if (preset == 2)
                        comp_level = 9;
                    else if (preset == 3)
                        comp_level = 19;
                }
                else if (comp_algo == 2)
                {
                    if (preset == 1)
                        comp_level = 3;
                    else if (preset == 2)
                        comp_level = 6;
                    else if (preset == 3)
                        comp_level = 9;
                }
            }
        }
        worker->ss_def_pk_params(
            roots,
            output_dir->text() + "/" + output_name->text(),
            password_v->text(),
            m_use_keyfile_mode,
            keyfile_path_v->text(),
            algo,
            kdf_cfg,
            _include_hidden->isChecked(),
            __cp_metadata->isChecked(),
            comp_algo,
            comp_level,
            chunk_size_used->value());
        progress_dialog pd(worker, this);
        if (pd.exec() == QDialog::Accepted)
        {
            info(this, "Success", "Archive successfully created.");
            accept();
        }
        else
        {
            if (!pd.what_err_msg().isEmpty())
            {
                error(this, "Something went wrong!!!", pd.what_err_msg());
            }
        }
    }
    void cd_mk_archive::on_cancel()
    {
        reject();
    }
    cd_decrypt_archive::cd_decrypt_archive(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Decrypt archive");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(400, 200);
        setAcceptDrops(true);
        setup_ui();
        dont_burn_my_eyes(this);
    }
    void cd_decrypt_archive::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QFormLayout *form = new QFormLayout();
        QHBoxLayout *arch_layout = new QHBoxLayout();
        _output_path = new QLineEdit(this);
        _output_path->setContextMenuPolicy(Qt::NoContextMenu);
        QPushButton *btn_browse_arch = new QPushButton("Browse", this);
        arch_layout->addWidget(_output_path);
        arch_layout->addWidget(btn_browse_arch);
        form->addRow("Archive file:", arch_layout);
        QHBoxLayout *out_layout = new QHBoxLayout();
        output_dir_ = new QLineEdit(this);
        output_dir_->setText(pk::cfg::settings::instance().def_output_path());
        output_dir_->setContextMenuPolicy(Qt::NoContextMenu);
        QPushButton *btn_browse_out = new QPushButton("Browse", this);
        out_layout->addWidget(output_dir_);
        out_layout->addWidget(btn_browse_out);
        form->addRow("Extract to:", out_layout);
        password_v = new QLineEdit(this);
        password_v->setEchoMode(QLineEdit::Password);
        password_v->setContextMenuPolicy(Qt::NoContextMenu);
        QAction *warn_action2 = password_v->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/warn.svg")), QLineEdit::TrailingPosition);
        warn_action2->setToolTip("CAPS lock is enabled, if you weren't aware.");
        warn_action2->setVisible(false);
        QTimer *caps_timer2 = new QTimer(password_v);
        connect(caps_timer2, &QTimer::timeout, password_v, [warn_action2]()
                {
#ifdef _WIN32
                    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    warn_action2->setVisible(caps);
#endif
                });
        caps_timer2->start(100);
        QAction *toggle_action2 = password_v->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")), QLineEdit::TrailingPosition);
        connect(toggle_action2, &QAction::triggered, this, [this, toggle_action2]()
                {
            if (password_v->echoMode() == QLineEdit::Password) {
                password_v->setEchoMode(QLineEdit::Normal);
                toggle_action2->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/hide_password.svg")));
            } else {
                password_v->setEchoMode(QLineEdit::Password);
                toggle_action2->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")));
            } });
        input_stack = new QStackedWidget(this);
        input_stack->addWidget(password_v);
        QWidget *keyfile_page = new QWidget();
        QHBoxLayout *keyfile_layout = new QHBoxLayout(keyfile_page);
        keyfile_layout->setContentsMargins(0, 0, 0, 0);
        keyfile_path_v = new QLineEdit(this);
        keyfile_path_v->setPlaceholderText("Select a keyfile");
        keyfile_path_v->setReadOnly(true);
        QPushButton *btn_browse_keyfile = new QPushButton("Browse", this);
        keyfile_layout->addWidget(keyfile_path_v);
        keyfile_layout->addWidget(btn_browse_keyfile);
        input_stack->addWidget(keyfile_page);
        QLabel *lbl_pass = new QLabel("Password / keyfile\n(CTRL + SHIFT + K to toggle):", this);
        form->addRow(lbl_pass, input_stack);
        QShortcut *shortcut_keyfile = new QShortcut(QKeySequence("Ctrl+Shift+K"), this);
        connect(shortcut_keyfile, &QShortcut::activated, this, [this]()
                {
            m_use_keyfile_mode = !m_use_keyfile_mode;
            input_stack->setCurrentIndex(m_use_keyfile_mode ? 1 : 0); });
        connect(btn_browse_keyfile, &QPushButton::clicked, this, [this]()
                {
            QString path = QFileDialog::getOpenFileName(this, "Select keyfile", "", "MAGE keyfiles (*.mgkx) ;; All files (*)");
            if (!path.isEmpty()) keyfile_path_v->setText(path); });
        main_layout->addLayout(form);
        main_layout->addStretch();
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_decrypt = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/decrypt.svg")), " Decrypt archive", this);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Cancel", this);
        action_layout->addWidget(btn_decrypt);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        connect(btn_browse_arch, &QPushButton::clicked, this, &cd_decrypt_archive::on_browse_archive);
        connect(btn_browse_out, &QPushButton::clicked, this, &cd_decrypt_archive::on_browse_output);
        connect(btn_decrypt, &QPushButton::clicked, this, &cd_decrypt_archive::on_decrypt);
        connect(btn_cancel, &QPushButton::clicked, this, &cd_decrypt_archive::on_cancel);
    }
    void cd_decrypt_archive::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    void cd_decrypt_archive::dropEvent(QDropEvent *event)
    {
        for (const QUrl &url : event->mimeData()->urls())
        {
            QString path = url.toLocalFile();
            if (!path.isEmpty())
            {
                _output_path->setText(path);
                update_default_path(path);
                break;
            }
        }
    }
    void cd_decrypt_archive::update_default_path(const QString &path)
    {
        if (output_dir_->text() == ".")
        {
            QFileInfo fi(path);
            output_dir_->setText(fi.absolutePath());
        }
    }
    void cd_decrypt_archive::on_browse_archive()
    {
        QString path = QFileDialog::getOpenFileName(this, "Select archive", "", "All files (*.*)");
        if (!path.isEmpty())
        {
            _output_path->setText(path);
            update_default_path(path);
        }
    }
    void cd_decrypt_archive::on_browse_output()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Select output directory");
        if (!dir.isEmpty())
            output_dir_->setText(dir);
    }
    void cd_decrypt_archive::on_decrypt()
    {
        if (_output_path->text().isEmpty())
        {
            warning(this, "ERROR", "Archive path is empty.");
            return;
        }
        if (output_dir_->text().isEmpty())
        {
            warning(this, "ERROR", "Extraction directory is empty.");
            return;
        }
        QString resolved_out = output_dir_->text();
        if (resolved_out == ".")
        {
            QFileInfo fi(_output_path->text());
            resolved_out = fi.absolutePath();
            output_dir_->setText(resolved_out);
        }
        QString err_msg;
        if (!pk::ui::outs::valid_path_probable(resolved_out, &err_msg))
        {
            warning(this, "ERROR", err_msg);
            return;
        }
        if (!QFileInfo::exists(resolved_out) || !QFileInfo(resolved_out).isDir())
        {
            warning(this, "ERROR", "Output directory does not exist: " + resolved_out);
            return;
        }
        if (!m_use_keyfile_mode && password_v->text().isEmpty())
        {
            warning(this, "ERROR", "Password is empty.");
            return;
        }
        if (m_use_keyfile_mode && keyfile_path_v->text().isEmpty())
        {
            warning(this, "ERROR", "Keyfile is not selected.");
            return;
        }
        worker::crypto_worker *worker = new worker::crypto_worker(worker::crypto_worker::mode::unpack);
        worker->ss_def_unpk_params(_output_path->text(), resolved_out, password_v->text(), m_use_keyfile_mode, keyfile_path_v->text());
        progress_dialog pd(worker, this);
        if (pd.exec() == QDialog::Accepted)
        {
            info(this, "Success", "Archive successfully decrypted.");
            accept();
        }
        else
        {
            if (!pd.what_err_msg().isEmpty())
            {
                error(this, "Something went wrong!!!", pd.what_err_msg());
            }
        }
    }
    void cd_decrypt_archive::on_cancel()
    {
        reject();
    }
    cd_settings::cd_settings(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Settings");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(450, 350);
        setup_ui();
        dont_burn_my_eyes(this);
    }
    void cd_settings::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QTabWidget *tabs = new QTabWidget(this);
        QWidget *tab_gen = new QWidget();
        QVBoxLayout *layout_gen = new QVBoxLayout(tab_gen);
        QFormLayout *form_out = new QFormLayout();
        def_output = new QLineEdit(this);
        def_output->setContextMenuPolicy(Qt::NoContextMenu);
        def_output->setText(pk::cfg::settings::instance().def_output_path());
        form_out->addRow("Default output PATH:", def_output);
        def_ext__ = new QLineEdit(this);
        def_ext__->setContextMenuPolicy(Qt::NoContextMenu);
        def_ext__->setText(pk::cfg::settings::instance().def_ext());
        form_out->addRow("Default extension:", def_ext__);
        layout_gen->addLayout(form_out);
        mute_sfx_v = new QCheckBox("Mute SFX", this);
        mute_sfx_v->setChecked(pk::cfg::settings::instance().mute_sfx());
        layout_gen->addWidget(mute_sfx_v);
        QFormLayout *form_vol = new QFormLayout();
        sfx_vol = new QSlider(Qt::Horizontal, this);
        sfx_vol->setRange(0, 100);
        sfx_vol->setValue(pk::cfg::settings::instance().sfx_volume());
        form_vol->addRow("SFX volume:", sfx_vol);
        layout_gen->addLayout(form_vol);
        _include_hidden = new QCheckBox("Include hidden files / directories", this);
        _include_hidden->setChecked(pk::cfg::settings::instance().include_hidden());
        layout_gen->addWidget(_include_hidden);
        __cp_metadata = new QCheckBox("Copy file properties to metadata", this);
        __cp_metadata->setChecked(pk::cfg::settings::instance().cp_metadata());
        layout_gen->addWidget(__cp_metadata);
        QHBoxLayout *chunk_layout = new QHBoxLayout();
        chunk_layout->addWidget(new QLabel("Default chunk size:", this));
        chunk_size_used = new QSpinBox(this);
        chunk_size_used->setRange(1, 64);
        chunk_size_used->setSuffix(" MB");
        chunk_size_used->setValue(pk::cfg::settings::instance().def_chunk_size());
        chunk_size_used->setContextMenuPolicy(Qt::NoContextMenu);
        chunk_layout->addWidget(chunk_size_used);
        chunk_layout->addStretch();
        layout_gen->addLayout(chunk_layout);
        QString tooltips_path = QCoreApplication::applicationDirPath() + "/assets/txt_data/tooltips";
        QMap<QString, QString> tooltips = parse_tooltips(tooltips_path);
        if (tooltips.contains("def_output_path"))
            def_output->setToolTip(tooltips["def_output_path"]);
        if (tooltips.contains("def_ext"))
            def_ext__->setToolTip(tooltips["def_ext"]);
        if (tooltips.contains("mute_sfx"))
            mute_sfx_v->setToolTip(tooltips["mute_sfx"]);
        if (tooltips.contains("sfx_vol"))
            sfx_vol->setToolTip(tooltips["sfx_vol"]);
        if (tooltips.contains("include_hidden"))
            _include_hidden->setToolTip(tooltips["include_hidden"]);
        if (tooltips.contains("copy_file_properties"))
            __cp_metadata->setToolTip(tooltips["copy_file_properties"]);
        if (tooltips.contains("chunk_size"))
            chunk_size_used->setToolTip(tooltips["chunk_size"]);
        QCheckBox *m_disable_rc = new QCheckBox("Disable mouse clicking", this);
        layout_gen->addWidget(m_disable_rc);
        connect(m_disable_rc, &QCheckBox::clicked, this, []()
                {
            class mouse_blocker : public QObject {
            protected:
                bool eventFilter(QObject*, QEvent* e) override {
                    switch (e->type()) {
                        case QEvent::MouseButtonPress:
                        case QEvent::MouseButtonRelease:
                        case QEvent::MouseButtonDblClick:
                        case QEvent::MouseMove:
                        case QEvent::Wheel:
                            return true; // swallow it 👀
                        default:
                            return false;
                    }
                }
            };
            static mouse_blocker* blocker = nullptr;
            if (!blocker) {
                blocker = new mouse_blocker();
                qApp->installEventFilter(blocker);
            }
            QLabel* w = new QLabel();
            w->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
            w->setAttribute(Qt::WA_TranslucentBackground);
            w->setAttribute(Qt::WA_DeleteOnClose);
            QScreen *screen = QApplication::primaryScreen();
            w->setGeometry(screen->geometry());
            w->setAlignment(Qt::AlignCenter);
            QPixmap pm(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/new_era_of_desktop.jpg"));
            if (!pm.isNull()) {
                w->setPixmap(pm.scaled(w->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                w->setText("Asset not found :(");
                w->setStyleSheet("color: red; font-size: 48px; background: black;");
            }
            w->show();
            pk::ui::sfx::play_by_name("bell.wav");
            QPropertyAnimation* anim = new QPropertyAnimation(w, "windowOpacity", w);
            anim->setDuration(3000);
            anim->setStartValue(1.0);
            anim->setEndValue(0.0);
            anim->setEasingCurve(QEasingCurve::OutQuad);
            anim->start();
            QObject::connect(anim, &QPropertyAnimation::finished, w, &QLabel::close); });
        layout_gen->addStretch();
        tabs->addTab(tab_gen, "General");
        QWidget *tab_enc = new QWidget();
        QFormLayout *form_enc = new QFormLayout(tab_enc);
        algo_combo = new QComboBox(this);
        algo_combo->addItem("AES-256-GCM");
        algo_combo->addItem("XChaCha20-Poly1305");
        algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cipher());
        form_enc->addRow("Default Cipher:", algo_combo);
        QLabel *aes_ni_label2 = new QLabel(this);
        if (pk::crypto::aes_ni_there())
        {
            aes_ni_label2->setText("AES-NI: yeah");
            aes_ni_label2->setStyleSheet("QLabel { color: #a0dca0; font-size: 11px; }");
            aes_ni_label2->setToolTip("Your CPU supports AES-NI, making AES-256-GCM extremely fast.");
        }
        else
        {
            aes_ni_label2->setText("AES-NI: nah");
            aes_ni_label2->setStyleSheet("QLabel { color: #e0d060; font-size: 11px; }");
            aes_ni_label2->setToolTip("Your CPU does not support AES-NI; XChaCha20-Poly1305 is recommended\nover AES-256-GCM for better performance and security on this system.");
        }
        form_enc->addRow("", aes_ni_label2);
        s_tc = new QSpinBox(this);
        s_tc->setRange(1, 100);
        s_tc->setValue(pk::cfg::settings::instance().def_time_cost());
        s_tc->setContextMenuPolicy(Qt::NoContextMenu);
        s_mem_cost = new QSpinBox(this);
        s_mem_cost->setRange(1, 4096);
        s_mem_cost->setValue(pk::cfg::settings::instance().def_mem_cost() / 1024);
        s_mem_cost->setSuffix(" MB");
        s_mem_cost->setContextMenuPolicy(Qt::NoContextMenu);
        s_cores = new QSpinBox(this);
        s_cores->setRange(1, 64);
        s_cores->setValue(pk::cfg::settings::instance().def_cores());
        s_cores->setContextMenuPolicy(Qt::NoContextMenu);
        form_enc->addRow("Default Argon2id time cost:", s_tc);
        form_enc->addRow("Default Argon2id mem_:", s_mem_cost);
        form_enc->addRow("Default Argon2id parallelism:", s_cores);
        tabs->addTab(tab_enc, "Encryption and KDF");
        QWidget *tab_comp = new QWidget();
        QFormLayout *form_comp = new QFormLayout(tab_comp);
        cmp_algo_combo = new QComboBox(this);
        cmp_algo_combo->addItem("None");
        cmp_algo_combo->addItem("ZSTD");
        cmp_algo_combo->addItem("LZMA2");
        cmp_algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_algorithm());
        form_comp->addRow("Default Algorithm:", cmp_algo_combo);
        cmp_preset_combo = new QComboBox(this);
        cmp_preset_combo->addItem("None");
        cmp_preset_combo->addItem("Normal");
        cmp_preset_combo->addItem("Good");
        cmp_preset_combo->addItem("ULTRAKILL");
        cmp_preset_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_preset());
        form_comp->addRow("Default Preset:", cmp_preset_combo);
        cmp_use_raw = new QCheckBox("Use raw levels instead", this);
        cmp_use_raw->setChecked(pk::cfg::settings::instance().ss_raw_cmp());
        form_comp->addRow(cmp_use_raw);
        _cmp_raw_lvl = new QSpinBox(this);
        _cmp_raw_lvl->setRange(0, 22);
        _cmp_raw_lvl->setValue(pk::cfg::settings::instance().def_cmp_lvl());
        _cmp_raw_lvl->setContextMenuPolicy(Qt::NoContextMenu);
        form_comp->addRow("Default raw level:", _cmp_raw_lvl);
        auto update_comp_ui = [this, form_comp]()
        {
            int algo = cmp_algo_combo->currentIndex();
            bool use_raw = cmp_use_raw->isChecked();
            auto set_enabled = [&](QWidget *w, bool enabled)
            {
                w->setEnabled(enabled);
                if (QWidget *label = form_comp->labelForField(w))
                {
                    label->setEnabled(enabled);
                }
            };
            if (algo == 0)
            {
                set_enabled(cmp_preset_combo, false);
                set_enabled(cmp_use_raw, false);
                set_enabled(_cmp_raw_lvl, false);
            }
            else
            {
                set_enabled(cmp_use_raw, true);
                if (use_raw)
                {
                    set_enabled(cmp_preset_combo, false);
                    set_enabled(_cmp_raw_lvl, true);
                    if (algo == 1)
                        _cmp_raw_lvl->setRange(0, 22);
                    else if (algo == 2)
                        _cmp_raw_lvl->setRange(0, 9);
                }
                else
                {
                    set_enabled(cmp_preset_combo, true);
                    set_enabled(_cmp_raw_lvl, false);
                }
            }
        };
        connect(cmp_algo_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update_comp_ui);
        connect(cmp_use_raw, &QCheckBox::checkStateChanged, this, update_comp_ui);
        update_comp_ui();
        tabs->addTab(tab_comp, "Compression");
        main_layout->addWidget(tabs);
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_save = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/save.svg")), " Save", this);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Close", this);
        action_layout->addWidget(btn_save);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        connect(btn_save, &QPushButton::clicked, this, &cd_settings::on_save);
        connect(btn_cancel, &QPushButton::clicked, this, &cd_settings::on_cancel);
    }
    void cd_settings::on_save()
    {
        QString def_out = def_output->text();
        QString err_msg;
        if (!pk::ui::outs::valid_path_probable(def_out, &err_msg))
        {
            warning(this, "ERROR", err_msg);
            return;
        }
        QString ext = def_ext__->text();
        if (!ext.startsWith("."))
            ext = "." + ext;
        if (!pk::ui::outs::r_u_a_valid_filename(ext, &err_msg))
        {
            warning(this, "ERROR", err_msg);
            return;
        }
        auto &s = pk::cfg::settings::instance();
        s.ss_def_output_path(def_out);
        s.ss_def_ext(ext);
        s.ss_def_mute_sfx(mute_sfx_v->isChecked());
        s.ss_def_sfx_vol(sfx_vol->value());
        s.ss_def_include_hidden(_include_hidden->isChecked());
        s.set_cp_metadata(__cp_metadata->isChecked());
        s.ss_def_chunk_size(chunk_size_used->value());
        s.ss_def_cipher(algo_combo->currentIndex());
        s.ss_def_tc(s_tc->value());
        s.ss_def_mem_kbs(s_mem_cost->value() * 1024);
        s.ss_def_cores(s_cores->value());
        s.ss_def_cmp(cmp_algo_combo->currentIndex());
        s.ss_def_cmp_preset(cmp_preset_combo->currentIndex());
        s.ss_def_cmp_raw(_cmp_raw_lvl->value());
        s.ss_def_use_raw_cmp(cmp_use_raw->isChecked());
        s.save();
        info(this, "Success", "Settings successfully saved.");
    }
    void cd_settings::on_cancel()
    {
        reject();
    }
    void cd_about_mage(QWidget *parent)
    {
        QDialog dialog(parent);
        dont_burn_my_eyes(&dialog);
        dialog.setWindowTitle("About MAGE");
        dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        dialog.setFixedSize(450, 240);
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QLabel *lbl_title = new QLabel("<b>MAGE - Make actually good encryption</b>", &dialog);
        layout->addWidget(lbl_title);
        QFrame *line1 = new QFrame(&dialog);
        line1->setFrameShape(QFrame::HLine);
        line1->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line1);
        QLabel *lbl_info = new QLabel(
            "Version: v0.1a\n"
            "Build: " __DATE__ " " __TIME__ "\n\n"
            "Made by Common, just Common.\n"
            "Doći ću ti u snovima...",
            &dialog);
        layout->addWidget(lbl_info);
        QFrame *line2 = new QFrame(&dialog);
        line2->setFrameShape(QFrame::HLine);
        line2->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line2);
        QLabel *lbl_quote = new QLabel("The best encryption software is audited! MAGE is not audited; therefore MAGE is not the best.", &dialog);
        lbl_quote->setWordWrap(true);
        layout->addWidget(lbl_quote);
        QHBoxLayout *btn_layout = new QHBoxLayout();
        btn_layout->addStretch();
        QPushButton *btn_ok = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " Fair enough", &dialog);
        QObject::connect(btn_ok, &QPushButton::clicked, &dialog, &QDialog::accept);
        btn_layout->addWidget(btn_ok);
        btn_layout->addStretch();
        layout->addLayout(btn_layout);
        dialog.exec();
    }
}

// end