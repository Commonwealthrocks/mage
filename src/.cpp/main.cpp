// main.cpp
// last updated: 04/10/2026
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
#include <QMouseEvent>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QIcon>
#include <QTabBar>
#include <QPropertyAnimation>
#include <QPointer>
class scroll_filter : public QObject
{
public:
    explicit scroll_filter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool m_m2_scrolled = false;
    QPointer<QScrollBar> m_active_hbar;
    QPropertyAnimation *m_scroll_anim = nullptr;
    int m_target_value = 0;
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel)
        {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            bool shift = (wheelEvent->modifiers() & Qt::ShiftModifier) || (QApplication::keyboardModifiers() & Qt::ShiftModifier);
            bool right_click = (wheelEvent->buttons() & Qt::RightButton) || (QApplication::mouseButtons() & Qt::RightButton);

            QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
            if (!tabBar && obj && obj->parent())
            {
                tabBar = qobject_cast<QTabBar *>(obj->parent());
            }
            if (tabBar)
            {
                int delta = wheelEvent->angleDelta().y();
                if (delta == 0)
                    delta = wheelEvent->angleDelta().x();
                if (delta != 0)
                {
                    int count = tabBar->count();
                    if (count > 1)
                    {
                        int cur = tabBar->currentIndex();
                        int next = cur + (delta < 0 ? 1 : -1);
                        next = qBound(0, next, count - 1);
                        if (next != cur)
                            tabBar->setCurrentIndex(next);
                    }
                    return true;
                }
            }

            if (qobject_cast<QComboBox *>(obj) ||
                qobject_cast<QAbstractSpinBox *>(obj) ||
                qobject_cast<QSlider *>(obj))
            {
                if (!shift && !right_click)
                {
                    wheelEvent->ignore();
                    return true;
                }
            }
            else if (shift || right_click || wheelEvent->angleDelta().x() != 0)
            {
                QWidget *w = qobject_cast<QWidget *>(obj);
                QAbstractScrollArea *scroll_area = nullptr;
                while (w)
                {
                    scroll_area = qobject_cast<QAbstractScrollArea *>(w);
                    if (scroll_area)
                        break;
                    w = w->parentWidget();
                }
                if (scroll_area && scroll_area->horizontalScrollBar() && scroll_area->horizontalScrollBar()->maximum() > 0)
                {
                    QScrollBar *hbar = scroll_area->horizontalScrollBar();
                    int delta = (shift || right_click) ? wheelEvent->angleDelta().y() : wheelEvent->angleDelta().x();
                    if (delta == 0)
                        delta = wheelEvent->angleDelta().x();
                    if (delta == 0)
                        delta = wheelEvent->angleDelta().y();
                    if (delta != 0)
                    {
                        if (right_click)
                            m_m2_scrolled = true;
                        if (m_active_hbar != hbar)
                        {
                            if (m_scroll_anim)
                            {
                                m_scroll_anim->stop();
                            }
                            m_active_hbar = hbar;
                            m_target_value = hbar->value();
                        }
                        if (!m_scroll_anim || m_scroll_anim->targetObject() != hbar)
                        {
                            delete m_scroll_anim;
                            m_scroll_anim = new QPropertyAnimation(hbar, "value", this);
                        }
                        if (m_scroll_anim->state() != QAbstractAnimation::Running)
                        {
                            m_target_value = hbar->value();
                        }
                        else
                        {
                            int cur = hbar->value();
                            if ((delta > 0 && cur < m_target_value) || (delta < 0 && cur > m_target_value))
                            {
                                m_target_value = cur;
                            }
                        }
                        double notches = static_cast<double>(delta) / 120.0;
                        int step = static_cast<int>(notches * 36.0);
                        if (step == 0)
                            step = (delta > 0 ? 1 : -1) * 16;
                        int min_val = hbar->minimum();
                        int max_val = hbar->maximum();
                        m_target_value = std::clamp(m_target_value - step, min_val, max_val);
                        if (hbar->value() != m_target_value || m_scroll_anim->state() == QAbstractAnimation::Running)
                        {
                            m_scroll_anim->stop();
                            m_scroll_anim->setDuration(130);
                            m_scroll_anim->setStartValue(hbar->value());
                            m_scroll_anim->setEndValue(m_target_value);
                            m_scroll_anim->setEasingCurve(QEasingCurve::OutQuad);
                            m_scroll_anim->start();
                        }
                        wheelEvent->accept();
                        return true;
                    }
                }
            }
        }
        else if (event->type() == QEvent::ContextMenu || event->type() == QEvent::MouseButtonRelease)
        {
            if (m_m2_scrolled)
            {
                auto *me = dynamic_cast<QMouseEvent *>(event);
                if (!me || me->button() == Qt::RightButton)
                {
                    m_m2_scrolled = false;
                    event->accept();
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
    app.setOrganizationName("MAGE");
    app.setOrganizationDomain("mage.local");
    app.setApplicationVersion("v0.6a");
    // forgot to update this again, ahem...
    app.setWindowIcon(QIcon(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("assets/imgs/s_icons/mage.ico")));
    QStringList args = app.arguments();
    if (args.size() > 2 && (args[1] == "--encrypt" || args[1] == "--decrypt" || args[1] == "--verify"))
    {
        QString mode = (args[1] == "--encrypt" ? "encrypt" : (args[1] == "--verify" ? "verify" : "decrypt"));
        if (pk::ipc::s_t_prim_instance(mode, args[2]))
        {
            return 0;
        }
    }
    pk::ipc::ipc_server ipc_srv;
    ipc_srv.start();
    // init nacl
    if (sodium_init() < 0)
    {
        return 1;
    }
    app.installEventFilter(new scroll_filter(&app));
    app.installEventFilter(new pk::ui::shortcuts::shortcut_filter(&app));
    pk::ui::sfx::preload();
    if (args.size() > 2 && (args[1] == "--encrypt" || args[1] == "--decrypt" || args[1] == "--verify"))
    {
        QString mode = (args[1] == "--encrypt" ? "encrypt" : (args[1] == "--verify" ? "verify" : "decrypt"));
        pk::ui::gui window(&ipc_srv);
        window.handle_args(mode, args[2], true);
        return app.exec();
    }
    pk::ui::gui window(&ipc_srv);
    window.show();
    return app.exec();
}

// end