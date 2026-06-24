// stylesheet.cpp
// last updated: 17/06/2026
// fuck css
#include "../.hpp/stylesheet.hpp"
namespace pk::ui::style
{
    QString use_stylesheet()
    {
        return R"(
            * {
                outline: none;
            }
            QToolTip {
                background-color: #f0f0f0;
                color: #000000;
                border: 1px solid #767676;
                padding: 2px;
            }
            QWidget {
                background-color: #202020;
                color: #e0e0e0;
                font-family: 'Segoe UI', 'Tahoma', sans-serif;
                font-size: 13px;
            }
            QMenuBar {
                background-color: #2d2d2d;
                border-bottom: 1px solid #444444;
            }
            QMenuBar::item {
                background: transparent;
                padding: 4px 10px;
            }
            QMenuBar::item:selected {
                background: #444444;
            }
            QMenu {
                background-color: #2d2d2d;
                border: 1px solid #444444;
            }
            QMenu::item {
                padding: 5px 20px 5px 30px;
            }
            QMenu::icon {
                padding-left: 28px;
            }
            QMenu::item:selected {
                background-color: #007acc;
                color: white;
            }
            QMainWindow, QDialog, QMessageBox {
                background-color: #202020;
            }
            QPushButton {
                background-color: #333333;
                border: 1px solid #555555;
                padding: 6px 12px 6px 8px;
                border-radius: 0px;
                color: #e0e0e0;
                text-align: left;
                qproperty-iconSize: 20px 20px;
            }
            QPushButton:hover {
                background-color: #444444;
                border-color: #007acc;
            }
            QPushButton:pressed {
                background-color: #007acc;
                border-color: #005a9e;
            }
            QLineEdit, QSpinBox, QComboBox {
                background-color: #333333;
                border: 1px solid #555555;
                padding: 4px;
                color: #e0e0e0;
                border-radius: 0px;
            }
            QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
                border: 1px solid #007acc;
            }
            QListWidget {
                background-color: #2a2a2a;
                border: 1px solid #555555;
                color: #e0e0e0;
                border-radius: 0px;
            }
            QListWidget::item:selected {
                background-color: #007acc;
            }
            QGroupBox {
                border: 1px solid #555555;
                margin-top: 10px;
                font-weight: bold;
                border-radius: 0px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 3px;
            }
            QProgressBar {
                border: 1px solid #555555;
                border-radius: 0px;
                text-align: center;
                background-color: #333333;
                color: white;
            }
            QProgressBar::chunk {
                background-color: #007acc;
            }
            QTabWidget::pane {
                border: 1px solid #555555;
                background: #202020;
                border-radius: 0px;
            }
            QTabBar::tab {
                background: #333333;
                border: 1px solid #555555;
                padding: 6px 12px;
                color: #e0e0e0;
                border-radius: 0px;
            }
            QTabBar::tab:selected {
                background: #202020;
                border-bottom-color: #202020;
                font-weight: bold;
            }
            QCheckBox {
                color: #e0e0e0;
            }
            QCheckBox::indicator {
                width: 14px;
                height: 14px;
                border: 1px solid #555555;
                background: #333333;
                border-radius: 2px;
            }
            QCheckBox::indicator:checked {
                background: #007acc;
                border: 1px solid #007acc;
                image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSJ3aGl0ZSIgc3Ryb2tlLXdpZHRoPSI0IiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiPjxwb2x5bGluZSBwb2ludHM9IjIwIDYgOSAxNyA0IDEyIj48L3BvbHlsaW5lPjwvc3ZnPg==);
            }
            QSlider::groove:horizontal {
                border: 1px solid #555555;
                height: 4px;
                background: #333333;
                margin: 2px 0;
            }
            QSlider::handle:horizontal {
                background: #e0e0e0;
                border: 1px solid #555555;
                width: 12px;
                margin: -4px 0;
                border-radius: 6px;
            }
            QSlider::handle:horizontal:hover {
                background: #007acc;
            }
            QHeaderView::section {
                background-color: #2d2d2d;
                color: #e0e0e0;
                padding: 4px;
                border: 1px solid #444444;
            }
        )";
    }
}

// end