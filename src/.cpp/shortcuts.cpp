// shortcuts.cpp
// last updated: 24/06/2026
#include "../.hpp/shortcuts.hpp"
#include <QDir>
#include <QCoreApplication>
#include "../.hpp/outs.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
namespace pk::ui::shortcuts
{
    shortcut_filter::shortcut_filter(QObject *parent) : QObject(parent) {}

    bool shortcut_filter::eventFilter(QObject *obj, QEvent *event)
    {
        if (event->type() == QEvent::KeyPress)
        {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape)
            {
                if (auto *dialog = qobject_cast<QDialog *>(qApp->activeModalWidget()))
                {
                    dialog->reject();
                    return true;
                }
                else if (auto *dialog = qobject_cast<QDialog *>(qApp->activeWindow()))
                {
                    dialog->reject();
                    return true;
                }
            }
            else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
            {
                if (QWidget *active = qApp->activeWindow())
                {
                    if (active->objectName() == "gui" || active->inherits("pk::ui::gui"))
                    {
                        if (keyEvent->key() == Qt::Key_E)
                        {
                            QMetaObject::invokeMethod(active, "on_create_archive_clicked");
                            return true;
                        }
                        else if (keyEvent->key() == Qt::Key_D)
                        {
                            QMetaObject::invokeMethod(active, "on_decrypt_archive_clicked");
                            return true;
                        }
                        else if (keyEvent->key() == Qt::Key_S)
                        {
                            QMetaObject::invokeMethod(active, "on_settings_clicked");
                            return true;
                        }
                    }
                }
            }
            else if (keyEvent->modifiers() == Qt::ControlModifier)
            {
                if (keyEvent->key() == Qt::Key_Q)
                {
                    qApp->quit();
                    return true;
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::BackButton || mouseEvent->button() == Qt::XButton1)
            {
                if (auto *dialog = qobject_cast<QDialog *>(qApp->activeModalWidget()))
                {
                    dialog->reject();
                    return true;
                }
                else if (auto *dialog = qobject_cast<QDialog *>(qApp->activeWindow()))
                {
                    dialog->reject();
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
    void cd_keybinds(QWidget *parent)
    {
        QDialog dialog(parent);
        dialog.setWindowTitle("Keybinds");
        dialog.setFixedSize(400, 250);
        pk::ui::outs::dont_burn_my_eyes(&dialog);
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QLabel *title = new QLabel("Shortcuts", &dialog);
        QFont f = title->font();
        f.setBold(true);
        f.setPointSize(12);
        title->setFont(f);
        layout->addWidget(title);
        QTableWidget *table = new QTableWidget(&dialog);
        table->setColumnCount(2);
        table->setRowCount(5);
        table->setHorizontalHeaderLabels({"Shortcut", "Action"});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setShowGrid(false);
        auto add_row = [&](int row, const QString &keys, const QString &action)
        {
            table->setItem(row, 0, new QTableWidgetItem(keys));
            table->setItem(row, 1, new QTableWidgetItem(action));
        };
        add_row(0, "ESC / MOUSE4", "-> go back");
        add_row(1, "CTRL + SHIFT + E", "-> create archive");
        add_row(2, "CTRL + SHIFT + D", "-> decrypt archive");
        add_row(3, "CTRL + SHIFT + S", "-> settings");
        add_row(4, "CTRL + Q", "-> quit app");
        layout->addWidget(table);
        QPushButton *btn_close = new QPushButton(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/cancel.svg")), "Close", &dialog);
        QObject::connect(btn_close, &QPushButton::clicked, &dialog, &QDialog::accept);
        layout->addWidget(btn_close, 0, Qt::AlignRight);
        dialog.exec();
    }
}

// end