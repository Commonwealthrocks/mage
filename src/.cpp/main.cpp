// main.cpp
// last updated: 08/07/2026
#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include <QImageReader>
#include <QDebug>
#include <QResource>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include "../.hpp/gui.hpp"
#include "../.hpp/settings.hpp"
#include "../.hpp/sfx.hpp"
#include "../.hpp/shortcuts.hpp"
#include "../.hpp/mage_ipc.hpp"
#include <sodium.h>
#include <openssl/evp.h>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QSlider>
#include <QWheelEvent>
#include <QIcon>
#include <QTabBar>
class scroll_filter : public QObject
{
public:
    explicit scroll_filter(QObject *parent = nullptr) : QObject(parent) {}
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel)
        {
            if (qobject_cast<QComboBox *>(obj) ||
                qobject_cast<QAbstractSpinBox *>(obj) ||
                qobject_cast<QTabBar *>(obj) ||
                qobject_cast<QSlider *>(obj))
            {
                auto *wheelEvent = static_cast<QWheelEvent *>(event);
                bool shift = QApplication::keyboardModifiers() & Qt::ShiftModifier;
                bool right_click = QApplication::mouseButtons() & Qt::RightButton;
                if (!shift && !right_click)
                {
                    wheelEvent->ignore();
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MAGE");
    app.setApplicationVersion("v0.3a"); // forgot to change ts, oops
    app.setWindowIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/s_icons/mage.ico")));
    // 1. Check if there's already a primary MAGE instance running.
    //    Do this BEFORE starting our own server, otherwise we'd connect to ourselves.
    QStringList args = app.arguments();
    if (args.size() > 2 && (args[1] == "--encrypt" || args[1] == "--decrypt"))
    {
        QString mode = (args[1] == "--encrypt" ? "encrypt" : "decrypt");
        if (pk::ipc::s_t_prim_instance(mode, args[2]))
        {
            return 0; // forwarded to an existing instance, our job here is done
        }
    }

    // 2. No primary instance found — we are the primary. Start the IPC server now
    //    so any instances launched immediately after us can find us.
    pk::ipc::ipc_server ipc_srv;
    ipc_srv.start();

    // init nacl
    if (sodium_init() < 0)
    {
        return 1;
    }

    try
    {
        pk::cfg::settings::instance().load();
    }
    catch (...)
    {
        // ignore it, it'd be fine!
    }
    app.installEventFilter(new scroll_filter(&app));
    app.installEventFilter(new pk::ui::shortcuts::shortcut_filter(&app));
    pk::ui::sfx::preload();

    if (args.size() > 2 && (args[1] == "--encrypt" || args[1] == "--decrypt"))
    {
        QString mode = (args[1] == "--encrypt" ? "encrypt" : "decrypt");
        pk::ui::gui window(&ipc_srv);
        window.handle_args(mode, args[2]);
        return app.exec();
    }
    pk::ui::gui window(&ipc_srv);
    window.show();
    return app.exec();
}

// end