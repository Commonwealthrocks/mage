// shortcuts.hpp
// last updated: 18/06/2026
#pragma once
#include <QObject>
#include <QEvent>
#include <QWidget>
namespace pk::ui::shortcuts
{
    class shortcut_filter : public QObject
    {
        Q_OBJECT
    public:
        explicit shortcut_filter(QObject *parent = nullptr);
        bool eventFilter(QObject *obj, QEvent *event) override;
    };
    void cd_keybinds(QWidget *parent);
}

// end