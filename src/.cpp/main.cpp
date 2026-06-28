// main.cpp
// last updated: 28/06/2026
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
#include <sodium.h>
#include <openssl/evp.h>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QSlider>
#include <QWheelEvent>
#include <QIcon>
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
    // init nacl
    if (sodium_init() < 0)
    {
        return 1;
    }

    QApplication app(argc, argv);
    app.setApplicationName("MAGE");
    app.setApplicationVersion("v0.1a");
    app.setWindowIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/s_icons/mage.ico")));

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
    pk::ui::gui window;
    QStringList args = app.arguments();
    if (args.size() > 2 && (args[1] == "--encrypt" || args[1] == "--decrypt"))
    {
        window.handle_args(args[1] == "--encrypt" ? "encrypt" : "decrypt", args[2]);
        return 0;
    }
    window.show();
    return app.exec();
}

// end