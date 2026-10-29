// outs.cpp
// last updated: 28/10/2026
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
#include <QTabBar>
#include <QWheelEvent>
#include "../.hpp/keyfile.hpp"
#include <QStackedWidget>
#include <QScrollArea>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTextEdit>
#include "../.hpp/logger.hpp"
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
#include <QListWidget>
#include <QPixmap>
#include <QListView>
#include <QTreeView>
#include <QTreeWidget>
#include <QHeaderView>
#include <QClipboard>
#include <QFileInfo>
#include <QIcon>
#include <QRegularExpression>
#include "../.hpp/error_msg.hpp"
#include "../.hpp/aes_ni.hpp"
#include "../.hpp/path_handler.hpp"
#include "../.hpp/am_i_evil.hpp"
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
#include "../.hpp/cm.hpp"
#include <QDesktopServices>
#include <QUrl>
#include <unordered_set>
#include <algorithm>
#include "../.hpp/secure_memory.hpp"
namespace pk::ui::outs
{
    static QString sexify_tooltip(const QString &raw)
    {
        QStringList lines = raw.trimmed().split('\n');
        if (lines.isEmpty())
            return raw;

        QString title = lines[0].trimmed();
        QString body;
        for (int i = 1; i < lines.size(); ++i)
        {
            QString l = lines[i].trimmed();
            if (l.isEmpty())
            {
                if (!body.isEmpty() && !body.endsWith("<br><br>"))
                    body += "<br><br>";
            }
            else
            {
                if (!body.isEmpty() && !body.endsWith("<br><br>"))
                    body += " ";
                body += l.toHtmlEscaped();
            }
        }
        if (body.isEmpty())
        {
            return QString("<div style='max-width: 340px;'><b style='color: #ffffff;'>%1</b></div>").arg(title.toHtmlEscaped());
        }
        return QString("<div style='max-width: 340px;'>"
                       "<b style='color: #ffffff;'>%1</b><br><br>"
                       "<span style='color: #d0d0d0;'>%2</span>"
                       "</div>")
            .arg(title.toHtmlEscaped(), body);
    }
    static QMap<QString, QString> parse_tooltips(const QString &___filepath)
    {
        QMap<QString, QString> tooltips;
        QFile file(___filepath);
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
                if (line.endsWith("\"\"\""))
                {
                    int eq_pos = line.indexOf('=');
                    if (eq_pos != -1)
                    {
                        int first_quote = line.indexOf("\"\"\"");
                        int last_quote = line.lastIndexOf("\"\"\"");
                        QString key = line.left(eq_pos).trimmed();
                        if (first_quote != last_quote && first_quote != -1)
                        {
                            QString raw = line.mid(first_quote + 3, last_quote - first_quote - 3).trimmed();
                            tooltips[key] = sexify_tooltip(raw);
                        }
                        else
                        {
                            current_key = key;
                            in_value = true;
                            current_text.clear();
                        }
                    }
                }
            }
            else
            {
                if (line.trimmed() == "\"\"\"")
                {
                    tooltips[current_key] = sexify_tooltip(current_text);
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
            DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), 19, &dark, sizeof(dark));
            DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), 20, &dark, sizeof(dark));
        }
#endif
    }
    enum class msg_type
    {
        info,
        warning,
        error,
        ask
    };
    class custom_msg_dialog : public QDialog
    {
    public:
        custom_msg_dialog(
            const QString &title,
            const QString &message,
            msg_type type = msg_type::info,
            QWidget *parent = nullptr,
            const QString &suggestion = QString(),
            const QString &details = QString())
            : QDialog(parent)
        {
            setWindowTitle(title);
            setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint) | Qt::MSWindowsFixedSizeDialogHint);
            dont_burn_my_eyes(this);
            QVBoxLayout *main_layout = new QVBoxLayout(this);
            main_layout->setSpacing(12);
            main_layout->setContentsMargins(18, 18, 18, 14);
            QHBoxLayout *header_layout = new QHBoxLayout();
            header_layout->setSpacing(12);
            QLabel *lbl_icon = new QLabel(this);
            QString icon_name = "warn.svg";
            if (type == msg_type::info)
                icon_name = "ok.svg";
            else if (type == msg_type::ask)
                icon_name = "about.svg";
            QIcon icon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/" + icon_name));
            lbl_icon->setPixmap(icon.pixmap(36, 36));
            lbl_icon->setFixedSize(36, 36);
            lbl_icon->setAlignment(Qt::AlignCenter);
            header_layout->addWidget(lbl_icon, 0, Qt::AlignTop);
            QLabel *lbl_title = new QLabel(title, this);
            lbl_title->setStyleSheet("font-weight: bold; font-size: 13pt; color: #ffffff;");
            lbl_title->setWordWrap(true);
            header_layout->addWidget(lbl_title, 1, Qt::AlignVCenter);
            main_layout->addLayout(header_layout);
            QTextEdit *txt_message = new QTextEdit(this);
            txt_message->setPlainText(message);
            txt_message->setReadOnly(true);
            txt_message->setTextInteractionFlags(Qt::NoTextInteraction);
            txt_message->setFocusPolicy(Qt::NoFocus);
            txt_message->viewport()->setCursor(Qt::ArrowCursor);
            txt_message->setFrameShape(QFrame::NoFrame);
            txt_message->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            txt_message->setStyleSheet(
                "QTextEdit { background-color: transparent; color: #e0e0e0; "
                "font-size: 10pt; border: none; padding: 2px 2px; }"
                "QScrollBar:vertical { background: #1e1e1e; width: 8px; border-radius: 4px; }"
                "QScrollBar::handle:vertical { background: #444444; border-radius: 4px; min-height: 20px; }"
                "QScrollBar::handle:vertical:hover { background: #666666; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
            const int dlg_w = 640;
            const int inner_w = dlg_w - 36;
            txt_message->document()->setTextWidth(inner_w);
            int doc_h = static_cast<int>(std::ceil(txt_message->document()->size().height())) + 6;
            txt_message->document()->setTextWidth(-1);
            int target_h = std::clamp(doc_h, 45, 360);
            txt_message->setFixedHeight(target_h);
            main_layout->addWidget(txt_message);
            if (!suggestion.trimmed().isEmpty())
            {
                QFrame *sug_card = new QFrame(this);
                sug_card->setObjectName("sug_card");
                sug_card->setStyleSheet("#sug_card { background-color: #272727; border-left: 3px solid #007acc; border-radius: 2px; padding: 6px 10px; }");
                QVBoxLayout *sug_layout = new QVBoxLayout(sug_card);
                sug_layout->setContentsMargins(6, 4, 6, 4);
                sug_layout->setSpacing(2);
                QLabel *sug_label = new QLabel(suggestion, sug_card);
                sug_label->setWordWrap(true);
                sug_label->setTextInteractionFlags(Qt::NoTextInteraction);
                sug_label->setFocusPolicy(Qt::NoFocus);
                sug_label->setStyleSheet("border: none; background: transparent; color: #b0c4de; font-size: 9.5pt;");
                sug_layout->addWidget(sug_label);
                main_layout->addWidget(sug_card);
            }
            if (!details.trimmed().isEmpty() && details.trimmed() != message.trimmed())
            {
                QPushButton *btn_details = new QPushButton("Show details", this);
                btn_details->setStyleSheet("QPushButton { background-color: transparent; border: none; color: #888888; font-size: 9pt; text-align: left; padding: 2px 0; } QPushButton:hover { color: #007acc; text-decoration: underline; }");
                QTextEdit *text_details = new QTextEdit(this);
                text_details->setPlainText(details);
                text_details->setReadOnly(true);
                text_details->setTextInteractionFlags(Qt::NoTextInteraction);
                text_details->setFocusPolicy(Qt::NoFocus);
                text_details->setMaximumHeight(130);
                text_details->setStyleSheet("QTextEdit { background-color: #1a1a1a; color: #aaaaaa; font-family: 'Consolas', 'Courier New', monospace; font-size: 8.5pt; border: 1px solid #444444; border-radius: 2px; padding: 4px; }");
                text_details->setVisible(false);
                connect(btn_details, &QPushButton::clicked, this, [btn_details, text_details, this, dlg_w]()
                        {
                    bool visible = !text_details->isVisible();
                    text_details->setVisible(visible);
                    btn_details->setText(visible ? "Hide details" : "Show details");
                    this->adjustSize();
                    this->setFixedSize(dlg_w, this->sizeHint().height()); });
                main_layout->addWidget(btn_details);
                main_layout->addWidget(text_details);
            }
            QHBoxLayout *btn_layout = new QHBoxLayout();
            btn_layout->addStretch();
            if (type == msg_type::ask)
            {
                QPushButton *btn_yes = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " Yes", this);
                QPushButton *btn_no = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " No", this);
                connect(btn_yes, &QPushButton::clicked, this, &QDialog::accept);
                connect(btn_no, &QPushButton::clicked, this, &QDialog::reject);
                btn_layout->addWidget(btn_yes);
                btn_layout->addWidget(btn_no);
            }
            else
            {
                QPushButton *btn_ok = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " OK", this);
                btn_ok->setDefault(true);
                connect(btn_ok, &QPushButton::clicked, this, &QDialog::accept);
                btn_layout->addWidget(btn_ok);
            }
            main_layout->addLayout(btn_layout);
            adjustSize();
            setFixedSize(dlg_w, sizeHint().height());
        }
    };
    void info(QWidget *parent, const QString &title, const QString &message, const QString &suggestion, bool play_sfx)
    {
        if (play_sfx)
            pk::ui::sfx::play_info();
        custom_msg_dialog dlg(title, message, msg_type::info, parent, suggestion);
        dlg.exec();
    }
    void warning(QWidget *parent, const QString &title, const QString &message, const QString &suggestion)
    {
        pk::ui::sfx::play_error();
        custom_msg_dialog dlg(title, message, msg_type::warning, parent, suggestion);
        dlg.exec();
    }
    void error(QWidget *parent, const QString &title, const QString &message, const QString &suggestion, const QString &details)
    {
        pk::ui::sfx::play_error();
        custom_msg_dialog dlg(title, message, msg_type::error, parent, suggestion, details);
        dlg.exec();
    }
    void show_error(QWidget *parent, const QString &raw_or_formatted_error)
    {
        auto desc = pk::error::parse(raw_or_formatted_error.toStdString());
        error(parent,
              QString::fromStdString(desc.title),
              QString::fromStdString(desc.message),
              QString::fromStdString(desc.suggestion),
              QString::fromStdString(desc.technical));
    }
    bool ask(QWidget *parent, const QString &title, const QString &message)
    {
        custom_msg_dialog dlg(title, message, msg_type::ask, parent);
        return dlg.exec() == QDialog::Accepted;
    }
    void cd_prog_dialog::setup_ui(const QString &initial_title)
    {
        setWindowTitle(initial_title);
        setFixedSize(550, 340);
        setWindowFlags((windowFlags() | Qt::Window | Qt::WindowMinimizeButtonHint) & ~Qt::WindowCloseButtonHint);
        setModal(true);
        dont_burn_my_eyes(this);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSpacing(8);
        layout->setContentsMargins(16, 16, 16, 14);
        QLabel *hdr_progress = new QLabel("Overall progress", this);
        hdr_progress->setStyleSheet("font-weight: bold; font-size: 11pt; color: #ffffff;");
        layout->addWidget(hdr_progress);
        lbl_overall = new QLabel("Preparing...", this);
        lbl_overall->setStyleSheet("font-size: 9.5pt; color: #cccccc;");
        layout->addWidget(lbl_overall);
        lbl_file = new QLabel(" ", this);
        lbl_file->setStyleSheet("font-size: 9pt; color: #888888;");
        layout->addWidget(lbl_file);
        progress___ = new QProgressBar(this);
        progress___->setRange(0, 100);
        progress___->setValue(0);
        progress___->setTextVisible(true);
        progress___->setFormat("0%");
        progress___->setFixedHeight(22);
        layout->addWidget(progress___);
        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #333333; background-color: #333333; max-height: 1px;");
        layout->addWidget(sep);
        QLabel *hdr_stats = new QLabel("Stats", this);
        hdr_stats->setStyleSheet("font-weight: bold; font-size: 10pt; color: #cccccc;");
        layout->addWidget(hdr_stats);
        QGridLayout *stats_layout = new QGridLayout();
        stats_layout->setVerticalSpacing(4);
        stats_layout->setHorizontalSpacing(16);
        auto make_hdr = [this](const QString &t)
        {
            QLabel *l = new QLabel(t, this);
            l->setStyleSheet("color: #888888; font-size: 9pt;");
            return l;
        };
        lbl_proc = new QLabel("-", this);
        lbl_sped = new QLabel("-", this);
        lbl_elapsed = new QLabel("00:00:00", this);
        lbl_eta = new QLabel("Calculating...", this);
        lbl_file_count = new QLabel("-", this);
        int row = 0;
        stats_layout->addWidget(make_hdr("Processed:"), row, 0);
        stats_layout->addWidget(lbl_proc, row, 1);
        row++;
        stats_layout->addWidget(make_hdr("Speed:"), row, 0);
        stats_layout->addWidget(lbl_sped, row, 1);
        row++;
        stats_layout->addWidget(make_hdr("Elapsed:"), row, 0);
        stats_layout->addWidget(lbl_elapsed, row, 1);
        row++;
        stats_layout->addWidget(make_hdr("ETA:"), row, 0);
        stats_layout->addWidget(lbl_eta, row, 1);
        row++;
        stats_layout->addWidget(make_hdr("Files:"), row, 0);
        stats_layout->addWidget(lbl_file_count, row, 1);
        layout->addLayout(stats_layout);
        layout->addStretch();
        QHBoxLayout *btn_layout = new QHBoxLayout();
        btn_layout->addStretch();
        btn_sendtobk = new QPushButton("Background", this);
        connect(btn_sendtobk, &QPushButton::clicked, this, &cd_prog_dialog::on_background);
        btn_layout->addWidget(btn_sendtobk);
        btn_cancel = new QPushButton("Cancel", this);
        connect(btn_cancel, &QPushButton::clicked, this, &cd_prog_dialog::reject);
        btn_layout->addWidget(btn_cancel);
        layout->addLayout(btn_layout);
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &cd_prog_dialog::on_update_stats);
        timer->start(500);
        elapsed.start();
        speed_timer.start();
    }
    void cd_prog_dialog::connect_worker(worker::crypto_worker *w)
    {
        if (!w)
            return;
        connect(w, &worker::crypto_worker::success, this, &cd_prog_dialog::on_success);
        connect(w, &worker::crypto_worker::error, this, &cd_prog_dialog::on_error);
        connect(w, &worker::crypto_worker::progress, this, &cd_prog_dialog::on_progress);
        connect(w, &worker::crypto_worker::pr_details, this, &cd_prog_dialog::on_pr_details);
        connect(w, &worker::crypto_worker::scan_stats, this, &cd_prog_dialog::on_scan_stats);
        connect(w, &worker::crypto_worker::file_counts, this, &cd_prog_dialog::on_file_counts);
        connect(w, &worker::crypto_worker::work_fraction, this, &cd_prog_dialog::on_work_fraction);
        connect(w, &worker::crypto_worker::current_ac0, this, &cd_prog_dialog::on_current_ac0);
        connect(w, &worker::crypto_worker::current_file, this, &cd_prog_dialog::on_current_file);
        connect(w, &worker::crypto_worker::need_fg, this, &cd_prog_dialog::restore_from_tray);
        connect(w, &QThread::finished, w, &QObject::deleteLater);
    }
    cd_prog_dialog::cd_prog_dialog(worker::crypto_worker *worker, QWidget *parent)
        : QDialog(parent), m2_worker(worker), curr_job_idx(0), bm(false), is_cancelled(false),
          last_processed(0), current_proc(0), tot_bytes(0), current_pct(0), _success(false)
    {
        QString base_title = "Creating archive";
        if (m2_worker)
        {
            if (m2_worker->what_mode() == worker::crypto_worker::mode::unpack)
                base_title = "Decrypting archive";
            else if (m2_worker->what_mode() == worker::crypto_worker::mode::am_i_evil)
                base_title = "Verifying archive";
        }
        setup_ui(base_title + " - 0%");
        if (m2_worker)
        {
            if (m2_worker->what_mode() == worker::crypto_worker::mode::pack)
                is_scanning = true;
            else
                is_kdf = true;
            connect_worker(m2_worker);
            m2_worker->start();
        }
    }
    cd_prog_dialog::cd_prog_dialog(const QList<decrypt_job> &jobs, QWidget *parent)
        : QDialog(parent), m2_worker(nullptr), curr_job_idx(0), bm(true), is_cancelled(false),
          last_processed(0), current_proc(0), tot_bytes(0), current_pct(0), _success(false)
    {
        batch_total_weight = 0;
        for (const auto &j : jobs)
        {
            uint64_t disk_sz = static_cast<uint64_t>(std::max<qint64>(0, QFileInfo(j.archive_path).size()));
            uint64_t w = std::max<uint64_t>(65536ULL, disk_sz);
            batch_total_weight += w;
            m_jobs.append({worker::crypto_worker::mode::unpack, j.list_index, j.archive_path, j.display_name, j.password, j.keyfile, j.base_out, j.ext_behavior, j.ext_overwrite, w});
        }
        setup_ui("Decrypting archive - 0%");
        QTimer::singleShot(0, this, &cd_prog_dialog::start_next_job);
    }
    cd_prog_dialog::cd_prog_dialog(const QList<verify_job> &jobs, QWidget *parent)
        : QDialog(parent), m2_worker(nullptr), curr_job_idx(0), bm(true), is_cancelled(false),
          last_processed(0), current_proc(0), tot_bytes(0), current_pct(0), _success(false)
    {
        batch_total_weight = 0;
        for (const auto &j : jobs)
        {
            uint64_t disk_sz = static_cast<uint64_t>(std::max<qint64>(0, QFileInfo(j.archive_path).size()));
            uint64_t w = std::max<uint64_t>(65536ULL, disk_sz);
            batch_total_weight += w;
            m_jobs.append({worker::crypto_worker::mode::am_i_evil, j.list_index, j.archive_path, j.display_name, j.password, j.keyfile, "", 0, 0, w});
        }
        setup_ui("Verifying archive - 0%");
        QTimer::singleShot(0, this, &cd_prog_dialog::start_next_job);
    }
    cd_prog_dialog::~cd_prog_dialog()
    {
        if (__in_bk)
        {
            __in_bk = false;
            if (parentWidget())
            {
                parentWidget()->setWindowState((parentWidget()->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                parentWidget()->showNormal();
            }
            QApplication::setQuitOnLastWindowClosed(prev_quit_ol);
        }
        if (tr_ico)
        {
            tr_ico->setContextMenu(nullptr);
            tr_ico->hide();
            delete tr_ico;
            tr_ico = nullptr;
        }
        if (tr_menu)
        {
            tr_menu->hide();
            delete tr_menu;
            tr_menu = nullptr;
        }
        if (m2_worker && m2_worker->isRunning())
        {
            m2_worker->requestInterruption();
            m2_worker->wait(500);
        }
    }
    int cd_prog_dialog::exec()
    {
        if (done_called)
            return result();
        setResult(QDialog::Rejected);
        setModal(true);
        show();
        dont_burn_my_eyes(this);
        QEventLoop loop;
        exec_loop = &loop;
        connect(qApp, &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
        if (!done_called)
            loop.exec(QEventLoop::DialogExec);
        exec_loop = nullptr;
        return result();
    }
    void cd_prog_dialog::done(int r)
    {
        done_called = true;
        QDialog::done(r);
        if (exec_loop && exec_loop->isRunning())
            exec_loop->exit(r);
    }
    void cd_prog_dialog::closeEvent(QCloseEvent *event)
    {
        if (done_called)
        {
            QDialog::closeEvent(event);
            return;
        }
        event->ignore();
        reject();
    }
    void cd_prog_dialog::cleanup_tray()
    {
        if (tr_ico)
        {
            tr_ico->setContextMenu(nullptr);
            tr_ico->hide();
            tr_ico->deleteLater();
            tr_ico = nullptr;
        }
        if (tr_menu)
        {
            tr_menu->hide();
            tr_menu->deleteLater();
            tr_menu = nullptr;
        }
    }
    void cd_prog_dialog::start_next_job()
    {
        if (is_cancelled || curr_job_idx >= m_jobs.size())
        {
            if (is_cancelled)
            {
                for (int i = curr_job_idx; i < m_jobs.size(); ++i)
                {
                    bool already = false;
                    for (const auto &r : job_res__)
                    {
                        if (r.list_index == m_jobs[i].list_index)
                        {
                            already = true;
                            break;
                        }
                    }
                    if (!already)
                    {
                        job_res__.append({m_jobs[i].list_index, m_jobs[i].archive_path, m_jobs[i].display_name, false, "Operation cancelled by user.", true, {}});
                    }
                }
                reject();
                return;
            }
            current_pct = 100;
            progress___->setValue(100);
            progress___->setFormat("100%");
            bool all_succeeded = !job_res__.isEmpty();
            for (const auto &res : job_res__)
            {
                if (!res.success)
                {
                    all_succeeded = false;
                    break;
                }
            }
            if (all_succeeded)
            {
                pk::ui::sfx::play_success();
            }
            if (__in_bk)
            {
                restore_from_tray();
                QApplication::alert(parentWidget() ? parentWidget() : this);
            }
            cleanup_tray();
            accept();
            return;
        }
        const auto &job = m_jobs[curr_job_idx];
        last_processed = 0;
        current_proc = 0;
        tot_bytes = 0;
        done_files_count = 0;
        total_files_count = 0;
        local_job_pct = 0;
        phase_work_frac = 0.0;
        phase_elapsed.invalidate();
        is_scanning = false;
        is_kdf = true;
        current_file_text.clear();
        current_ac0_text.clear();
        lbl_file->setText(" ");
        lbl_file_count->setText(m_jobs.size() > 1
                                    ? QString("Archive %1 of %2").arg(curr_job_idx + 1).arg(m_jobs.size())
                                    : QString("1 of 1"));
        QString action_verb = (job.mode == worker::crypto_worker::mode::am_i_evil) ? "Verifying archive" : "Decrypting archive";
        lbl_overall->setText(QString("Archive %1 of %2: %3 (Deriving encryption key...)").arg(curr_job_idx + 1).arg(m_jobs.size()).arg(job.display_name));
        setWindowTitle(QString("%1 (%2/%3) - %4 - %5%").arg(action_verb).arg(curr_job_idx + 1).arg(m_jobs.size()).arg(job.display_name).arg(current_pct));
        if (tr_ico && tr_ico->isVisible())
        {
            tr_ico->setToolTip(windowTitle());
        }
        m2_worker = new worker::crypto_worker(job.mode);
        if (job.mode == worker::crypto_worker::mode::am_i_evil)
        {
            m2_worker->ss_def_verify_params(job.archive_path, job.password, job.keyfile);
        }
        else
        {
            m2_worker->ss_def_unpk_params(job.archive_path, job.base_out, job.password, job.keyfile, job.ext_behavior, job.ext_overwrite);
        }
        connect_worker(m2_worker);
        m2_worker->start();
        if (__in_bk)
        {
            m2_worker->setPriority(QThread::LowestPriority);
        }
    }
    void cd_prog_dialog::reject()
    {
        if (is_cancelled)
        {
            if (!m2_worker || !m2_worker->isRunning())
            {
                if (__in_bk)
                    restore_from_tray();
                cleanup_tray();
                QDialog::reject();
            }
            return;
        }
        is_cancelled = true;
        if (btn_cancel)
        {
            btn_cancel->setEnabled(false);
            btn_cancel->setText("Cancelling...");
        }
        if (btn_sendtobk)
        {
            btn_sendtobk->setEnabled(false);
        }
        if (lbl_overall)
            lbl_overall->setText("Cancelling operation...");

        if (m2_worker && m2_worker->isRunning())
        {
            m2_worker->requestInterruption();
            if (!m2_worker->wait(100))
            {
                connect(m2_worker, &QThread::finished, this, [this]()
                        {
                    if (bm)
                    {
                        for (int i = curr_job_idx; i < m_jobs.size(); ++i)
                        {
                            bool already = false;
                            for (const auto &r : job_res__)
                            {
                                if (r.list_index == m_jobs[i].list_index)
                                {
                                    already = true;
                                    break;
                                }
                            }
                            if (!already)
                            {
                                job_res__.append({m_jobs[i].list_index, m_jobs[i].archive_path, m_jobs[i].display_name, false, "Operation cancelled by user.", true, {}});
                            }
                        }
                    }
                    if (__in_bk)
                        restore_from_tray();
                    cleanup_tray();
                    QDialog::reject(); });
                return;
            }
        }
        if (bm)
        {
            for (int i = curr_job_idx; i < m_jobs.size(); ++i)
            {
                bool already = false;
                for (const auto &r : job_res__)
                {
                    if (r.list_index == m_jobs[i].list_index)
                    {
                        already = true;
                        break;
                    }
                }
                if (!already)
                {
                    job_res__.append({m_jobs[i].list_index, m_jobs[i].archive_path, m_jobs[i].display_name, false, "Operation cancelled by user.", true, {}});
                }
            }
        }
        if (__in_bk)
            restore_from_tray();
        cleanup_tray();
        QDialog::reject();
    }
    void cd_prog_dialog::changeEvent(QEvent *event)
    {
        QDialog::changeEvent(event);
        if (event->type() == QEvent::WindowStateChange)
        {
            if (isMinimized() && !__in_bk && !is_cancelled && !done_called)
            {
                QTimer::singleShot(0, this, &cd_prog_dialog::on_background);
            }
        }
    }
    void cd_prog_dialog::on_background()
    {
        if (__in_bk || is_cancelled || done_called)
            return;
        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
            showMinimized();
            return;
        }
        __in_bk = true;
        prev_quit_ol = QApplication::quitOnLastWindowClosed();
        QApplication::setQuitOnLastWindowClosed(false);
        if (m2_worker && m2_worker->isRunning())
            m2_worker->setPriority(QThread::LowestPriority);
        if (!tr_ico)
        {
            tr_ico = new QSystemTrayIcon(this);
            const QDir app_dir(QCoreApplication::applicationDirPath());
            QIcon icon = windowIcon();
            if (icon.isNull() || icon.pixmap(32, 32).isNull())
                icon = QApplication::windowIcon();
            if (icon.isNull() || icon.pixmap(32, 32).isNull())
                icon = QIcon(app_dir.absoluteFilePath("assets/imgs/s_icons/mage.ico"));
            if (icon.isNull() || icon.pixmap(32, 32).isNull())
                icon = QIcon(app_dir.absoluteFilePath("assets/imgs/s_icons/_/32.png"));
            if (icon.isNull() || icon.pixmap(32, 32).isNull())
                icon = QIcon(app_dir.absoluteFilePath("assets/imgs/s_icons/ink/mage_32.svg"));
            tr_ico->setIcon(icon);
            tr_menu = new QMenu(nullptr);
            tr_menu->setStyleSheet("QMenu::separator { height: 1px; background: #444444; margin: 4px 6px; }");
            QAction *act_bring_back = tr_menu->addAction(
                QIcon(app_dir.absoluteFilePath("assets/imgs/ok.svg")), "Bring back");
            connect(act_bring_back, &QAction::triggered, this, &cd_prog_dialog::restore_from_tray);
            QAction *act_cancel = tr_menu->addAction(
                QIcon(app_dir.absoluteFilePath("assets/imgs/cancel.svg")), "Cancel / halt");
            connect(act_cancel, &QAction::triggered, this, &cd_prog_dialog::on_tray_cancel);
            tr_menu->addSeparator();
            QAction *act_force_kill = tr_menu->addAction(
                QIcon(app_dir.absoluteFilePath("assets/imgs/warn.svg")), "Force kill (if bugged)");
            connect(act_force_kill, &QAction::triggered, this, &cd_prog_dialog::on_force_kill);
            tr_ico->setContextMenu(tr_menu);
            connect(tr_ico, &QSystemTrayIcon::activated, this, &cd_prog_dialog::on_tray_activated);
        }
        tr_ico->setToolTip(windowTitle().isEmpty() ? "MAGE operation in background" : windowTitle());
        tr_ico->show();
        hide();
        if (parentWidget())
            parentWidget()->hide();
    }
    void cd_prog_dialog::on_tray_activated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::MiddleClick)
        {
            restore_from_tray();
        }
    }
    void cd_prog_dialog::restore_from_tray()
    {
        if (!__in_bk)
            return;
        __in_bk = false;
        if (tr_menu)
            tr_menu->hide();
        if (tr_ico)
            tr_ico->hide();
        if (m2_worker && m2_worker->isRunning())
            m2_worker->setPriority(QThread::NormalPriority);
        if (parentWidget())
        {
            parentWidget()->setWindowState((parentWidget()->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            parentWidget()->showNormal();
            parentWidget()->raise();
            dont_burn_my_eyes(parentWidget());
        }
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        showNormal();
        raise();
        activateWindow();
        dont_burn_my_eyes(this);
#ifdef _WIN32
        SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
        QApplication::setQuitOnLastWindowClosed(prev_quit_ol);
    }
    void cd_prog_dialog::on_tray_cancel()
    {
        restore_from_tray();
        reject();
    }
    void cd_prog_dialog::on_force_kill()
    {
        is_cancelled = true;
        force_taskkill = true;
        current_err = "Operation cancelled (force killed).";
        if (timer)
            timer->stop();

        if (m2_worker)
        {
            QString partial_out;
            if (m2_worker->what_mode() == worker::crypto_worker::mode::pack)
            {
                partial_out = QString::fromUtf8(m2_worker->what_out_path().c_str());
            }
            m2_worker->disconnect(this);
            if (m2_worker->isRunning())
            {
                m2_worker->requestInterruption();
                if (!m2_worker->wait(150))
                {
                    m2_worker->terminate();
                    m2_worker->wait(500);
                }
            }
            m2_worker = nullptr;
            if (!partial_out.isEmpty() && QFileInfo::exists(partial_out))
            {
                QFile::remove(partial_out);
            }
        }
        if (bm)
        {
            for (int i = curr_job_idx; i < m_jobs.size(); ++i)
            {
                bool already = false;
                for (const auto &r : job_res__)
                {
                    if (r.list_index == m_jobs[i].list_index)
                    {
                        already = true;
                        break;
                    }
                }
                if (!already)
                {
                    job_res__.append({m_jobs[i].list_index, m_jobs[i].archive_path, m_jobs[i].display_name, false, "Operation force killed by user.", true, {}});
                }
            }
        }
        if (__in_bk)
            restore_from_tray();
        cleanup_tray();
        QDialog::reject();
    }
    void cd_prog_dialog::on_progress(int percentage)
    {
        percentage = std::clamp(percentage, 0, 100);
        if (bm)
        {
            if (percentage < local_job_pct)
                percentage = local_job_pct;
            local_job_pct = percentage;
            uint64_t cur_w = (curr_job_idx < m_jobs.size()) ? m_jobs[curr_job_idx].job_weight : 0ULL;
            double overall_frac = (static_cast<double>(batch_done_weight) + (static_cast<double>(local_job_pct) / 100.0) * static_cast<double>(cur_w)) /
                                  static_cast<double>(std::max<uint64_t>(1ULL, batch_total_weight));
            overall_work_frac = std::max(overall_work_frac, std::clamp(overall_frac, 0.0, 1.0));
            int cap = (curr_job_idx + 1 < m_jobs.size()) ? 99 : 100;
            int mapped = std::clamp(static_cast<int>(overall_work_frac * 100.0), 0, cap);
            if (mapped < current_pct)
                mapped = current_pct;
            current_pct = mapped;
        }
        else
        {
            if (percentage < current_pct)
                percentage = current_pct;
            current_pct = percentage;
        }
        progress___->setValue(current_pct);
        progress___->setFormat(QString::number(current_pct) + "%");
        if (bm)
        {
            QString action_verb = (curr_job_idx < m_jobs.size() && m_jobs[curr_job_idx].mode == worker::crypto_worker::mode::am_i_evil) ? "Verifying archive" : "Decrypting archive";
            if (curr_job_idx < m_jobs.size())
            {
                setWindowTitle(QString("%1 (%2/%3) - %4 - %5%").arg(action_verb).arg(curr_job_idx + 1).arg(m_jobs.size()).arg(m_jobs[curr_job_idx].display_name).arg(current_pct));
            }
            else
            {
                setWindowTitle(QString("%1 - %2%").arg(action_verb).arg(current_pct));
            }
        }
        else
        {
            QString base_title = "Creating archive";
            if (m2_worker)
            {
                if (m2_worker->what_mode() == worker::crypto_worker::mode::unpack)
                    base_title = "Decrypting archive";
                else if (m2_worker->what_mode() == worker::crypto_worker::mode::am_i_evil)
                    base_title = "Verifying archive";
            }
            setWindowTitle(base_title + " - " + QString::number(current_pct) + "%");
        }
        if (tr_ico && tr_ico->isVisible())
        {
            tr_ico->setToolTip(windowTitle());
        }
    }
    void cd_prog_dialog::on_pr_details(uint64_t processed, uint64_t total)
    {
        if (tot_bytes == 0 && total > 0 && !phase_elapsed.isValid())
            phase_elapsed.start();
        current_proc = processed;
        tot_bytes = total;
    }
    void cd_prog_dialog::on_scan_stats(uint64_t files_found, uint64_t bytes_found)
    {
        scan_files_found = files_found;
        scan_bytes_found = bytes_found;
    }
    void cd_prog_dialog::on_file_counts(uint64_t done_files, uint64_t total_files)
    {
        done_files_count = done_files;
        total_files_count = total_files;
    }
    void cd_prog_dialog::on_work_fraction(double fraction)
    {
        if (!phase_elapsed.isValid())
            phase_elapsed.start();
        phase_work_frac = std::clamp(fraction, 0.0, 1.0);
    }
    void cd_prog_dialog::on_current_ac0(const QString &action)
    {
        current_ac0_text = action;
        if (bm && curr_job_idx < m_jobs.size())
        {
            if (action.isEmpty() || action == "Preparing...")
            {
                lbl_overall->setText(QString("Archive %1 of %2: %3").arg(curr_job_idx + 1).arg(m_jobs.size()).arg(m_jobs[curr_job_idx].display_name));
            }
            else
            {
                lbl_overall->setText(QString("Archive %1 of %2: %3 (%4)").arg(curr_job_idx + 1).arg(m_jobs.size()).arg(m_jobs[curr_job_idx].display_name).arg(action));
            }
        }
        else
        {
            lbl_overall->setText(action);
        }
        bool new_scanning = action.contains("Discovering", Qt::CaseInsensitive);
        bool new_kdf = action.contains("Argon2id", Qt::CaseInsensitive) || action.contains("Deriving", Qt::CaseInsensitive);
        if ((is_kdf || is_scanning) && !new_kdf && !new_scanning && !phase_elapsed.isValid())
        {
            phase_elapsed.start();
            speed_timer.restart();
        }
        is_scanning = new_scanning;
        is_kdf = new_kdf;
    }
    void cd_prog_dialog::on_current_file(const QString &file)
    {
        current_file_text = file;
        QString display = file;
        if (display.length() > 65)
            display = "..." + display.right(62);
        lbl_file->setText(display);
    }
    void cd_prog_dialog::on_update_stats()
    {
        auto format_size = [](uint64_t s) -> QString
        {
            return QString::fromStdString(pk::crypto::am_i_evil::format_bytes(s));
        };
        auto commas = [](uint64_t n) -> QString
        {
            return QString::fromStdString(pk::crypto::am_i_evil::numbers_with_commas_unlike_in_gta_5(n));
        };
        auto format_speed = [](double bps) -> QString
        {
            if (bps >= 1024.0 * 1024.0)
                return QString("%1MB/s").arg(bps / (1024.0 * 1024.0), 0, 'f', 2);
            if (bps >= 1024.0)
                return QString("%1KB/s").arg(bps / 1024.0, 0, 'f', 1);
            return QString("%1B/s").arg(static_cast<uint64_t>(bps));
        };
        double dt_sec = std::max(0.05, static_cast<double>(speed_timer.restart()) / 1000.0);
        qint64 total_ms = elapsed.elapsed();
        qint64 secs = total_ms / 1000;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        lbl_elapsed->setText(QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));

        if (is_scanning)
        {
            uint64_t items_delta = (scan_files_found >= last_scan_files) ? (scan_files_found - last_scan_files) : 0;
            last_scan_files = scan_files_found;
            double inst_items_s = static_cast<double>(items_delta) / dt_sec;
            if (items_delta > 0)
                ewma_scan_speed = (ewma_scan_speed <= 0.0) ? inst_items_s : (0.4 * inst_items_s + 0.6 * ewma_scan_speed);
            lbl_proc->setText(QString("%1 discovered").arg(format_size(scan_bytes_found)));
            lbl_sped->setText(ewma_scan_speed >= 1.0 ? QString("%1 items/s").arg(commas(static_cast<uint64_t>(ewma_scan_speed))) : QString("Scanning..."));
            lbl_eta->setText("Scanning...");
            lbl_file_count->setText(QString("%1 discovered").arg(commas(scan_files_found)));
            return;
        }

        if (is_kdf)
        {
            if (bm)
            {
                if (local_job_pct < 5)
                    on_progress(local_job_pct + 1);
            }
            else
            {
                int kdf_cap = (m2_worker && m2_worker->what_mode() == worker::crypto_worker::mode::pack) ? 15 : 5;
                if (current_pct < kdf_cap)
                    on_progress(current_pct + 1);
            }
            if (tot_bytes > 0)
                lbl_proc->setText(format_size(current_proc) + " / " + format_size(tot_bytes));
            else if (scan_bytes_found > 0)
                lbl_proc->setText(format_size(0) + " / " + format_size(scan_bytes_found));
            else
                lbl_proc->setText("Deriving key...");

            lbl_sped->setText((bm && ewma_speed >= 1.0) ? format_speed(ewma_speed) : QString("Deriving key..."));
            lbl_eta->setText("Deriving key...");
            if (bm && m_jobs.size() > 1)
                lbl_file_count->setText(QString("Archive %1 of %2").arg(std::min(curr_job_idx + 1, static_cast<int>(m_jobs.size()))).arg(m_jobs.size()));
            else if (total_files_count > 0)
                lbl_file_count->setText(QString("0 / %1").arg(commas(total_files_count)));
            else if (scan_files_found > 0)
                lbl_file_count->setText(QString("0 / %1").arg(commas(scan_files_found)));
            else
                lbl_file_count->setText("1 of 1");
            return;
        }

        uint64_t bytes_delta = (current_proc >= last_processed) ? (current_proc - last_processed) : 0;
        last_processed = current_proc;
        double inst_speed = static_cast<double>(bytes_delta) / dt_sec;
        if (bytes_delta > 0)
            ewma_speed = (ewma_speed <= 0.0) ? inst_speed : (0.35 * inst_speed + 0.65 * ewma_speed);
        else if (ewma_speed > 0.0)
            ewma_speed *= 0.80;

        if (ewma_speed >= 1.0)
            lbl_sped->setText(format_speed(ewma_speed));
        else if (done_files_count > 0)
            lbl_sped->setText("Processing...");
        else
            lbl_sped->setText("-");

        if (tot_bytes > 0)
            lbl_proc->setText(format_size(current_proc) + " / " + format_size(tot_bytes));
        else if (current_proc > 0)
            lbl_proc->setText(format_size(current_proc));
        else
            lbl_proc->setText("0B / 0B");

        if (current_pct >= 100)
        {
            lbl_eta->setText("Quick, wasn't it?");
        }
        else
        {
            double frac = 0.0;
            qint64 ref_ms = 0;
            if (bm && m_jobs.size() > 1)
            {
                frac = overall_work_frac;
                ref_ms = total_ms;
            }
            else
            {
                frac = phase_work_frac;
                if (frac <= 0.0 && tot_bytes > 0)
                    frac = static_cast<double>(current_proc) / static_cast<double>(tot_bytes);
                ref_ms = phase_elapsed.isValid() ? phase_elapsed.elapsed() : total_ms;
            }
            if (frac > 0.002 && ref_ms >= 250)
            {
                double est_total_ms = static_cast<double>(ref_ms) / frac;
                double remaining_ms = std::max(0.0, est_total_ms - static_cast<double>(ref_ms));
                ewma_eta_ms = (ewma_eta_ms < 0.0) ? remaining_ms : (0.4 * remaining_ms + 0.6 * ewma_eta_ms);
                qint64 rem_secs = static_cast<qint64>(ewma_eta_ms / 1000.0);
                int eh = rem_secs / 3600;
                int em = (rem_secs % 3600) / 60;
                int es = rem_secs % 60;
                lbl_eta->setText(QString("%1:%2:%3").arg(eh, 2, 10, QChar('0')).arg(em, 2, 10, QChar('0')).arg(es, 2, 10, QChar('0')));
            }
            else
            {
                lbl_eta->setText("Calculating...");
            }
        }

        if (bm && m_jobs.size() > 1)
        {
            int cur_arch = std::min(curr_job_idx + 1, static_cast<int>(m_jobs.size()));
            if (total_files_count > 0)
                lbl_file_count->setText(QString("Archive %1 of %2 (%3 / %4)").arg(cur_arch).arg(m_jobs.size()).arg(commas(done_files_count)).arg(commas(total_files_count)));
            else
                lbl_file_count->setText(QString("Archive %1 of %2").arg(cur_arch).arg(m_jobs.size()));
        }
        else if (total_files_count > 0)
        {
            lbl_file_count->setText(QString("%1 / %2").arg(commas(done_files_count)).arg(commas(total_files_count)));
        }
        else
        {
            lbl_file_count->setText("1 of 1");
        }
    }
    void cd_prog_dialog::on_success()
    {
        if (bm)
        {
            if (curr_job_idx < m_jobs.size())
            {
                const auto &job = m_jobs[curr_job_idx];
                pk::crypto::am_i_evil::verification_report rep;
                if (m2_worker)
                    rep = m2_worker->what_report();
                job_res__.append({job.list_index, job.archive_path, job.display_name, true, "", false, rep});
                batch_done_weight += job.job_weight;
            }
            if (m2_worker)
            {
                m2_worker->wait();
                m2_worker = nullptr;
            }
            curr_job_idx++;
            start_next_job();
        }
        else
        {
            _success = true;
            on_progress(100);
            pk::ui::sfx::play_success();
            if (__in_bk)
            {
                restore_from_tray();
                QApplication::alert(parentWidget() ? parentWidget() : this);
            }
            cleanup_tray();
            accept();
        }
    }
    void cd_prog_dialog::on_error(const QString &msg)
    {
        if (bm)
        {
            bool is_cancel = msg.contains("cancelled", Qt::CaseInsensitive) || is_cancelled;
            if (curr_job_idx < m_jobs.size())
            {
                bool already = false;
                for (const auto &r : job_res__)
                {
                    if (r.list_index == m_jobs[curr_job_idx].list_index)
                    {
                        already = true;
                        break;
                    }
                }
                if (!already)
                {
                    const auto &job = m_jobs[curr_job_idx];
                    pk::crypto::am_i_evil::verification_report rep;
                    if (m2_worker)
                        rep = m2_worker->what_report();
                    job_res__.append({job.list_index, job.archive_path, job.display_name, false, msg, is_cancel, rep});
                }
                if (!is_cancel)
                    batch_done_weight += m_jobs[curr_job_idx].job_weight;
            }
            if (m2_worker)
            {
                m2_worker->wait();
                m2_worker = nullptr;
            }
            if (is_cancel || is_cancelled)
            {
                is_cancelled = true;
                current_err = msg;
                for (int i = curr_job_idx + 1; i < m_jobs.size(); ++i)
                {
                    bool already = false;
                    for (const auto &r : job_res__)
                    {
                        if (r.list_index == m_jobs[i].list_index)
                        {
                            already = true;
                            break;
                        }
                    }
                    if (!already)
                    {
                        job_res__.append({m_jobs[i].list_index, m_jobs[i].archive_path, m_jobs[i].display_name, false, "Operation cancelled by user.", true, {}});
                    }
                }
                reject();
                return;
            }
            curr_job_idx++;
            start_next_job();
        }
        else
        {
            _success = false;
            current_err = msg;
            bool is_cancel = is_cancelled || msg.contains("cancelled", Qt::CaseInsensitive);
            if (is_cancel)
                is_cancelled = true;
            else if (msg.isEmpty())
                pk::ui::sfx::play_error();
            if (__in_bk)
            {
                restore_from_tray();
                QApplication::alert(parentWidget() ? parentWidget() : this);
            }
            cleanup_tray();
            QDialog::reject();
        }
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
            warning(this, "Missing path", "Destination path for the keyfile cannot be empty.", "Browse to or enter a destination file path.");
            return;
        }
        try
        {
            pk::crypto::keyfile::generate(path_input->text().toStdString());
            gen_path_ = path_input->text();
            info(this, "Keyfile generated", "Keyfile successfully generated.");
            accept();
        }
        catch (const std::exception &e)
        {
            error(this, "Keyfile generation failed", QString("Failed to generate keyfile: ") + e.what(), "Check that the directory exists and you have write permissions.");
        }
    }
    void cd_keyfile::on_cancel()
    {
        reject();
    }
    cd_mk_archive::cd_mk_archive(QWidget *parent, const QString &ini_path)
        : QDialog(parent)
    {
        setWindowTitle("Create archive");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(500, 600);
        setAcceptDrops(true);
        setup_ui();
        dont_burn_my_eyes(this);
        if (!ini_path.isEmpty())
        {
            add_path(ini_path);
        }
    }
    void cd_mk_archive::add_path(const QString &path)
    {
        // normalize to canonical form so C:\foo, C:/foo, and C:/FOO (on windows)
        // all resolve to the same thing, preventing duplicates
        QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty())
            canonical = QDir::cleanPath(path);
        for (int i = 0; i < file_list->count(); ++i)
        {
            if (file_list->item(i)->text() == canonical)
                return;
        }
        file_list->addItem(canonical);
        update_default_path(canonical);
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
        QPushButton *btn_add_file = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add files", this);
        QPushButton *btn_add_dir = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add folders", this);
        QPushButton *btn_rem_sel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), " Remove", this);
        QPushButton *btn_clear = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm_all.svg")), " Clear all", this);
        file_btns->addWidget(btn_add_file);
        file_btns->addWidget(btn_add_dir);
        file_btns->addWidget(btn_rem_sel);
        file_btns->addWidget(btn_clear);
        files_layout->addLayout(file_btns);
        main_layout->addWidget(group_files);
        QTabWidget *tabs = new QTabWidget(this);
        QWidget *tab_gen = new QWidget();
        QFormLayout *form_gen = new QFormLayout(tab_gen);
        _include_hidden = new QCheckBox("Include hidden files / directories", this);
        _include_hidden->setChecked(pk::cfg::settings::instance().include_hidden());
        __cp_metadata = new QCheckBox("Copy properties (creation / modified dates, permissions)", this);
        __cp_metadata->setChecked(pk::cfg::settings::instance().cp_metadata());
        chunk_size_used = new QSpinBox(this);
        chunk_size_used->setRange(1, 64);
        chunk_size_used->setValue(pk::cfg::settings::instance().def_chunk_size());
        chunk_size_used->setSuffix(chunk_size_used->value() == 1 ? "MB" : "MBs");
        chunk_size_used->setContextMenuPolicy(Qt::NoContextMenu);
        connect(chunk_size_used, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val)
                { chunk_size_used->setSuffix(val == 1 ? "MB" : "MBs"); });
        form_gen->addRow(_include_hidden);
        form_gen->addRow(__cp_metadata);
        form_gen->addRow("Chunk size:", chunk_size_used);
        tabs->addTab(tab_gen, "General options");
        QWidget *tab_enc = new QWidget();
        QFormLayout *form_enc = new QFormLayout(tab_enc);
        algo_combo = new QComboBox(this);
        algo_combo->addItem("AES-256-GCM");
        algo_combo->addItem("XChaCha20-Poly1305");
        algo_combo->addItem("AES-256-SIV");
        algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cipher());
        form_enc->addRow("Cipher:", algo_combo);
        QLabel *aes_ni_label = new QLabel(this);
        if (pk::crypto::aes_ni_there())
        {
            aes_ni_label->setText("AES-NI: yeah");
            aes_ni_label->setStyleSheet("QLabel { color: #a0dca0; font-size: 11px; }");
            aes_ni_label->setToolTip("Your CPU supports AES-NI, making AES-256-GCM or AES-256-SIV\nextremely fast.");
        }
        else
        {
            aes_ni_label->setText("AES-NI: nah");
            aes_ni_label->setStyleSheet("QLabel { color: #e0d060; font-size: 11px; }");
            aes_ni_label->setToolTip("Your CPU does not support AES-NI. XChaCha20-Poly1305 is recommended over AES-256-GCM or AES-256-SIV\nfor better performance and security on this system.");
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
        s_mem_cost->setSuffix(s_mem_cost->value() == 1 ? "MB" : "MBs");
        s_mem_cost->setContextMenuPolicy(Qt::NoContextMenu);
        connect(s_mem_cost, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val)
                { s_mem_cost->setSuffix(val == 1 ? "MB" : "MBs"); });
        s_cores = new QSpinBox(this);
        s_cores->setRange(1, 64);
        s_cores->setValue(pk::cfg::settings::instance().def_cores());
        s_tc->setContextMenuPolicy(Qt::NoContextMenu);
        s_mem_cost->setContextMenuPolicy(Qt::NoContextMenu);
        s_cores->setContextMenuPolicy(Qt::NoContextMenu);
        form_kdf->addRow("Argon2id time cost:", s_tc);
        form_kdf->addRow("Argon2id memory cost:", s_mem_cost);
        form_kdf->addRow("Argon2id parallelism:", s_cores);
        tabs->addTab(tab_kdf, "KDF");
        QWidget *tab_comp = new QWidget();
        QFormLayout *form_comp = new QFormLayout(tab_comp);
        cmp_algo_combo = new QComboBox(this);
        cmp_algo_combo->addItem("ZSTD");
        cmp_algo_combo->addItem("LZMA2");
        cmp_algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_algorithm());
        form_comp->addRow("Algorithm:", cmp_algo_combo);
        cmp_preset_combo = new QComboBox(this);
        cmp_preset_combo->addItem("Store");
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
            set_enabled(cmp_use_raw, true);
            if (use_raw)
            {
                set_enabled(cmp_preset_combo, false);
                set_enabled(_cmp_raw_lvl, true);
                if (algo == 0)
                    _cmp_raw_lvl->setRange(0, 22);
                else if (algo == 1)
                    _cmp_raw_lvl->setRange(0, 9);
            }
            else
            {
                set_enabled(cmp_preset_combo, true);
                set_enabled(_cmp_raw_lvl, false);
            }
        };
        connect(cmp_algo_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update_comp_ui);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(cmp_use_raw, &QCheckBox::checkStateChanged, this, update_comp_ui);
#else
        connect(cmp_use_raw, &QCheckBox::stateChanged, this, update_comp_ui);
#endif
        update_comp_ui();
        tabs->addTab(tab_comp, "Compression");
        main_layout->addWidget(tabs);
        QGroupBox *group_pass = new QGroupBox("Archive authentication (password and / or keyfile)", this);
        QVBoxLayout *pass_layout = new QVBoxLayout(group_pass);
        password_v = new QLineEdit(this);
        password_v->setEchoMode(QLineEdit::Password);
        password_v->setPlaceholderText("Enter password (optional if using keyfile)...");
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
        pass_layout->addWidget(password_v);
        QHBoxLayout *keyfile_layout = new QHBoxLayout();
        keyfile_path_v = new QLineEdit(this);
        keyfile_path_v->setPlaceholderText("Select a keyfile (optional if using password)...");
        keyfile_path_v->setReadOnly(true);
        keyfile_path_v->setClearButtonEnabled(true);
        QPushButton *btn_browse_keyfile = new QPushButton("Browse", this);
        QPushButton *btn_generate_keyfile = new QPushButton("Generate", this);
        QPushButton *btn_clear_keyfile = new QPushButton("Clear", this);
        keyfile_layout->addWidget(keyfile_path_v);
        keyfile_layout->addWidget(btn_browse_keyfile);
        keyfile_layout->addWidget(btn_generate_keyfile);
        keyfile_layout->addWidget(btn_clear_keyfile);
        pass_layout->addLayout(keyfile_layout);
        main_layout->addWidget(group_pass);
        connect(btn_clear_keyfile, &QPushButton::clicked, this, [this]()
                { keyfile_path_v->clear(); });
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
        btn_create->setDefault(true);
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
            if (!path.isEmpty())
                add_path(path);
        }
    }
    void cd_mk_archive::on_browse_output()
    {
        QFileDialog dialog(this, "Select output directory");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dont_burn_my_eyes(&dialog);
        if (dialog.exec() == QDialog::Accepted)
        {
            QStringList dirs = dialog.selectedFiles();
            if (!dirs.isEmpty())
                output_dir->setText(dirs.first());
        }
    }
    void cd_mk_archive::on_add_files()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select files");
        for (const QString &f : files)
            add_path(f);
    }
    void cd_mk_archive::on_add_folders()
    {
        QFileDialog dialog(this, "Select folder(s)");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dont_burn_my_eyes(&dialog);
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
                if (!d.isEmpty())
                    add_path(d);
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
            warning(this, "Missing output path", "Archive output directory or name is empty.", "Specify both a destination folder and an archive name.");
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
            warning(this, "Invalid destination path", err_msg);
            return;
        }
        if (!QFileInfo::exists(resolved_out) || !QFileInfo(resolved_out).isDir())
        {
            warning(this, "Output directory not found", "Output directory does not exist: " + resolved_out, "Choose an existing directory for the archive.");
            return;
        }
        if (!pk::ui::outs::r_u_a_valid_filename(output_name->text(), &err_msg))
        { // putujmo Rahela!
            warning(this, "Invalid archive name", err_msg);
            return;
        }
        if (file_list->count() == 0)
        {
            warning(this, "No files selected", "Please add at least one file or folder to encrypt.");
            return;
        }
        if (password_v->text().isEmpty() && keyfile_path_v->text().isEmpty())
        {
            warning(this, "Missing credentials", "You must provide either a password or a keyfile.");
            return;
        }
        pk::crypto::kdf::kdf_cfg kdf_cfg{};
        kdf_cfg.time_cost = s_tc->value();
        kdf_cfg.memory_cost_kb = s_mem_cost->value() * 1024;
        kdf_cfg.parallelism = s_cores->value();
        kdf_cfg.hash_length = 32;
        pk::crypto::cipher::algorithm algo = static_cast<pk::crypto::cipher::algorithm>(algo_combo->currentIndex());
        worker::crypto_worker *worker = new worker::crypto_worker(worker::crypto_worker::mode::pack);
        std::vector<std::string> roots;
        for (int i = 0; i < file_list->count(); ++i)
        {
            roots.push_back(file_list->item(i)->text().toUtf8().toStdString());
        }
        uint8_t comp_algo = cmp_algo_combo->currentIndex();
        int8_t comp_level = 0;
        if (cmp_use_raw->isChecked())
        {
            comp_level = _cmp_raw_lvl->value();
        }
        else
        {
            int preset = cmp_preset_combo->currentIndex();
            if (preset == 0)
                comp_level = 0;
            else if (comp_algo == 0)
            {
                if (preset == 1)
                    comp_level = 3;
                else if (preset == 2)
                    comp_level = 9;
                else if (preset == 3)
                    comp_level = 19;
            }
            else if (comp_algo == 1)
            {
                if (preset == 1)
                    comp_level = 3;
                else if (preset == 2)
                    comp_level = 6;
                else if (preset == 3)
                    comp_level = 9;
            }
        }
        worker->ss_def_pk_params(
            roots,
            output_dir->text() + "/" + output_name->text(),
            password_v->text(),
            keyfile_path_v->text(),
            algo,
            kdf_cfg,
            _include_hidden->isChecked(),
            __cp_metadata->isChecked(),
            comp_algo,
            comp_level,
            chunk_size_used->value());
        QPointer<worker::crypto_worker> safe_worker(worker);
        cd_prog_dialog pd(worker, this);
        if (pd.exec() == QDialog::Accepted)
        {
            if (safe_worker)
                safe_worker->wait();
            info(this, "Archive created", "Archive successfully created.", QString(), false);
            accept();
        }
        else
        {
            if (safe_worker && !pd.was_force_killed())
                safe_worker->wait(500);
            if (!pd.was_cancelled() && !pd.what_err_msg().isEmpty() && !pd.what_err_msg().contains("cancelled", Qt::CaseInsensitive))
            {
                show_error(this, pd.what_err_msg());
            }
        }
    }
    cd_mk_archive::~cd_mk_archive()
    {
        if (password_v && !password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
    }
    void cd_mk_archive::on_cancel()
    {
        if (password_v && !password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
        reject();
    }
    void custom_entropy::wipe()
    {
        if (!password.isEmpty())
        {
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(password.data())), password.size() * sizeof(QChar));
            password.clear();
        }
        has_password = false;
        keyfile_path.clear();
        has_keyfile = false;
    }
    cd_gib_entropy::cd_gib_entropy(const QList<QPair<QString, custom_entropy>> &targets, QWidget *parent)
        : QDialog(parent)
    {
        setWindowFlags(windowFlags() | Qt::Window);
        dont_burn_my_eyes(this);
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        main_layout->setSpacing(8);
        main_layout->setContentsMargins(10, 10, 10, 10);
        if (targets.size() == 1)
        {
            QString fn = QFileInfo(targets[0].first).fileName();
            QString fn_elided = fontMetrics().elidedText(fn, Qt::ElideMiddle, 240);
            setWindowTitle("Set entropy - " + fn_elided);
            setFixedSize(480, 280);
            archive_tab_ui ui_ref;
            QWidget *page = create_page(targets[0].first, targets[0].second, ui_ref);
            m_tab_uis.append(ui_ref);
            main_layout->addWidget(page, 1);
        }
        else
        {
            setWindowTitle(QString("Set entropy - %1 archives selected").arg(targets.size()));
            setFixedSize(540, 380);
            m_tabs = new QTabWidget(this);
            m_tabs->setUsesScrollButtons(true);
            m_tabs->setElideMode(Qt::ElideMiddle);
            m_tabs->setStyleSheet(
                "QTabBar::tab { max-width: 150px; min-width: 60px; padding: 5px 10px; } "
                "QTabBar QToolButton { width: 0px; height: 0px; max-width: 0px; max-height: 0px; padding: 0px; margin: 0px; border: none; background: transparent; }");
            if (m_tabs->tabBar())
                m_tabs->tabBar()->installEventFilter(this);
            for (int i = 0; i < targets.size(); ++i)
            {
                QString fn = QFileInfo(targets[i].first).fileName();
                archive_tab_ui ui_ref;
                QWidget *page = create_page(targets[i].first, targets[i].second, ui_ref);
                m_tab_uis.append(ui_ref);
                QString tab_title = fontMetrics().elidedText(fn, Qt::ElideMiddle, 130);
                m_tabs->addTab(page, tab_title);
                m_tabs->setTabToolTip(i, QString("%1\n%2").arg(fn, targets[i].first));
            }
            main_layout->addWidget(m_tabs, 1);
            QPushButton *btn_apply_all = new QPushButton(
                QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " Apply current tab's entropy to all selected archives", this);
            btn_apply_all->setToolTip("Copy the password and keyfile settings from the active tab to all other tabs in this selection");
            connect(btn_apply_all, &QPushButton::clicked, this, &cd_gib_entropy::on_apply_to_all);
            main_layout->addWidget(btn_apply_all);
        }
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_save = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/ok.svg")), " Save", this);
        btn_save->setDefault(true);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Cancel", this);
        action_layout->addWidget(btn_save);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        connect(btn_save, &QPushButton::clicked, this, &cd_gib_entropy::on_save);
        connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    bool cd_gib_entropy::eventFilter(QObject *watched, QEvent *event)
    {
        if (m_tabs && m_tabs->tabBar() && watched == m_tabs->tabBar() && event->type() == QEvent::Wheel)
        {
            QWheelEvent *we = static_cast<QWheelEvent *>(event);
            int delta = we->angleDelta().y();
            if (delta == 0)
                delta = we->angleDelta().x();
            if (delta != 0)
            {
                int count = m_tabs->count();
                if (count > 1)
                {
                    int cur = m_tabs->currentIndex();
                    int next = cur + (delta < 0 ? 1 : -1);
                    next = qBound(0, next, count - 1);
                    if (next != cur)
                        m_tabs->setCurrentIndex(next);
                }
                return true;
            }
        }
        return QDialog::eventFilter(watched, event);
    }
    QWidget *cd_gib_entropy::create_page(const QString &file_path, const custom_entropy &entropy, archive_tab_ui &ui_ref)
    {
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setSpacing(6);
        layout->setContentsMargins(8, 8, 8, 8);
        ui_ref.file_path = file_path;
        QHBoxLayout *path_layout = new QHBoxLayout();
        QLabel *lbl_path_tag = new QLabel("Archive:", page);
        lbl_path_tag->setStyleSheet("font-weight: bold; font-size: 11px;");
        QLineEdit *txt_current_path = new QLineEdit(page);
        txt_current_path->setReadOnly(true);
        txt_current_path->setText(file_path);
        txt_current_path->setToolTip(file_path);
        txt_current_path->setContextMenuPolicy(Qt::NoContextMenu);
        QPushButton *btn_copy_path = new QPushButton("Copy", page);
        btn_copy_path->setToolTip("Copy full archive path to clipboard");
        connect(btn_copy_path, &QPushButton::clicked, page, [file_path]()
                { QApplication::clipboard()->setText(file_path); });
        path_layout->addWidget(lbl_path_tag);
        path_layout->addWidget(txt_current_path, 1);
        path_layout->addWidget(btn_copy_path);
        layout->addLayout(path_layout);
        QGroupBox *grp_pwd = new QGroupBox("Custom password", page);
        grp_pwd->setStyleSheet(
            "QGroupBox { border: none; font-weight: bold; font-size: 11px; margin-top: 2px; padding-top: 12px; } "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0px; color: #ffffff; }");
        QVBoxLayout *pwd_layout = new QVBoxLayout(grp_pwd);
        pwd_layout->setSpacing(4);
        pwd_layout->setContentsMargins(0, 4, 0, 0);
        ui_ref.chk_custom_pwd = new QCheckBox("Specify custom password for this archive", grp_pwd);
        ui_ref.chk_custom_pwd->setChecked(entropy.has_password);
        pwd_layout->addWidget(ui_ref.chk_custom_pwd);
        ui_ref.txt_pwd = new QLineEdit(grp_pwd);
        ui_ref.txt_pwd->setEchoMode(QLineEdit::Password);
        ui_ref.txt_pwd->setContextMenuPolicy(Qt::NoContextMenu);
        ui_ref.txt_pwd->setPlaceholderText("Enter custom archive password...");
        ui_ref.txt_pwd->setEnabled(entropy.has_password);
        if (entropy.has_password)
            ui_ref.txt_pwd->setText(entropy.password);
        QAction *warn_action = ui_ref.txt_pwd->addAction(
            QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/warn.svg")),
            QLineEdit::TrailingPosition);
        warn_action->setToolTip("CAPS lock is enabled, if you weren't aware.");
        warn_action->setVisible(false);
        QTimer *caps_timer = new QTimer(ui_ref.txt_pwd);
        connect(caps_timer, &QTimer::timeout, ui_ref.txt_pwd, [warn_action]()
                {
#ifdef _WIN32
                    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    warn_action->setVisible(caps);
#endif
                });
        caps_timer->start(100);
        QAction *toggle_action = ui_ref.txt_pwd->addAction(
            QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")),
            QLineEdit::TrailingPosition);
        QLineEdit *pwd_ptr = ui_ref.txt_pwd;
        connect(toggle_action, &QAction::triggered, pwd_ptr, [pwd_ptr, toggle_action]()
                {
            if (pwd_ptr->echoMode() == QLineEdit::Password) {
                pwd_ptr->setEchoMode(QLineEdit::Normal);
                toggle_action->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/hide_password.svg")));
            } else {
                pwd_ptr->setEchoMode(QLineEdit::Password);
                toggle_action->setIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/view_password.svg")));
            } });
        connect(ui_ref.chk_custom_pwd, &QCheckBox::toggled, ui_ref.txt_pwd, &QLineEdit::setEnabled);
        pwd_layout->addWidget(ui_ref.txt_pwd);
        layout->addWidget(grp_pwd);
        QGroupBox *grp_kf = new QGroupBox("Custom keyfile", page);
        grp_kf->setStyleSheet(
            "QGroupBox { border: none; border-top: 1px solid #383838; font-weight: bold; font-size: 11px; margin-top: 8px; padding-top: 12px; } "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; color: #ffffff; }");
        QVBoxLayout *kf_layout = new QVBoxLayout(grp_kf);
        kf_layout->setSpacing(4);
        kf_layout->setContentsMargins(0, 4, 0, 0);
        ui_ref.chk_custom_kf = new QCheckBox("Specify custom keyfile for this archive", grp_kf);
        ui_ref.chk_custom_kf->setChecked(entropy.has_keyfile);
        kf_layout->addWidget(ui_ref.chk_custom_kf);
        QHBoxLayout *kf_row = new QHBoxLayout();
        ui_ref.txt_kf = new QLineEdit(grp_kf);
        ui_ref.txt_kf->setReadOnly(true);
        ui_ref.txt_kf->setPlaceholderText("Select keyfile (optional)...");
        ui_ref.txt_kf->setEnabled(entropy.has_keyfile);
        if (entropy.has_keyfile)
            ui_ref.txt_kf->setText(entropy.keyfile_path);
        ui_ref.btn_browse_kf = new QPushButton("Browse", grp_kf);
        ui_ref.btn_clear_kf = new QPushButton("Clear", grp_kf);
        ui_ref.btn_browse_kf->setEnabled(entropy.has_keyfile);
        ui_ref.btn_clear_kf->setEnabled(entropy.has_keyfile);
        connect(ui_ref.chk_custom_kf, &QCheckBox::toggled, ui_ref.txt_kf, &QLineEdit::setEnabled);
        connect(ui_ref.chk_custom_kf, &QCheckBox::toggled, ui_ref.btn_browse_kf, &QPushButton::setEnabled);
        connect(ui_ref.chk_custom_kf, &QCheckBox::toggled, ui_ref.btn_clear_kf, &QPushButton::setEnabled);
        QLineEdit *kf_ptr = ui_ref.txt_kf;
        connect(ui_ref.btn_browse_kf, &QPushButton::clicked, page, [this, kf_ptr]()
                {
            QString f = QFileDialog::getOpenFileName(this, "Select custom keyfile", "", "MAGE keyfiles (*.mgkx);; All files (*.*)");
            if (!f.isEmpty())
                kf_ptr->setText(f); });
        connect(ui_ref.btn_clear_kf, &QPushButton::clicked, page, [kf_ptr]()
                { kf_ptr->clear(); });
        kf_row->addWidget(ui_ref.txt_kf);
        kf_row->addWidget(ui_ref.btn_browse_kf);
        kf_row->addWidget(ui_ref.btn_clear_kf);
        kf_layout->addLayout(kf_row);
        layout->addWidget(grp_kf);
        layout->addStretch();
        return page;
    }
    void cd_gib_entropy::on_apply_to_all()
    {
        if (!m_tabs || m_tab_uis.isEmpty())
            return;

        int cur = m_tabs->currentIndex();
        if (cur < 0 || cur >= m_tab_uis.size())
            return;
        const auto &src = m_tab_uis[cur];
        bool pwd_checked = src.chk_custom_pwd->isChecked();
        QString pwd_text = src.txt_pwd->text();
        bool kf_checked = src.chk_custom_kf->isChecked();
        QString kf_text = src.txt_kf->text();
        for (int i = 0; i < m_tab_uis.size(); ++i)
        {
            if (i == cur)
                continue;
            m_tab_uis[i].chk_custom_pwd->setChecked(pwd_checked);
            m_tab_uis[i].txt_pwd->setText(pwd_text);
            m_tab_uis[i].chk_custom_kf->setChecked(kf_checked);
            m_tab_uis[i].txt_kf->setText(kf_text);
        }
        pk::ui::sfx::play_info();
    }
    void cd_gib_entropy::on_save()
    {
        m_results.clear();
        for (const auto &ui : m_tab_uis)
        {
            custom_entropy ce;
            if (ui.chk_custom_pwd && ui.chk_custom_pwd->isChecked() && !ui.txt_pwd->text().isEmpty())
            {
                ce.has_password = true;
                ce.password = ui.txt_pwd->text();
            }
            if (ui.chk_custom_kf && ui.chk_custom_kf->isChecked() && !ui.txt_kf->text().isEmpty())
            {
                ce.has_keyfile = true;
                ce.keyfile_path = ui.txt_kf->text();
            }
            m_results.append(ce);
        }
        accept();
    }
    cd_gib_entropy::~cd_gib_entropy()
    {
        for (auto &ui : m_tab_uis)
        {
            if (ui.txt_pwd && !ui.txt_pwd->text().isEmpty())
            {
                QString s = ui.txt_pwd->text();
                pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
                ui.txt_pwd->clear();
            }
        }
        for (auto &r : m_results)
        {
            r.wipe();
        }
        m_results.clear();
    }
    cd_decrypt_archive::cd_decrypt_archive(QWidget *parent, const QString &ini_path)
        : QDialog(parent)
    {
        setWindowTitle("Decrypt archive");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(500, 520);
        setAcceptDrops(true);
        setup_ui();
        dont_burn_my_eyes(this);
        if (!ini_path.isEmpty())
            add_path(ini_path);
    }
    cd_decrypt_archive::~cd_decrypt_archive()
    {
        if (password_v && !password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
    }
    void cd_decrypt_archive::refresh_item_display(QListWidgetItem *item, const QString &path)
    {
        item->setToolTip(path);
        QString display = path;
        if (__custom_entropy_.contains(path) && __custom_entropy_[path].has_any())
        {
            display += "  " + __custom_entropy_[path].badge_tag();
        }
        item->setText(display);
    }
    void cd_decrypt_archive::add_path(const QString &path)
    {
        if (QFileInfo(path).isDir())
            return;
        QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty())
            canonical = QDir::cleanPath(path);
        for (int i = 0; i < archive_list->count(); ++i)
        {
            QString item_path = archive_list->item(i)->toolTip().isEmpty() ? archive_list->item(i)->text() : archive_list->item(i)->toolTip();
            if (item_path == canonical)
                return;
        }
        QListWidgetItem *item = new QListWidgetItem();
        archive_list->addItem(item);
        refresh_item_display(item, canonical);
        update_default_path(canonical);
    }
    void cd_decrypt_archive::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QGroupBox *group_archives = new QGroupBox("Archives to decrypt", this);
        QVBoxLayout *archives_layout = new QVBoxLayout(group_archives);
        archive_list = new QListWidget(this);
        archive_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        archive_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        archive_list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(archive_list, &QListWidget::customContextMenuRequested, this, &cd_decrypt_archive::on_ls_cm);
        QHBoxLayout *list_btn_layout = new QHBoxLayout();
        QPushButton *btn_add = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add files", this);
        QPushButton *btn_remove = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), " Remove", this);
        QPushButton *btn_clear = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm_all.svg")), " Clear all", this);
        list_btn_layout->addWidget(btn_add);
        list_btn_layout->addWidget(btn_remove);
        list_btn_layout->addWidget(btn_clear);
        list_btn_layout->addStretch();
        archives_layout->addWidget(archive_list);
        archives_layout->addLayout(list_btn_layout);
        main_layout->addWidget(group_archives);
        QFormLayout *form = new QFormLayout();
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
        password_v->setPlaceholderText("Enter password (optional if using keyfile)...");
        QHBoxLayout *keyfile_layout = new QHBoxLayout();
        keyfile_path_v = new QLineEdit(this);
        keyfile_path_v->setPlaceholderText("Select a keyfile (optional if using password)...");
        keyfile_path_v->setReadOnly(true);
        keyfile_path_v->setClearButtonEnabled(true);
        QPushButton *btn_browse_keyfile = new QPushButton("Browse", this);
        QPushButton *btn_clear_keyfile = new QPushButton("Clear", this);
        keyfile_layout->addWidget(keyfile_path_v);
        keyfile_layout->addWidget(btn_browse_keyfile);
        keyfile_layout->addWidget(btn_clear_keyfile);
        form->addRow("Password:", password_v);
        form->addRow("Keyfile:", keyfile_layout);
        connect(btn_clear_keyfile, &QPushButton::clicked, this, [this]()
                { keyfile_path_v->clear(); });
        connect(btn_browse_keyfile, &QPushButton::clicked, this, [this]()
                {
            QString path = QFileDialog::getOpenFileName(this, "Select keyfile", "", "MAGE keyfiles (*.mgkx) ;; All files (*)");
            if (!path.isEmpty()) keyfile_path_v->setText(path); });

        QString tooltips_path = QCoreApplication::applicationDirPath() + "/assets/txt_data/tooltips";
        QMap<QString, QString> tooltips = parse_tooltips(tooltips_path);
        ext_behavior = new QComboBox(this);
        ext_behavior->addItems({"Create archive folder", "Extract here (flat)", "Smart-ish extract"});
        ext_behavior->setCurrentIndex(pk::cfg::settings::instance().def_ext_behavior());
        if (tooltips.contains("extraction_behavior"))
            ext_behavior->setToolTip(tooltips["extraction_behavior"]);
        ext_overwrite = new QComboBox(this);
        ext_overwrite->addItems({"Ask", "Skip", "Overwrite"});
        ext_overwrite->setCurrentIndex(pk::cfg::settings::instance().def_ext_overwrite());
        if (tooltips.contains("overwrite_rules"))
            ext_overwrite->setToolTip(tooltips["overwrite_rules"]);
        ext_open = new QCheckBox("Auto-open output folder", this);
        ext_open->setChecked(pk::cfg::settings::instance().def_ext_open());
        if (tooltips.contains("auto_open_output"))
            ext_open->setToolTip(tooltips["auto_open_output"]);
        form->addRow("Behavior:", ext_behavior);
        form->addRow("Conflict rule:", ext_overwrite);
        form->addRow("", ext_open);
        main_layout->addLayout(form);
        main_layout->addStretch();
        QHBoxLayout *action_layout = new QHBoxLayout();
        action_layout->addStretch();
        QPushButton *btn_decrypt = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/decrypt.svg")), " Decrypt archives", this);
        btn_decrypt->setDefault(true);
        QPushButton *btn_cancel = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Cancel", this);
        action_layout->addWidget(btn_decrypt);
        action_layout->addWidget(btn_cancel);
        main_layout->addLayout(action_layout);
        connect(btn_add, &QPushButton::clicked, this, &cd_decrypt_archive::on_add_files);
        connect(btn_remove, &QPushButton::clicked, this, &cd_decrypt_archive::on_remove_files);
        connect(btn_clear, &QPushButton::clicked, this, &cd_decrypt_archive::on_clear_all);
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
                add_path(path);
        }
    }
    void cd_decrypt_archive::update_default_path(const QString &path)
    {
        QString current = output_dir_->text();
        if (current.isEmpty() || current == ".")
        {
            QFileInfo fi(path);
            output_dir_->setText(fi.absolutePath());
        }
    }
    void cd_decrypt_archive::on_add_files()
    {
        QStringList paths = QFileDialog::getOpenFileNames(this, "Select archives", "", "All files (*.*)");
        for (const QString &p : paths)
            add_path(p);
    }
    void cd_decrypt_archive::on_remove_files()
    {
        for (QListWidgetItem *item : archive_list->selectedItems())
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            if (__custom_entropy_.contains(p))
            {
                __custom_entropy_[p].wipe();
                __custom_entropy_.remove(p);
            }
            delete item;
        }
    }
    void cd_decrypt_archive::on_clear_all()
    {
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
        archive_list->clear();
    }
    void cd_decrypt_archive::on_ls_cm(const QPoint &pos)
    {
        QList<QListWidgetItem *> selected = archive_list->selectedItems();
        if (selected.isEmpty())
            return;
        QMenu menu(this);
        bool any_has_entropy = false;
        bool any_has_pwd = false;
        bool any_has_kf = false;
        for (QListWidgetItem *item : selected)
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            if (__custom_entropy_.contains(p))
            {
                const auto &ce = __custom_entropy_[p];
                if (ce.has_any())
                    any_has_entropy = true;
                if (ce.has_password)
                    any_has_pwd = true;
                if (ce.has_keyfile)
                    any_has_kf = true;
            }
        }
        QString action_text = any_has_entropy ? "View / edit custom entropy..." : "Source custom password / keyfile...";
        QAction *act_source = menu.addAction(
            QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/key.svg")),
            action_text);
        connect(act_source, &QAction::triggered, this, &cd_decrypt_archive::on_src_ce);
        if (any_has_pwd)
        {
            QAction *act_rm_pwd = menu.addAction("Remove custom password...");
            connect(act_rm_pwd, &QAction::triggered, this, &cd_decrypt_archive::on_rm_cp);
        }
        if (any_has_kf)
        {
            QAction *act_rm_kf = menu.addAction("Remove custom keyfile...");
            connect(act_rm_kf, &QAction::triggered, this, &cd_decrypt_archive::on_rm_ck);
        }
        if (any_has_entropy)
        {
            QAction *act_rm_all = menu.addAction("Remove all custom entropy...");
            connect(act_rm_all, &QAction::triggered, this, &cd_decrypt_archive::on_rm_all);
        }
        menu.addSeparator();
        QAction *act_remove = menu.addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), "Remove from list");
        connect(act_remove, &QAction::triggered, this, &cd_decrypt_archive::on_remove_files);
        menu.exec(archive_list->viewport()->mapToGlobal(pos));
    }
    void cd_decrypt_archive::on_src_ce()
    {
        QList<QListWidgetItem *> selected = archive_list->selectedItems();
        if (selected.isEmpty())
            return;
        QList<QPair<QString, custom_entropy>> targets;
        for (QListWidgetItem *item : selected)
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            custom_entropy ce;
            if (__custom_entropy_.contains(p))
                ce = __custom_entropy_[p];
            targets.append(qMakePair(p, ce));
        }
        cd_gib_entropy dlg(targets, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            QList<custom_entropy> results = dlg.get_results();
            for (int i = 0; i < targets.size() && i < results.size(); ++i)
            {
                QString p = targets[i].first;
                if (__custom_entropy_.contains(p))
                    __custom_entropy_[p].wipe();

                if (results[i].has_any())
                    __custom_entropy_[p] = results[i];
                else
                    __custom_entropy_.remove(p);
            }
            for (QListWidgetItem *item : selected)
            {
                QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
                refresh_item_display(item, p);
            }
        }
    }
    void cd_decrypt_archive::on_rm_cp()
    {
        for (QListWidgetItem *item : archive_list->selectedItems())
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            if (__custom_entropy_.contains(p))
            {
                auto &ce = __custom_entropy_[p];
                if (!ce.password.isEmpty())
                {
                    pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(ce.password.data())), ce.password.size() * sizeof(QChar));
                    ce.password.clear();
                }
                ce.has_password = false;
                if (!ce.has_any())
                    __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
    }
    void cd_decrypt_archive::on_rm_ck()
    {
        for (QListWidgetItem *item : archive_list->selectedItems())
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            if (__custom_entropy_.contains(p))
            {
                auto &ce = __custom_entropy_[p];
                ce.keyfile_path.clear();
                ce.has_keyfile = false;
                if (!ce.has_any())
                    __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
    }
    void cd_decrypt_archive::on_rm_all()
    {
        for (QListWidgetItem *item : archive_list->selectedItems())
        {
            QString p = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            if (__custom_entropy_.contains(p))
            {
                __custom_entropy_[p].wipe();
                __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
    }
    void cd_decrypt_archive::on_browse_output()
    {
        QFileDialog dialog(this, "Select output directory");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dont_burn_my_eyes(&dialog);
        if (dialog.exec() == QDialog::Accepted)
        {
            QStringList dirs = dialog.selectedFiles();
            if (!dirs.isEmpty())
                output_dir_->setText(dirs.first());
        }
    }
    void cd_decrypt_archive::on_decrypt()
    {
        if (archive_list->count() == 0)
        {
            warning(this, "Nothing to decrypt", "Add at least one archive to the list first.");
            return;
        }
        if (output_dir_->text().isEmpty())
        {
            warning(this, "No output directory", "Specify an output directory first.");
            return;
        }
        bool global_has_entropy = !password_v->text().isEmpty() || !keyfile_path_v->text().isEmpty();
        if (!global_has_entropy)
        {
            bool all_have_custom = true;
            for (int i = 0; i < archive_list->count(); ++i)
            {
                QString p = archive_list->item(i)->toolTip().isEmpty() ? archive_list->item(i)->text() : archive_list->item(i)->toolTip();
                if (!__custom_entropy_.contains(p) || !__custom_entropy_[p].has_any())
                {
                    all_have_custom = false;
                    break;
                }
            }
            if (!all_have_custom)
            {
                warning(this, "Missing credentials", "Provide a password or keyfile (globally or per archive).");
                return;
            }
        }
        QString base_out = output_dir_->text();
        if (!QFileInfo::exists(base_out) || !QFileInfo(base_out).isDir())
        {
            warning(this, "Output directory missing", "The output directory does not exist:\n" + base_out);
            return;
        }
        struct result_entry
        {
            int list_index;
            QString archive_path;
            QString display_name;
        };
        QList<result_entry> succeeded_entries;
        QStringList failed_names;
        QStringList failed_paths;
        QStringList failed_reasons;
        int cancelled_count = 0;
        int actual_error_count = 0;
        QList<cd_prog_dialog::decrypt_job> jobs;
        for (int i = 0; i < archive_list->count(); ++i)
        {
            QString archive_path = archive_list->item(i)->toolTip().isEmpty()
                                       ? archive_list->item(i)->text()
                                       : archive_list->item(i)->toolTip();
            QString display_name = QFileInfo(archive_path).fileName();
            QString eff_pwd = password_v->text();
            QString eff_kf = keyfile_path_v->text();
            if (__custom_entropy_.contains(archive_path))
            {
                const auto &ce = __custom_entropy_[archive_path];
                if (ce.has_password)
                    eff_pwd = ce.password;
                if (ce.has_keyfile)
                    eff_kf = ce.keyfile_path;
            }
            if (eff_pwd.isEmpty() && eff_kf.isEmpty())
            {
                actual_error_count++;
                failed_names.append(display_name);
                failed_paths.append(archive_path);
                failed_reasons.append("Missing credentials; no password or keyfile set for this archive.");
                continue;
            }
            jobs.append({i, archive_path, display_name, eff_pwd, eff_kf, base_out, ext_behavior->currentIndex(), ext_overwrite->currentIndex()});
        }
        if (!jobs.isEmpty())
        {
            cd_prog_dialog pd(jobs, this);
            pd.exec();
            for (const auto &r : pd.results())
            {
                if (r.success)
                {
                    succeeded_entries.append({r.list_index, r.archive_path, r.display_name});
                }
                else
                {
                    if (r.cancelled)
                        cancelled_count++;
                    else
                        actual_error_count++;
                    failed_names.append(r.display_name);
                    failed_paths.append(r.archive_path);
                    failed_reasons.append(r.cancelled ? "Decryption cancelled by user." : r.error_msg);
                }
            }
        }
        if (actual_error_count == 0 && cancelled_count > 0 && succeeded_entries.isEmpty())
        {
            return;
        }
        QString summary;
        if (!succeeded_entries.isEmpty())
        {
            summary += QString("Decrypted successfully (%1 archive%2):\n\n").arg(succeeded_entries.size()).arg(succeeded_entries.size() == 1 ? "" : "s");
            for (const auto &e : succeeded_entries)
            {
                summary += QString("%1\n%2\n\n").arg(e.display_name).arg(e.archive_path);
            }
        }
        if (!failed_names.isEmpty())
        {
            QString hdr = (actual_error_count == 0) ? "Cancelled" : "Failed";
            summary += QString("%1 (%2 archive%3):\n\n").arg(hdr).arg(failed_names.size()).arg(failed_names.size() == 1 ? "" : "s");
            for (int i = 0; i < failed_names.size(); ++i)
            {
                QString raw_err = failed_reasons[i];
                QString reason;
                if (raw_err.contains("cancelled", Qt::CaseInsensitive))
                {
                    reason = "Decryption cancelled by user.";
                }
                else if (raw_err.contains("Missing credentials", Qt::CaseInsensitive))
                {
                    reason = "Missing password or keyfile.";
                }
                else
                {
                    auto parsed = pk::error::parse(raw_err.toStdString());
                    if (parsed.title == "Authentication failed" ||
                        raw_err.contains("MAC verification failed", Qt::CaseInsensitive) ||
                        raw_err.contains("wrong password", Qt::CaseInsensitive) ||
                        raw_err.contains("authentication tag", Qt::CaseInsensitive))
                    {
                        reason = "Authentication failed (wrong password or corrupted data).";
                    }
                    else if (!parsed.title.empty() && parsed.title != "Operation failed" && parsed.title != "Keyfile error")
                    {
                        reason = QString::fromStdString(parsed.title);
                    }
                    else
                    {
                        reason = QString::fromStdString(parsed.title.empty() ? "Decryption failed" : parsed.title);
                    }
                }
                summary += QString("%1\n%2\nReason: %3\n\n").arg(failed_names[i]).arg(failed_paths[i]).arg(reason);
            }
        }
        if (failed_names.isEmpty())
            info(this, "Decryption completed", summary.trimmed(), QString(), false);
        else if (actual_error_count == 0)
            info(this, "Decryption cancelled", summary.trimmed());
        else if (succeeded_entries.isEmpty())
            error(this, "Decryption failed", summary.trimmed());
        else
            warning(this, "Decryption completed with errors", summary.trimmed());
        if (!succeeded_entries.isEmpty() && ext_open->isChecked())
            QDesktopServices::openUrl(QUrl::fromLocalFile(base_out));
        for (int k = succeeded_entries.size() - 1; k >= 0; --k)
        {
            const auto &e = succeeded_entries[k];
            if (__custom_entropy_.contains(e.archive_path))
            {
                __custom_entropy_[e.archive_path].wipe();
                __custom_entropy_.remove(e.archive_path);
            }
            delete archive_list->takeItem(e.list_index);
        }
        if (failed_names.isEmpty())
        {
            if (!password_v->text().isEmpty())
            {
                QString s = password_v->text();
                pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
                password_v->clear();
            }
            for (auto &ce : __custom_entropy_)
                ce.wipe();
            __custom_entropy_.clear();
            accept();
        }
    }
    void cd_decrypt_archive::on_cancel()
    {
        if (!password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
        reject();
    }
    cd_am_i_evil::cd_am_i_evil(QWidget *parent, const QString &ini_path)
        : QDialog(parent)
    {
        setWindowTitle("Verify archive");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(920, 500);
        setAcceptDrops(true);
        setup_ui();
        dont_burn_my_eyes(this);
        if (!ini_path.isEmpty())
            add_path(ini_path);
        QTimer::singleShot(0, this, [this]()
                           {
            adjust_qc();
            adjust_pc(); });
    }
    cd_am_i_evil::~cd_am_i_evil()
    {
        if (password_v && !password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
    }
    void cd_am_i_evil::refresh_item_display(QTreeWidgetItem *item, const QString &path)
    {
        item->setToolTip(0, path);
        QString fn = QFileInfo(path).fileName();
        if (__custom_entropy_.contains(path) && __custom_entropy_[path].has_any())
        {
            QString badge = __custom_entropy_[path].badge_tag();
            item->setText(0, QString("%1  %2").arg(fn, badge));
        }
        else
        {
            item->setText(0, fn);
        }
    }
    void cd_am_i_evil::add_path(const QString &path)
    {
        add_paths(QStringList{path});
    }
    void cd_am_i_evil::add_paths(const QStringList &paths)
    {
        if (paths.isEmpty())
            return;
        queue_tree->setUpdatesEnabled(false);
        std::unordered_set<std::string> existing_paths;
        existing_paths.reserve(m_reports.size() + paths.size());
        for (const auto &rep : m_reports)
        {
            existing_paths.insert(rep.header.___filepath.string());
        }
        QList<QTreeWidgetItem *> new_items;
        new_items.reserve(paths.size());
        bool was_empty = m_reports.empty();
        QTreeWidgetItem *first_new_item = nullptr;
        for (const QString &path : paths)
        {
            if (path.isEmpty() || QFileInfo(path).isDir())
                continue;
            QString canonical = QFileInfo(path).canonicalFilePath();
            if (canonical.isEmpty())
                canonical = QDir::cleanPath(path);
            std::string canon_std = canonical.toStdString();
            if (existing_paths.find(canon_std) != existing_paths.end())
                continue;
            existing_paths.insert(canon_std);
            pk::crypto::am_i_evil::verification_report rep;
            rep.header = pk::crypto::am_i_evil::view_header(canon_std);
            rep.total_chunks = rep.header.est_chunks;
            rep.verdict = pk::crypto::am_i_evil::__vv_::io_error;
            m_reports.push_back(rep);
            QTreeWidgetItem *item = new QTreeWidgetItem();
            refresh_item_display(item, canonical);
            if (!rep.header.file_exists)
            {
                item->setText(1, "INVALID");
                item->setToolTip(1, QString::fromStdString(rep.header.sanity_notes));
                item->setForeground(1, QColor(0xff, 0x66, 0x66));
            }
            else if (!rep.header.valid_magic || !rep.header.structure_ok || !rep.header.kdf_safe)
            {
                item->setText(1, "INVALID");
                item->setToolTip(1, QString::fromStdString(rep.header.sanity_notes));
                item->setForeground(1, QColor(0xff, 0x66, 0x66));
            }
            else
            {
                item->setText(1, "OK");
                item->setToolTip(1, "Container header and parameters valid");
                item->setForeground(1, QColor(0x55, 0xff, 0x55));
            }
            item->setText(2, "PENDING");
            item->setToolTip(2, "Awaiting credentials to verify AEAD integrity");
            item->setForeground(2, QColor(0xaa, 0xaa, 0xaa));
            item->setTextAlignment(1, Qt::AlignCenter);
            item->setTextAlignment(2, Qt::AlignCenter);
            new_items.append(item);
            if (!first_new_item)
                first_new_item = item;
        }
        if (!new_items.isEmpty())
        {
            queue_tree->addTopLevelItems(new_items);
            if (was_empty && first_new_item)
            {
                queue_tree->setCurrentItem(first_new_item);
            }
        }
        queue_tree->setUpdatesEnabled(true);
        update_qs();
        refresh_properties();
        adjust_qc();
    }
    void cd_am_i_evil::view_archive_index(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_reports.size()))
            return;
        m_reports[index].header = pk::crypto::am_i_evil::view_header(m_reports[index].header.___filepath);
        m_reports[index].total_chunks = m_reports[index].header.est_chunks;
    }
    void cd_am_i_evil::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    void cd_am_i_evil::dropEvent(QDropEvent *event)
    {
        QStringList paths;
        for (const QUrl &url : event->mimeData()->urls())
        {
            QString path = url.toLocalFile();
            if (!path.isEmpty())
                paths.append(path);
        }
        if (!paths.isEmpty())
            add_paths(paths);
    }
    void cd_am_i_evil::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        main_layout->setSpacing(6);
        main_layout->setContentsMargins(8, 8, 8, 8);
        QHBoxLayout *columns_layout = new QHBoxLayout();
        columns_layout->setSpacing(8);
        QVBoxLayout *left_layout = new QVBoxLayout();
        left_layout->setSpacing(6);
        QGroupBox *group_queue = new QGroupBox("Archive queue", this);
        group_queue->setStyleSheet("QGroupBox { border: none; font-weight: bold; font-size: 12px; margin-top: 2px; padding-top: 14px; } "
                                   "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0px; color: #ffffff; }");
        QVBoxLayout *queue_layout = new QVBoxLayout(group_queue);
        queue_layout->setSpacing(4);
        queue_layout->setContentsMargins(0, 4, 0, 0);
        queue_tree = new QTreeWidget(this);
        queue_tree->setHeaderLabels(QStringList{"Archive", "Header check", "AEAD integrity"});
        queue_tree->setRootIsDecorated(false); // qt?
        queue_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        queue_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(queue_tree, &QTreeWidget::customContextMenuRequested, this, &cd_am_i_evil::on_tree_cm);
        queue_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        queue_tree->setTextElideMode(Qt::ElideNone);
        queue_tree->header()->setStretchLastSection(false);
        queue_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
        queue_tree->header()->setMinimumSectionSize(60);
        queue_tree->headerItem()->setTextAlignment(1, Qt::AlignCenter);
        queue_tree->headerItem()->setTextAlignment(2, Qt::AlignCenter);
        queue_layout->addWidget(queue_tree, 1);
        QHBoxLayout *queue_btn_layout = new QHBoxLayout();
        QPushButton *btn_add = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/add.svg")), " Add...", this);
        QPushButton *btn_remove = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), " Remove", this);
        QPushButton *btn_clear = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm_all.svg")), " Clear all", this);
        queue_btn_layout->addWidget(btn_add);
        queue_btn_layout->addWidget(btn_remove);
        queue_btn_layout->addWidget(btn_clear);
        queue_btn_layout->addStretch();
        queue_layout->addLayout(queue_btn_layout);
        lbl_queue_summary = new QLabel("Queue: 0 archives | Headers: 0 valid, 0 warnings | AEAD: 0 verified, 0 failed, 0 pending", this);
        lbl_queue_summary->setStyleSheet("font-size: 10px; color: #a0a0a0;");
        queue_layout->addWidget(lbl_queue_summary);
        left_layout->addWidget(group_queue, 1);
        QGroupBox *group_auth = new QGroupBox("AEAD integrity verification", this);
        group_auth->setStyleSheet(
            "QGroupBox { border: none; border-top: 1px solid #383838; font-weight: bold; font-size: 12px; margin-top: 8px; padding-top: 14px; } "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; color: #ffffff; }");
        QVBoxLayout *auth_box_layout = new QVBoxLayout(group_auth);
        auth_box_layout->setSpacing(4);
        auth_box_layout->setContentsMargins(0, 4, 0, 0);
        QLabel *lbl_auth_note = new QLabel("To verify AEAD integrity; the archive entropy source must be\nprovided, without it only header checks can be done.", this);
        lbl_auth_note->setWordWrap(true);
        lbl_auth_note->setStyleSheet("color: #a0a0a0; font-size: 10px;");
        auth_box_layout->addWidget(lbl_auth_note);
        QFormLayout *form_auth = new QFormLayout();
        form_auth->setContentsMargins(0, 2, 0, 2);
        form_auth->setSpacing(4);
        password_v = new QLineEdit(this);
        password_v->setEchoMode(QLineEdit::Password);
        password_v->setContextMenuPolicy(Qt::NoContextMenu);
        password_v->setPlaceholderText("Password (optional if only inspecting)...");
        QAction *warn_action = password_v->addAction(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/warn.svg")), QLineEdit::TrailingPosition);
        warn_action->setToolTip("CAPS lock is enabled, if you weren't aware.");
        warn_action->setVisible(false);
        QTimer *caps_timer = new QTimer(password_v);
        connect(caps_timer, &QTimer::timeout, password_v, [warn_action]()
                {
#ifdef _WIN32
                    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    warn_action->setVisible(caps);
#endif
                });
        caps_timer->start(100);
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
        QHBoxLayout *keyfile_layout = new QHBoxLayout();
        keyfile_path_v = new QLineEdit(this);
        keyfile_path_v->setPlaceholderText("Select keyfile (optional)...");
        keyfile_path_v->setReadOnly(true);
        keyfile_path_v->setClearButtonEnabled(true);
        QPushButton *btn_browse_keyfile = new QPushButton("Browse", this);
        QPushButton *btn_clear_keyfile = new QPushButton("Clear", this);
        keyfile_layout->addWidget(keyfile_path_v);
        keyfile_layout->addWidget(btn_browse_keyfile);
        keyfile_layout->addWidget(btn_clear_keyfile);
        form_auth->addRow("Password:", password_v);
        form_auth->addRow("Keyfile:", keyfile_layout);
        auth_box_layout->addLayout(form_auth);
        left_layout->addWidget(group_auth);
        columns_layout->addLayout(left_layout, 10);
        QVBoxLayout *right_layout = new QVBoxLayout();
        right_layout->setSpacing(6);
        QLabel *lbl_notice = new QLabel("Notice: diagnostic verification discloses container geometry and chunk layouts. "
                                        "Do not share logs or detailed error output with untrusted parties.",
                                        this);
        lbl_notice->setWordWrap(true);
        lbl_notice->setStyleSheet("color: #e5c07b; font-size: 10px; padding: 4px 6px; "
                                  "background-color: rgba(229, 192, 123, 0.08); "
                                  "border: 1px solid rgba(229, 192, 123, 0.25); border-radius: 4px;");
        right_layout->addWidget(lbl_notice);
        QHBoxLayout *path_layout = new QHBoxLayout();
        QLabel *lbl_path_tag = new QLabel("Selected:", this);
        lbl_path_tag->setStyleSheet("font-weight: bold; font-size: 11px;");
        txt_current_path = new QLineEdit(this);
        txt_current_path->setReadOnly(true);
        txt_current_path->setPlaceholderText("No archive selected");
        btn_copy_path = new QPushButton("Copy", this);
        btn_copy_path->setToolTip("Copy full archive path to clipboard");
        path_layout->addWidget(lbl_path_tag);
        path_layout->addWidget(txt_current_path, 1);
        path_layout->addWidget(btn_copy_path);
        right_layout->addLayout(path_layout);
        QGroupBox *group_props = new QGroupBox("Container properties", this);
        group_props->setStyleSheet("QGroupBox { border: none; font-weight: bold; font-size: 12px; margin-top: 2px; padding-top: 14px; } "
                                   "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0px; color: #ffffff; }");
        QVBoxLayout *props_layout = new QVBoxLayout(group_props);
        props_layout->setContentsMargins(0, 4, 0, 0);
        pt = new QTreeWidget(this);
        pt->setHeaderLabels(QStringList{"Property", "Value"});
        pt->setRootIsDecorated(true);
        pt->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        pt->setTextElideMode(Qt::ElideNone);
        pt->header()->setStretchLastSection(false);
        pt->header()->setSectionResizeMode(QHeaderView::Interactive);
        pt->header()->setMinimumSectionSize(60);
        props_layout->addWidget(pt);
        right_layout->addWidget(group_props, 1);
        columns_layout->addLayout(right_layout, 11);
        main_layout->addLayout(columns_layout, 1);
        QHBoxLayout *action_layout = new QHBoxLayout();
        QPushButton *btn_export = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/save.svg")), " Export log (.log)", this);
        action_layout->addWidget(btn_export);
        action_layout->addStretch();
        QPushButton *btn_verify = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/verify.svg")), " Verify", this);
        btn_verify->setDefault(true);
        QPushButton *btn_close = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), " Close", this);
        action_layout->addWidget(btn_verify);
        action_layout->addWidget(btn_close);
        main_layout->addLayout(action_layout);
        connect(btn_add, &QPushButton::clicked, this, &cd_am_i_evil::on_add_files);
        connect(btn_remove, &QPushButton::clicked, this, &cd_am_i_evil::on_remove_files);
        connect(btn_clear, &QPushButton::clicked, this, &cd_am_i_evil::on_clear_all);
        connect(queue_tree, &QTreeWidget::itemSelectionChanged, this, &cd_am_i_evil::on_selection_changed);
        connect(btn_copy_path, &QPushButton::clicked, this, &cd_am_i_evil::on_copy_path);
        connect(btn_browse_keyfile, &QPushButton::clicked, this, [this]()
                {
            QString f = QFileDialog::getOpenFileName(this, "Select keyfile", "", "All files (*.*)");
            if (!f.isEmpty())
                keyfile_path_v->setText(f); });
        connect(btn_clear_keyfile, &QPushButton::clicked, this, [this]()
                { keyfile_path_v->clear(); });
        connect(btn_export, &QPushButton::clicked, this, &cd_am_i_evil::on_export_log);
        connect(btn_verify, &QPushButton::clicked, this, &cd_am_i_evil::on_verify_integrity);
        connect(btn_close, &QPushButton::clicked, this, &cd_am_i_evil::on_close);
    }
    void cd_am_i_evil::on_add_files()
    {
        QStringList paths = QFileDialog::getOpenFileNames(this, "Select archives to verify", "", "MAGE archives (*.mage);; All files (*.*)");
        if (!paths.isEmpty())
            add_paths(paths);
    }
    void cd_am_i_evil::on_remove_files()
    {
        QList<QTreeWidgetItem *> selected = queue_tree->selectedItems();
        if (selected.isEmpty() && queue_tree->currentItem())
            selected.append(queue_tree->currentItem());

        if (selected.isEmpty())
            return;

        QList<int> rows;
        for (QTreeWidgetItem *item : selected)
        {
            int r = queue_tree->indexOfTopLevelItem(item);
            if (r >= 0 && !rows.contains(r))
                rows.append(r);
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());

        for (int r : rows)
        {
            if (r >= 0 && r < static_cast<int>(m_reports.size()))
            {
                QString p = QString::fromStdString(m_reports[r].header.___filepath.string());
                if (__custom_entropy_.contains(p))
                {
                    __custom_entropy_[p].wipe();
                    __custom_entropy_.remove(p);
                }
                m_reports.erase(m_reports.begin() + r);
                delete queue_tree->takeTopLevelItem(r);
            }
        }
        update_qs();
        refresh_properties();
        adjust_qc();
    }
    void cd_am_i_evil::on_clear_all()
    {
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
        m_reports.clear();
        queue_tree->clear();
        txt_current_path->clear();
        pt->clear();
        update_qs();
        adjust_qc();
    }
    void cd_am_i_evil::on_tree_cm(const QPoint &pos)
    {
        QList<QTreeWidgetItem *> selected = queue_tree->selectedItems();
        if (selected.isEmpty())
            return;

        QMenu menu(this);
        bool any_has_entropy = false;
        bool any_has_pwd = false;
        bool any_has_kf = false;
        for (QTreeWidgetItem *item : selected)
        {
            QString p = item->toolTip(0);
            if (__custom_entropy_.contains(p))
            {
                const auto &ce = __custom_entropy_[p];
                if (ce.has_any())
                    any_has_entropy = true;
                if (ce.has_password)
                    any_has_pwd = true;
                if (ce.has_keyfile)
                    any_has_kf = true;
            }
        }
        QString action_text = any_has_entropy ? "View / edit custom entropy..." : "Source custom password / keyfile...";
        QAction *act_source = menu.addAction(
            QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/key.svg")),
            action_text);
        connect(act_source, &QAction::triggered, this, &cd_am_i_evil::on_src_ce);
        if (any_has_pwd)
        {
            QAction *act_rm_pwd = menu.addAction("Remove custom password");
            connect(act_rm_pwd, &QAction::triggered, this, &cd_am_i_evil::on_rm_cp);
        }
        if (any_has_kf)
        {
            QAction *act_rm_kf = menu.addAction("Remove custom keyfile");
            connect(act_rm_kf, &QAction::triggered, this, &cd_am_i_evil::on_rm_ck);
        }
        if (any_has_entropy)
        {
            QAction *act_rm_all = menu.addAction("Remove all custom entropy");
            connect(act_rm_all, &QAction::triggered, this, &cd_am_i_evil::on_rm_all);
        }
        menu.addSeparator();
        QAction *act_remove = menu.addAction(
            QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm.svg")), "Remove from list");
        connect(act_remove, &QAction::triggered, this, &cd_am_i_evil::on_remove_files);
        menu.exec(queue_tree->viewport()->mapToGlobal(pos));
    }
    void cd_am_i_evil::on_src_ce()
    {
        QList<QTreeWidgetItem *> selected = queue_tree->selectedItems();
        if (selected.isEmpty())
            return;
        QList<QPair<QString, custom_entropy>> targets;
        for (QTreeWidgetItem *item : selected)
        {
            QString p = item->toolTip(0);
            custom_entropy ce;
            if (__custom_entropy_.contains(p))
                ce = __custom_entropy_[p];
            targets.append(qMakePair(p, ce));
        }
        cd_gib_entropy dlg(targets, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            QList<custom_entropy> results = dlg.get_results();
            for (int i = 0; i < targets.size() && i < results.size(); ++i)
            {
                QString p = targets[i].first;
                if (__custom_entropy_.contains(p))
                    __custom_entropy_[p].wipe();

                if (results[i].has_any())
                    __custom_entropy_[p] = results[i];
                else
                    __custom_entropy_.remove(p);
            }
            for (QTreeWidgetItem *item : selected)
            {
                QString p = item->toolTip(0);
                refresh_item_display(item, p);
            }
            refresh_properties();
            adjust_qc();
        }
    }
    void cd_am_i_evil::on_rm_cp()
    {
        for (QTreeWidgetItem *item : queue_tree->selectedItems())
        {
            QString p = item->toolTip(0);
            if (__custom_entropy_.contains(p))
            {
                auto &ce = __custom_entropy_[p];
                if (!ce.password.isEmpty())
                {
                    pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(ce.password.data())), ce.password.size() * sizeof(QChar));
                    ce.password.clear();
                }
                ce.has_password = false;
                if (!ce.has_any())
                    __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
        refresh_properties();
        adjust_qc();
    }
    void cd_am_i_evil::on_rm_ck()
    {
        for (QTreeWidgetItem *item : queue_tree->selectedItems())
        {
            QString p = item->toolTip(0);
            if (__custom_entropy_.contains(p))
            {
                auto &ce = __custom_entropy_[p];
                ce.keyfile_path.clear();
                ce.has_keyfile = false;
                if (!ce.has_any())
                    __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
        refresh_properties();
        adjust_qc();
    }
    void cd_am_i_evil::on_rm_all()
    {
        for (QTreeWidgetItem *item : queue_tree->selectedItems())
        {
            QString p = item->toolTip(0);
            if (__custom_entropy_.contains(p))
            {
                __custom_entropy_[p].wipe();
                __custom_entropy_.remove(p);
                refresh_item_display(item, p);
            }
        }
        refresh_properties();
        adjust_qc();
    }
    void cd_am_i_evil::on_selection_changed()
    {
        refresh_properties();
    }
    void cd_am_i_evil::on_copy_path()
    {
        QString path = txt_current_path->text();
        if (!path.isEmpty())
        {
            QApplication::clipboard()->setText(path);
        }
    }
    void cd_am_i_evil::update_qs()
    {
        int total = static_cast<int>(m_reports.size());
        int headers_valid = 0;
        int headers_warnings = 0;
        int aead_verified = 0;
        int aead_failed = 0;
        int aead_pending = 0;
        for (const auto &rep : m_reports)
        {
            if (rep.header.file_exists && rep.header.valid_magic && rep.header.structure_ok && rep.header.kdf_safe)
                ++headers_valid;
            else
                ++headers_warnings;

            if (rep.verdict == pk::crypto::am_i_evil::__vv_::success)
                ++aead_verified;
            else if (rep.verdict == pk::crypto::am_i_evil::__vv_::io_error && rep.verified_chunks == 0 && rep.error_details.empty())
                ++aead_pending;
            else
                ++aead_failed;
        }
        lbl_queue_summary->setText(
            QString("Queue: %1 archive(s) | Headers: %2 valid, %3 warning(s) | AEAD: %4 verified, %5 failed, %6 pending").arg(total).arg(headers_valid).arg(headers_warnings).arg(aead_verified).arg(aead_failed).arg(aead_pending));
    }
    void cd_am_i_evil::refresh_properties()
    {
        int row = queue_tree->indexOfTopLevelItem(queue_tree->currentItem());
        if (row < 0 || row >= static_cast<int>(m_reports.size()))
        {
            txt_current_path->clear();
            pt->clear();
            return;
        }
        const auto &rep = m_reports[row];
        txt_current_path->setText(QString::fromStdString(rep.header.___filepath.string()));
        pt->clear();
        auto add_category = [this](const QString &title) -> QTreeWidgetItem *
        {
            QTreeWidgetItem *cat = new QTreeWidgetItem(pt);
            cat->setText(0, title);
            cat->setFirstColumnSpanned(true);
            QFont f = cat->font(0);
            f.setBold(true);
            cat->setFont(0, f);
            cat->setExpanded(true);
            return cat;
        };
        auto add_prop = [](QTreeWidgetItem *parent, const QString &name, const QString &val, const QColor &color = QColor(), const QString &tooltip = QString())
        {
            QTreeWidgetItem *item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setText(1, val);
            if (color.isValid())
                item->setForeground(1, color);
            if (!tooltip.isEmpty())
            {
                item->setToolTip(0, tooltip);
                item->setToolTip(1, tooltip);
            }
            return item;
        };
        QTreeWidgetItem *cat_geom = add_category("Container geometry");
        add_prop(cat_geom, "On disk file size", QString("%1 (%2)").arg(QString::fromStdString(pk::crypto::am_i_evil::numbers_with_commas_unlike_in_gta_5(rep.header.file_size_bytes) + "B")).arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.header.file_size_bytes))));
        add_prop(cat_geom, "Format signature", rep.header.valid_magic ? "MAGE (valid magic)" : "invalid signature", rep.header.valid_magic ? QColor(0x55, 0xff, 0x55) : QColor(0xff, 0x55, 0x55));
        add_prop(cat_geom, "Format version", rep.header.valid_magic ? QString("v%1").arg(static_cast<int>(rep.header.version)) : "-");
        add_prop(cat_geom, "Payload chunk size", rep.header.valid_magic ? QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.header.chunk_size)) : "-");
        add_prop(cat_geom, "Estimated chunks", rep.header.valid_magic ? QString::number(rep.header.est_chunks) : "-");
        QTreeWidgetItem *cat_crypto = add_category("Cryptographic parameters");
        add_prop(cat_crypto, "Cipher algorithm", rep.header.valid_magic ? QString::fromStdString(rep.header.algo_name) : "-");
        add_prop(cat_crypto, "Argon2id memory cost", rep.header.valid_magic ? QString::fromStdString(pk::crypto::am_i_evil::format_bytes(static_cast<uint64_t>(rep.header.memory_cost_kb) * 1024)) : "-");
        add_prop(cat_crypto, "Argon2id time cost", rep.header.valid_magic ? QString("%1 pass(es)").arg(rep.header.time_cost) : "-");
        add_prop(cat_crypto, "Argon2id parallelism", rep.header.valid_magic ? QString("%1 thread(s)").arg(rep.header.parallelism) : "-");
        QString p = QString::fromStdString(rep.header.___filepath.string());
        if (__custom_entropy_.contains(p) && __custom_entropy_[p].has_any())
        {
            const auto &ce = __custom_entropy_[p];
            QString src_str = QString("Custom (%1)").arg(ce.badge_tag());
            add_prop(cat_crypto, "Entropy source", src_str, QColor(0x55, 0xff, 0x55), "Using archive-specific custom entropy");
        }
        else
        {
            add_prop(cat_crypto, "Entropy source", "default (global input)", QColor(0xa0, 0xa0, 0xa0), "Using global password and keyfile inputs");
        }
        QTreeWidgetItem *cat_cmp = add_category("Compression n' metadata");
        QString cmp_str = "-";
        if (rep.header.valid_magic)
        {
            cmp_str = QString::fromStdString(rep.header.compression_algo_name);
            if (rep.header.compression_algo_id <= 1)
            {
                if (rep.header.compression_level == 0)
                    cmp_str += " (store)";
                else
                    cmp_str += QString(" (level %1)").arg(rep.header.compression_level);
            }
        }
        add_prop(cat_cmp, "Compression algorithm", cmp_str);
        add_prop(cat_cmp, "Preserves POSIX / Win32 metadata", rep.header.valid_magic ? (rep.header.has_metadata ? "yes (true)" : "no (false)") : "-");
        QTreeWidgetItem *cat_health = add_category("Header health n' security");
        if (!rep.header.valid_magic)
        {
            add_prop(cat_health, "Structure integrity", "INVALID (not a valid signature or a MAGE file in general)", QColor(0xff, 0x55, 0x55));
        }
        else
        {
            if (!rep.header.kdf_safe)
                add_prop(cat_health, "KDF DoS safety", "WARN (KDF parameters exceed set limits)", QColor(0xff, 0x44, 0x44));
            else
                add_prop(cat_health, "KDF DoS safety", "OK", QColor(0x55, 0xff, 0x55));

            if (!rep.header.structure_ok)
                add_prop(cat_health, "Structure integrity", "INVALID (header structure is malformed or invalid)", QColor(0xff, 0xaa, 0x33));
            else
                add_prop(cat_health, "Structure integrity", "OK", QColor(0x55, 0xff, 0x55));
        }
        QStringList diag_notes;
        if (!rep.header.file_exists)
        {
            diag_notes << "(?) File existence: target archive does not exist or cannot be accessed.";
        }
        else if (!rep.header.valid_magic)
        {
            diag_notes << "(X) Format signature: invalid magic bytes (not a valid MAGE archive).";
            diag_notes << QString("(X) Container structure: %1.").arg(QString::fromStdString(rep.header.sanity_notes));
        }
        else
        {
            diag_notes << "(OK) Format signature: MAGE container format.";
            if (rep.header.version == 1)
                diag_notes << "(OK) Container version: v1 (supported).";
            else
                diag_notes << QString("(OK) Container version: v%1 (unsupported version).").arg(static_cast<int>(rep.header.version));

            if (rep.header.algo_id <= 2)
                diag_notes << QString("(OK) Cipher algorithm: %1.").arg(QString::fromStdString(rep.header.algo_name));
            else
                diag_notes << QString("(X) Cipher algorithm: unrecognized cipher algorithm ID (%1).").arg(rep.header.algo_id);
            if (rep.header.compression_algo_id <= 1)
            {
                QString cmp = QString::fromStdString(rep.header.compression_algo_name);
                if (rep.header.compression_level == 0)
                    cmp += " (store)";
                else
                    cmp += QString(" (level %1)").arg(rep.header.compression_level);
                diag_notes << QString("(OK) Compression: %1.").arg(cmp);
            }
            else
            {
                diag_notes << QString("(X) Compression: unrecognized compression algorithm ID (%1).").arg(rep.header.compression_algo_id);
            }
            if (rep.header.chunk_size >= 1024 * 1024 && rep.header.chunk_size <= 64 * 1024 * 1024)
                diag_notes << QString("(OK) Chunk geometry: %1 (within 1MB - 64MBs bounds).").arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.header.chunk_size)));
            else
                diag_notes << QString("(X) Chunk geometry: %1 (out of bounds, 1MB - 64MBs required).").arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.header.chunk_size)));
            uint64_t mem_bytes = static_cast<uint64_t>(rep.header.memory_cost_kb) * 1024;
            if (rep.header.memory_cost_kb <= 2 * 1024 * 1024)
                diag_notes << QString("(OK) Argon2id memory cost: %1 (within 2GBs safety limit).").arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(mem_bytes)));
            else
                diag_notes << QString("(X) Argon2id memory cost: %1 (exceeds 2GBs safety threshold).").arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(mem_bytes)));
            if (rep.header.time_cost >= 1 && rep.header.time_cost <= 32)
                diag_notes << QString("(OK) Argon2id time cost: %1 pass(es) (within 32 passes limit).").arg(rep.header.time_cost);
            else
                diag_notes << QString("(X) Argon2id time cost: %1 pass(es) (exceeds 32 passes limit).").arg(rep.header.time_cost);
            if (rep.header.parallelism >= 1 && rep.header.parallelism <= 32)
                diag_notes << QString("(OK) Argon2id parallelism: %1 lane(s) (within 1 - 32 lanes limit).").arg(rep.header.parallelism);
            else
                diag_notes << QString("(X) Argon2id parallelism: %1 lane(s) (invalid or excessive lanes).").arg(rep.header.parallelism);
            diag_notes << QString("(OK) Metadata preservation: %1").arg(rep.header.has_metadata ? "enabled." : "disabled.");
            if (rep.header.structure_ok && rep.header.kdf_safe)
                diag_notes << "(OK) Container health: all parameters standard and valid.";
            else
                diag_notes << QString("(X) Container health: %1.").arg(QString::fromStdString(rep.header.sanity_notes));
        }
        bool is_healthy = (rep.header.valid_magic && rep.header.structure_ok && rep.header.kdf_safe);
        QColor diag_color = is_healthy ? QColor(0xa0, 0xa0, 0xa0) : QColor(0xe5, 0xc0, 0x7b);
        add_prop(cat_health, "Diagnostics / notes", "hover over to see...", diag_color, diag_notes.join("\n"));
        QTreeWidgetItem *cat_aead = add_category("Cryptographic status");
        bool is_pending = (rep.verdict == pk::crypto::am_i_evil::__vv_::io_error && rep.verified_chunks == 0 && rep.error_details.empty());
        if (is_pending)
        {
            add_prop(cat_aead, "Audit status", "pending (enter password / keyfile)", QColor(0xaa, 0xaa, 0xaa));
        }
        else
        {
            QString verdict_str = QString::fromStdString(pk::crypto::am_i_evil::verdict_to_str(rep.verdict));
            QColor verdict_color = (rep.verdict == pk::crypto::am_i_evil::__vv_::success) ? QColor(0x55, 0xff, 0x55) : QColor(0xff, 0x55, 0x55);
            add_prop(cat_aead, "Verification verdict", verdict_str, verdict_color);
            add_prop(cat_aead, "AEAD chunks authenticated", QString("%1 / %2").arg(rep.verified_chunks).arg(rep.total_chunks));
            if (rep.verdict == pk::crypto::am_i_evil::__vv_::success)
            {
                add_prop(cat_aead, "Declared original size", QString("%1 (%2)").arg(QString::fromStdString(pk::crypto::am_i_evil::numbers_with_commas_unlike_in_gta_5(rep.dub) + "B")).arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.dub))));
                add_prop(cat_aead, "Compression ratio", QString("%1% (%2% saved)").arg(QString::number(rep.compression_ratio_percent, 'f', 1)).arg(QString::number(100.0 - rep.compression_ratio_percent, 'f', 1)));
                add_prop(cat_aead, "Contained archive entries", QString("%1 regular file(s), %2 folder(s)").arg(rep.regular_files).arg(rep.directories));
                add_prop(cat_aead, "Path traversal check", "OK", QColor(0x55, 0xff, 0x55));
            }
            else if (!rep.error_details.empty())
            {
                add_prop(cat_aead, "Failure diagnostics", QString::fromStdString(rep.error_details), QColor(0xff, 0x66, 0x66), QString::fromStdString(rep.error_details));
            }
        }
        adjust_pc();
    }
    void cd_am_i_evil::on_verify_integrity()
    {
        if (m_reports.empty())
        {
            warning(this, "No archives selected", "Add at least one archive to verify.");
            return;
        }
        bool global_has_entropy = !password_v->text().isEmpty() || !keyfile_path_v->text().isEmpty();
        if (!global_has_entropy)
        {
            bool all_have_custom = true;
            for (const auto &rep : m_reports)
            {
                QString p = QString::fromStdString(rep.header.___filepath.string());
                if (!__custom_entropy_.contains(p) || !__custom_entropy_[p].has_any())
                {
                    all_have_custom = false;
                    break;
                }
            }
            if (!all_have_custom)
            {
                warning(this, "Credentials required", "A password or keyfile is required to verify AEAD cryptographic integrity (either globally or custom per-archive); container headers are already inspected without a password.", "Enter a password or select a keyfile before verifying integrity.");
                return;
            }
        }
        QStringList passed_reports;
        QStringList failed_reports;
        int cancelled_count = 0;
        int actual_error_count = 0;
        QList<cd_prog_dialog::verify_job> jobs;
        for (int i = 0; i < static_cast<int>(m_reports.size()); ++i)
        {
            auto &rep = m_reports[i];
            QString archive_path = QString::fromStdString(rep.header.___filepath.string());
            QString base_name = QFileInfo(archive_path).fileName();
            QString eff_pwd = password_v->text();
            QString eff_kf = keyfile_path_v->text();
            if (__custom_entropy_.contains(archive_path))
            {
                const auto &ce = __custom_entropy_[archive_path];
                if (ce.has_password)
                    eff_pwd = ce.password;
                if (ce.has_keyfile)
                    eff_kf = ce.keyfile_path;
            }
            if (eff_pwd.isEmpty() && eff_kf.isEmpty())
            {
                actual_error_count++;
                rep.verdict = pk::crypto::am_i_evil::__vv_::io_error;
                rep.error_details = "No password or keyfile provided for this archive.";
                QTreeWidgetItem *tree_item = queue_tree->topLevelItem(i);
                if (tree_item)
                {
                    tree_item->setText(2, "INVALID");
                    tree_item->setToolTip(2, "No entropy source provided");
                    tree_item->setForeground(2, QColor(0xff, 0x55, 0x55));
                }
                failed_reports.append(QString(">> %1:\n   - Error: No entropy source provided").arg(base_name));
                continue;
            }
            jobs.append({i, archive_path, base_name, eff_pwd, eff_kf});
        }
        if (!jobs.isEmpty())
        {
            cd_prog_dialog pd(jobs, this);
            pd.exec();
            for (const auto &res : pd.results())
            {
                int i = res.list_index;
                if (i < 0 || i >= static_cast<int>(m_reports.size()))
                    continue;
                auto &rep = m_reports[i];
                QTreeWidgetItem *tree_item = queue_tree->topLevelItem(i);
                QString base_name = res.display_name;
                if (res.success)
                {
                    rep = res.report;
                    if (tree_item)
                    {
                        tree_item->setText(2, "OK");
                        tree_item->setToolTip(2, QString("authentic n' verified (OK)\n%1 / %2 chunks verified").arg(rep.verified_chunks).arg(rep.total_chunks));
                        tree_item->setForeground(2, QColor(0x55, 0xff, 0x55));
                    }
                    QString msg = QString(">> %1:\n"
                                          "   - Status: %2\n"
                                          "   - AEAD chunks: %3 / %4 verified\n"
                                          "   - Uncompressed: %5 (compression: %6%)\n"
                                          "   - Entries: %7 file(s), %8 folder(s)")
                                      .arg(base_name)
                                      .arg(QString::fromStdString(pk::crypto::am_i_evil::verdict_to_str(rep.verdict)))
                                      .arg(rep.verified_chunks)
                                      .arg(rep.total_chunks)
                                      .arg(QString::fromStdString(pk::crypto::am_i_evil::format_bytes(rep.dub)))
                                      .arg(QString::number(rep.compression_ratio_percent, 'f', 1))
                                      .arg(rep.regular_files)
                                      .arg(rep.directories);
                    passed_reports.append(msg);
                }
                else
                {
                    if (res.cancelled)
                    {
                        cancelled_count++;
                        if (tree_item)
                        {
                            tree_item->setText(2, "CANCELLED");
                            tree_item->setToolTip(2, "Verification cancelled by user");
                            tree_item->setForeground(2, QColor(0xe5, 0xc0, 0x7b));
                        }
                        rep.error_details = "Verification cancelled by user.";
                        failed_reports.append(QString(">> %1:\n   - Verification cancelled").arg(base_name));
                    }
                    else
                    {
                        actual_error_count++;
                        if (res.report.header.file_exists)
                        {
                            rep = res.report;
                        }
                        if (rep.error_details.empty())
                        {
                            rep.error_details = res.error_msg.toStdString();
                        }
                        if (tree_item)
                        {
                            QString verdict_lbl = QString::fromStdString(pk::crypto::am_i_evil::verdict_to_str(rep.verdict));
                            tree_item->setText(2, "INVALID");
                            tree_item->setToolTip(2, verdict_lbl + "\n" + QString::fromStdString(rep.error_details));
                            tree_item->setForeground(2, QColor(0xff, 0x55, 0x55));
                        }
                        QString err = QString::fromStdString(rep.error_details);
                        failed_reports.append(QString(">> %1:\n   - Error: %2").arg(base_name).arg(err));
                    }
                }
            }
        }
        update_qs();
        refresh_properties();
        adjust_qc();
        if (actual_error_count == 0 && cancelled_count > 0 && passed_reports.isEmpty())
        {
            return;
        }
        QString summary;
        if (!passed_reports.isEmpty())
        {
            summary += QString(">>> Verification passed (%1)\n\n").arg(passed_reports.size());
            summary += passed_reports.join("\n\n");
        }
        if (!failed_reports.isEmpty())
        {
            if (!summary.isEmpty())
                summary += "\n\n";
            QString hdr = (actual_error_count == 0) ? "Verification cancelled" : "Verification failed";
            summary += QString(">>> %1 (%2)\n\n").arg(hdr).arg(failed_reports.size());
            summary += failed_reports.join("\n\n");
        }
        if (failed_reports.isEmpty())
        {
            info(this, "Verification completed", summary, QString(), false);
        }
        else if (actual_error_count == 0)
        {
            info(this, "Verification cancelled", summary);
        }
        else if (passed_reports.isEmpty())
        {
            error(this, "Verification failed", summary);
        }
        else
        {
            warning(this, "Verification completed with failures", summary);
        }
    }
    void cd_am_i_evil::on_export_log()
    {
        if (m_reports.empty())
        {
            warning(this, "No inspection data", "No archive inspection data to export.");
            return;
        }
        QString save_path = QFileDialog::getSaveFileName(this, "Export verification log", "mage_verification_report.log", "Log files (*.log);; Text files (*.txt);; All files (*.*)");
        if (save_path.isEmpty())
            return;
        std::string log_content = pk::crypto::am_i_evil::mk_export_log(m_reports);
        QFile file(save_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            error(this, "File write error", QString("Could not open file for writing:\n%1").arg(save_path));
            return;
        }
        QTextStream out(&file);
        out << QString::fromStdString(log_content);
        file.close();
        info(this, "Log exported", QString("Verification log successfully saved to:\n%1").arg(save_path));
    }
    void cd_am_i_evil::on_close()
    {
        if (!password_v->text().isEmpty())
        {
            QString s = password_v->text();
            pk::mem_::secure_wipe(reinterpret_cast<void *>(const_cast<QChar *>(s.data())), s.size() * sizeof(QChar));
            password_v->clear();
        }
        keyfile_path_v->clear();
        for (auto &ce : __custom_entropy_)
            ce.wipe();
        __custom_entropy_.clear();
        accept();
    }
    void cd_am_i_evil::adjust_qc()
    {
        if (!queue_tree)
            return;
        queue_tree->resizeColumnToContents(0);
        queue_tree->resizeColumnToContents(1);
        queue_tree->resizeColumnToContents(2);
        int other_cols = queue_tree->columnWidth(1) + queue_tree->columnWidth(2);
        int avail = queue_tree->viewport()->width() - other_cols;
        if (avail > queue_tree->columnWidth(0))
            queue_tree->setColumnWidth(0, avail);
    }
    void cd_am_i_evil::adjust_pc()
    {
        if (!pt)
            return;
        pt->resizeColumnToContents(0);
        pt->resizeColumnToContents(1);
        int min_col0 = 175;
        if (pt->columnWidth(0) < min_col0)
            pt->setColumnWidth(0, min_col0);
        int avail = pt->viewport()->width() - pt->columnWidth(0);
        if (avail > pt->columnWidth(1))
            pt->setColumnWidth(1, avail);
    }
    void cd_am_i_evil::resizeEvent(QResizeEvent *event)
    {
        QDialog::resizeEvent(event);
        adjust_qc();
        adjust_pc();
    }
    cd_settings::cd_settings(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Settings");
        setWindowFlags(windowFlags() | Qt::Window);
        setFixedSize(540, 380);
        setup_ui();
        dont_burn_my_eyes(this);
    }
    void cd_settings::setup_ui()
    {
        QVBoxLayout *main_layout = new QVBoxLayout(this);
        QTabWidget *tabs = new QTabWidget(this);
        tabs->setUsesScrollButtons(true);
        tabs->setElideMode(Qt::ElideNone);
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
        chunk_size_used->setValue(pk::cfg::settings::instance().def_chunk_size());
        chunk_size_used->setSuffix(chunk_size_used->value() == 1 ? "MB" : "MBs");
        chunk_size_used->setContextMenuPolicy(Qt::NoContextMenu);
        connect(chunk_size_used, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val)
                { chunk_size_used->setSuffix(val == 1 ? "MB" : "MBs"); });
        chunk_layout->addWidget(chunk_size_used);
        chunk_layout->addStretch();
        layout_gen->addLayout(chunk_layout);
        QString tooltips_path = QCoreApplication::applicationDirPath() + "/assets/txt_data/tooltips";
        QMap<QString, QString> tooltips = parse_tooltips(tooltips_path);
        if (tooltips.contains("default_output_path"))
            def_output->setToolTip(tooltips["default_output_path"]);
        if (tooltips.contains("default_extension"))
            def_ext__->setToolTip(tooltips["default_extension"]);
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
        if (tooltips.contains("disable_mouse_clicking"))
            m_disable_rc->setToolTip(tooltips["disable_mouse_clicking"]);
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
        QWidget *tab_ext = new QWidget();
        QFormLayout *form_ext = new QFormLayout(tab_ext);
        ext_behavior = new QComboBox(this);
        ext_behavior->addItems({"Create archive folder", "Extract here (flat)", "Smart-ish extract"});
        ext_behavior->setCurrentIndex(pk::cfg::settings::instance().def_ext_behavior());
        if (tooltips.contains("extraction_behavior"))
            ext_behavior->setToolTip(tooltips["extraction_behavior"]);
        form_ext->addRow("Behavior:", ext_behavior);
        ext_open = new QCheckBox("Auto-open output folder", this);
        ext_open->setChecked(pk::cfg::settings::instance().def_ext_open());
        if (tooltips.contains("auto_open_output"))
            ext_open->setToolTip(tooltips["auto_open_output"]);
        form_ext->addRow("", ext_open);
        ext_overwrite = new QComboBox(this);
        ext_overwrite->addItems({"Ask", "Skip", "Overwrite"});
        ext_overwrite->setCurrentIndex(pk::cfg::settings::instance().def_ext_overwrite());
        if (tooltips.contains("overwrite_rules"))
            ext_overwrite->setToolTip(tooltips["overwrite_rules"]);
        form_ext->addRow("Conflict rule:", ext_overwrite);
        tabs->addTab(tab_ext, "Extraction");
        QWidget *tab_enc = new QWidget();
        QFormLayout *form_enc = new QFormLayout(tab_enc);
        algo_combo = new QComboBox(this);
        algo_combo->addItem("AES-256-GCM");
        algo_combo->addItem("XChaCha20-Poly1305");
        algo_combo->addItem("AES-256-SIV");
        algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cipher());
        form_enc->addRow("Default cipher:", algo_combo);
        QLabel *aes_ni_label2 = new QLabel(this);
        if (pk::crypto::aes_ni_there())
        {
            aes_ni_label2->setText("AES-NI: yeah");
            aes_ni_label2->setStyleSheet("QLabel { color: #a0dca0; font-size: 11px; }");
            aes_ni_label2->setToolTip("Your CPU supports AES-NI, making AES-256-GCM or AES-256-SIV\nextremely fast.");
        }
        else
        {
            aes_ni_label2->setText("AES-NI: nah");
            aes_ni_label2->setStyleSheet("QLabel { color: #e0d060; font-size: 11px; }");
            aes_ni_label2->setToolTip("Your CPU does not support AES-NI; XChaCha20-Poly1305 is recommended\nover AES-256-GCM or AES-256-SIV\nfor better performance and security on this system.");
        }
        form_enc->addRow("", aes_ni_label2);
        s_tc = new QSpinBox(this);
        s_tc->setRange(1, 100);
        s_tc->setValue(pk::cfg::settings::instance().def_time_cost());
        s_tc->setContextMenuPolicy(Qt::NoContextMenu);
        s_mem_cost = new QSpinBox(this);
        s_mem_cost->setRange(1, 4096);
        s_mem_cost->setValue(pk::cfg::settings::instance().def_mem_cost() / 1024);
        s_mem_cost->setSuffix(s_mem_cost->value() == 1 ? "MB" : "MBs");
        s_mem_cost->setContextMenuPolicy(Qt::NoContextMenu);
        connect(s_mem_cost, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val)
                { s_mem_cost->setSuffix(val == 1 ? "MB" : "MBs"); });
        s_cores = new QSpinBox(this);
        s_cores->setRange(1, 64);
        s_cores->setValue(pk::cfg::settings::instance().def_cores());
        s_cores->setContextMenuPolicy(Qt::NoContextMenu);
        form_enc->addRow("Default Argon2id time cost:", s_tc);
        form_enc->addRow("Default Argon2id memory cost:", s_mem_cost);
        form_enc->addRow("Default Argon2id parallelism:", s_cores);
        tabs->addTab(tab_enc, "Encryption and KDF");
        QWidget *tab_comp = new QWidget();
        QFormLayout *form_comp = new QFormLayout(tab_comp);
        cmp_algo_combo = new QComboBox(this);
        cmp_algo_combo->addItem("ZSTD");
        cmp_algo_combo->addItem("LZMA2");
        cmp_algo_combo->setCurrentIndex(pk::cfg::settings::instance().def_cmp_algorithm());
        form_comp->addRow("Default algorithm:", cmp_algo_combo);
        cmp_preset_combo = new QComboBox(this);
        cmp_preset_combo->addItem("Store");
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
        if (tooltips.contains("default_cipher"))
            algo_combo->setToolTip(tooltips["default_cipher"]);
        if (tooltips.contains("default_argon2_time"))
            s_tc->setToolTip(tooltips["default_argon2_time"]);
        if (tooltips.contains("default_argon2_memory"))
            s_mem_cost->setToolTip(tooltips["default_argon2_memory"]);
        if (tooltips.contains("default_argon2_parallelism"))
            s_cores->setToolTip(tooltips["default_argon2_parallelism"]);
        if (tooltips.contains("default_compression_algo"))
            cmp_algo_combo->setToolTip(tooltips["default_compression_algo"]);
        if (tooltips.contains("default_compression_preset"))
            cmp_preset_combo->setToolTip(tooltips["default_compression_preset"]);
        if (tooltips.contains("use_raw_compression"))
            cmp_use_raw->setToolTip(tooltips["use_raw_compression"]);
        if (tooltips.contains("default_compression_raw"))
            _cmp_raw_lvl->setToolTip(tooltips["default_compression_raw"]);
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
            set_enabled(cmp_use_raw, true);
            if (use_raw)
            {
                set_enabled(cmp_preset_combo, false);
                set_enabled(_cmp_raw_lvl, true);
                if (algo == 0)
                    _cmp_raw_lvl->setRange(0, 22);
                else if (algo == 1)
                    _cmp_raw_lvl->setRange(0, 9);
            }
            else
            {
                set_enabled(cmp_preset_combo, true);
                set_enabled(_cmp_raw_lvl, false);
            }
        };
        connect(cmp_algo_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update_comp_ui);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(cmp_use_raw, &QCheckBox::checkStateChanged, this, update_comp_ui);
#else
        connect(cmp_use_raw, &QCheckBox::stateChanged, this, update_comp_ui);
#endif
        update_comp_ui();
        tabs->addTab(tab_comp, "Compression");
        QWidget *tab_cm = new QWidget();
        QVBoxLayout *layout_cm = new QVBoxLayout(tab_cm);
        layout_cm->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        layout_cm->setContentsMargins(15, 20, 15, 20);
#ifdef _WIN32
        QPushButton *btn_install_cm = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/install.svg")), " Install context menu", this);
        QLabel *lbl_install = new QLabel("Adds cascading MAGE options to your Windows right-click menu, allowing you to instantly encrypt or decrypt files without launching the app first.", this);
        lbl_install->setStyleSheet("color: #aaaaaa; font-size: 11px;");
        lbl_install->setWordWrap(true);
        QPushButton *btn_remove_cm = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/remove.svg")), " Remove context menu", this);
        QLabel *lbl_remove = new QLabel("Removes MAGE options from your Windows right-click menu cleanly.", this);
        lbl_remove->setStyleSheet("color: #aaaaaa; font-size: 11px;");
        lbl_remove->setWordWrap(true);
        layout_cm->addWidget(btn_install_cm, 0, Qt::AlignLeft);
        layout_cm->addSpacing(4);
        layout_cm->addWidget(lbl_install);
        layout_cm->addSpacing(20);
        layout_cm->addWidget(btn_remove_cm, 0, Qt::AlignLeft);
        layout_cm->addSpacing(4);
        layout_cm->addWidget(lbl_remove);
        layout_cm->addStretch();
        tabs->addTab(tab_cm, "Context menu");
        connect(btn_install_cm, &QPushButton::clicked, this, [this]()
                {
            pk::os::cm::install();
            pk::ui::outs::info(this, "OK", "Context menus installed successfully."); });
        connect(btn_remove_cm, &QPushButton::clicked, this, [this]()
                {
            pk::os::cm::remove();
            pk::ui::outs::info(this, "OK", "Context menus removed successfully."); });
#else
        QLabel *lbl_unsupported = new QLabel("<b>[ OS NOT SUPPORTED! ]</b><br><br>Context menu integration is currently only supported on Windows.", this);
        lbl_unsupported->setStyleSheet("color: #aaaaaa; font-size: 12px;");
        lbl_unsupported->setWordWrap(true);
        layout_cm->addWidget(lbl_unsupported);
        layout_cm->addStretch();
        tabs->addTab(tab_cm, "Context menu");
#endif
        main_layout->addWidget(tabs);
        QHBoxLayout *action_layout = new QHBoxLayout();
        QLabel *lbl_tip = new QLabel("     Tip: hover over a setting to get its tooltip!", this);
        lbl_tip->setStyleSheet("color: #aaaaaa; font-style: italic; font-size: 11px;");
        action_layout->addWidget(lbl_tip);
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
            warning(this, "Invalid default path", err_msg);
            return;
        }
        QString ext = def_ext__->text();
        if (!ext.startsWith("."))
            ext = "." + ext;
        if (!pk::ui::outs::r_u_a_valid_filename(ext, &err_msg))
        {
            warning(this, "Invalid extension", err_msg);
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
        s.ss_def_ext_behavior(ext_behavior->currentIndex());
        s.ss_def_ext_open(ext_open->isChecked());
        s.ss_def_ext_overwrite(ext_overwrite->currentIndex());
        s.save();
        info(this, "Settings saved", "Settings successfully saved.");
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
            "Version: v0.6a\n"
            "Build: " __DATE__ " " __TIME__ "\n\n"
            "Made by: Common, just Common.\n"
            "Audited by: no one. ",
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
    static QWidget *cd_int_c = nullptr;
    void toggle_internal_console()
    {
        if (cd_int_c)
        {
            if (cd_int_c->isVisible())
            {
                cd_int_c->hide();
            }
            else
            {
                cd_int_c->show();
                pk::ui::outs::dont_burn_my_eyes(cd_int_c);
                cd_int_c->raise();
                cd_int_c->activateWindow();
            }
            return;
        }
        cd_int_c = new QWidget();
        cd_int_c->setAttribute(Qt::WA_QuitOnClose, false);
        cd_int_c->setWindowTitle("MAGE - internal debug console");
        cd_int_c->resize(800, 600);
        cd_int_c->setMinimumSize(600, 380);
        pk::ui::outs::dont_burn_my_eyes(cd_int_c);
        QVBoxLayout *layout = new QVBoxLayout(cd_int_c);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);
        QTextEdit *text_edit = new QTextEdit(cd_int_c);
        text_edit->setReadOnly(true);
        text_edit->setStyleSheet(
            "QTextEdit {"
            "    background-color: #202020;"
            "    color: #d4d4d4;"
            "    font-family: 'Consolas', 'Courier New', 'Fira Code', monospace;"
            "    font-size: 11pt;"
            "    border: 1px solid #333333;"
            "    border-radius: 4px;"
            "    padding: 6px;"
            "    selection-background-color: #007acc;"
            "    selection-color: #ffffff;"
            "}");
        layout->addWidget(text_edit, 1);
        auto format_log_line = [](const QString &raw) -> QString
        {
            QString escaped = raw.toHtmlEscaped();
            static const QRegularExpression re(R"(^(\[\d{2}:\d{2}:\d{2}(?:\.\d{3})?\])\s*(.*)$)");
            auto match = re.match(escaped);
            QString timestamp_html;
            QString body;
            if (match.hasMatch())
            {
                timestamp_html = QString("<span style=\"color: #61afef; font-weight: 600;\">%1</span> ").arg(match.captured(1));
                body = match.captured(2);
            }
            else
            {
                body = escaped;
            }
            QString lower = body.toLower();
            QString body_color = "#d4d4d4";
            if (lower.contains("error") || lower.contains("failed") || lower.contains("fail") || lower.contains("invalid") || lower.contains("corrupt"))
            {
                body_color = "#f44747";
            }
            else if (lower.contains("warn") || lower.contains("warning") || lower.contains("caution") || lower.contains("notice"))
            {
                body_color = "#e5c07b";
            }
            else if (lower.contains("success") || lower.contains("successfully") || lower.contains("verified") || lower.contains("authentic") || lower.contains(" ok") || lower.startsWith("ok"))
            {
                body_color = "#98c379";
            }
            return QString("<div style=\"margin: 1px 0; line-height: 135%;\">%1<span style=\"color: %2;\">%3</span></div>").arg(timestamp_html, body_color, body);
        };
        for (const auto &msg : pk::core::logger::instance().get_history())
        {
            text_edit->append(format_log_line(QString::fromStdString(msg)));
        }
        text_edit->moveCursor(QTextCursor::End);
        QHBoxLayout *action_layout = new QHBoxLayout();
        QLabel *lbl_info = new QLabel("                         Internal runtime log n' diagnostics", cd_int_c);
        lbl_info->setStyleSheet("color: #777777; font-size: 11px; font-style: italic;");
        action_layout->addWidget(lbl_info);
        action_layout->addStretch();
        QPushButton *btn_clear = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/rm_all.svg")), " Clear console", cd_int_c);
        QPushButton *btn_export = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/save.svg")), " Export log...", cd_int_c);
        action_layout->addWidget(btn_clear);
        action_layout->addWidget(btn_export);
        layout->addLayout(action_layout);
        QObject::connect(btn_clear, &QPushButton::clicked, text_edit, &QTextEdit::clear);
        QObject::connect(btn_export, &QPushButton::clicked, cd_int_c, [text_edit]()
                         {
            QString path = QFileDialog::getSaveFileName(cd_int_c, "Export log", "mage_internal.log", "Log files (*.log);; Text files (*.txt);; All files (*.*)");
            if (!path.isEmpty())
            {
                QFile f(path);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    f.write(text_edit->toPlainText().toUtf8());
                    f.close();
                    pk::ui::outs::info(cd_int_c, "OK", QString("Log successfully exported to:\n%1").arg(path));
                }
            } });
        pk::core::logger::instance().set_callback([text_edit, format_log_line](const std::string &msg)
                                                  {
            QString qmsg = QString::fromStdString(msg);
            QMetaObject::invokeMethod(text_edit, [text_edit, qmsg, format_log_line]() {
                text_edit->append(format_log_line(qmsg));
                text_edit->moveCursor(QTextCursor::End);
            }, Qt::QueuedConnection); });

        cd_int_c->show();
        pk::ui::outs::dont_burn_my_eyes(cd_int_c);
    }
}

// end