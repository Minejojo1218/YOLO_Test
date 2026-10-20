#pragma once

#include <QString>

// 深色现代风格（参考 YOLO 开源）
namespace Theme {

inline QString dark()
{
    return QStringLiteral(R"(
* { font-family: "Microsoft YaHei", "Segoe UI"; }

QWidget { background: #1b1b26; color: #d6d6e0; font-size: 13px; }
QMainWindow, QDialog { background: #16161f; }

QMenuBar { background: #16161f; color: #cfcfe0; padding: 2px; }
QMenuBar::item { padding: 6px 12px; background: transparent; border-radius: 4px; }
QMenuBar::item:selected { background: #2d2d44; }
QMenu { background: #232332; color: #d6d6e0; border: 1px solid #34344a; }
QMenu::item { padding: 6px 26px; }
QMenu::item:selected { background: #3b82f6; color: white; }

QToolBar { background: #1d1d2b; border: none; border-bottom: 1px solid #2b2b40; spacing: 4px; padding: 4px; }
QToolBar::separator { width: 1px; background: #34344a; margin: 4px; }
QToolBar QToolButton { background: transparent; color: #d6d6e0; border: 1px solid transparent; border-radius: 6px; padding: 5px 10px; }
QToolBar QToolButton:hover { background: #2b2b40; border-color: #3a3a55; }
QToolBar QToolButton:checked { background: #3b82f6; border-color: #3b82f6; color: white; }

QPushButton { background: #2b2b40; border: 1px solid #3a3a55; border-radius: 6px; padding: 6px 12px; color: #d6d6e0; }
QPushButton:hover { background: #35354f; }
QPushButton:pressed { background: #3f3f5f; }
QPushButton:checked { background: #3b82f6; border-color: #3b82f6; color: white; }
QPushButton:disabled { color: #6b6b85; background: #232336; border-color: #2b2b40; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #12121c; border: 1px solid #34344a; border-radius: 5px; padding: 4px 8px; color: #e6e6f0; selection-background-color: #3b82f6; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: #3b82f6; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView { background: #232332; color: #d6d6e0; selection-background-color: #3b82f6; border: 1px solid #34344a; }

QListWidget, QTableWidget, QPlainTextEdit, QTextEdit, QTreeWidget { background: #11111a; border: 1px solid #2b2b40; border-radius: 6px; color: #d6d6e0; selection-background-color: #2f5fb3; selection-color: white; alternate-background-color: #151522; }
QListWidget::item { padding: 6px; border-radius: 4px; }
QListWidget::item:selected { background: #2f5fb3; color: white; }
QTableWidget::item { padding: 3px; }
QHeaderView::section { background: #232332; color: #b8b8cc; border: none; border-bottom: 1px solid #34344a; padding: 6px; }
QTableWidget QTableCornerButton::section { background: #232332; }

QGroupBox { border: 1px solid #303048; border-radius: 8px; margin-top: 14px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #8ab4f8; }

QLabel { color: #cfcfe0; background: transparent; }
QLabel#pageTitle { color: #f0f0ff; font-size: 18px; font-weight: bold; }
QLabel#metricValue { color: #8ab4f8; font-size: 16px; font-weight: bold; }

QProgressBar { background: #12121c; border: 1px solid #2b2b40; border-radius: 6px; text-align: center; color: #d6d6e0; }
QProgressBar::chunk { background: #3b82f6; border-radius: 5px; }

QTabWidget::pane { border: 1px solid #2b2b40; border-radius: 6px; top: -1px; background: #171722; }
QTabBar::tab { background: #1d1d2b; color: #b8b8cc; padding: 7px 16px; border: 1px solid #2b2b40; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }
QTabBar::tab:selected { background: #232332; color: #8ab4f8; }

QScrollBar:vertical { background: #14141e; width: 10px; }
QScrollBar::handle:vertical { background: #34344a; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #47476a; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollBar:horizontal { background: #14141e; height: 10px; }
QScrollBar::handle:horizontal { background: #34344a; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #47476a; }

QCheckBox, QRadioButton { color: #cfcfe0; spacing: 6px; background: transparent; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid #3a3a55; background: #12121c; }
QCheckBox::indicator:checked { background: #3b82f6; border-color: #3b82f6; }
QRadioButton::indicator { border-radius: 8px; }
QRadioButton::indicator:checked { background: #3b82f6; border-color: #3b82f6; }

QStatusBar { background: #16161f; color: #9a9ab0; }
QSplitter::handle { background: #2b2b40; }
QToolTip { background: #232332; color: #e6e6f0; border: 1px solid #3b82f6; }

QWidget#sideBar { background: #1a1a26; border-right: 1px solid #2b2b40; }
QListWidget#navList { background: transparent; border: none; font-size: 14px; }
QListWidget#navList::item { height: 44px; padding-left: 12px; margin: 2px 8px; border-radius: 6px; }
QListWidget#navList::item:selected { background: #2f5fb3; color: white; }
QListWidget#navList::item:hover { background: #26263a; }
QListWidget#navList::item:selected:hover { background: #2f5fb3; }

QLabel#logoTitle { color: #f0f0ff; font-size: 16px; font-weight: bold; }
QLabel#logoSub { color: #8a8aa8; font-size: 11px; }
)");
}

inline QString light()
{
    return QStringLiteral(R"(
* { font-family: "Microsoft YaHei", "Segoe UI"; }

QWidget { background: #f2f4f9; color: #2b2d42; font-size: 13px; }
QMainWindow, QDialog { background: #e8ebf3; }

QMenuBar { background: #ffffff; color: #2b2d42; padding: 2px; border-bottom: 1px solid #d7dbe6; }
QMenuBar::item { padding: 6px 12px; background: transparent; border-radius: 4px; }
QMenuBar::item:selected { background: #e3e9f7; }
QMenu { background: #ffffff; color: #2b2d42; border: 1px solid #d7dbe6; }
QMenu::item { padding: 6px 26px; }
QMenu::item:selected { background: #3b82f6; color: white; }

QToolBar { background: #ffffff; border: none; border-bottom: 1px solid #d7dbe6; spacing: 4px; padding: 4px; }
QToolBar::separator { width: 1px; background: #d7dbe6; margin: 4px; }
QToolBar QToolButton { background: transparent; color: #2b2d42; border: 1px solid transparent; border-radius: 6px; padding: 5px 10px; }
QToolBar QToolButton:hover { background: #e3e9f7; border-color: #d7dbe6; }
QToolBar QToolButton:checked { background: #3b82f6; border-color: #3b82f6; color: white; }

QPushButton { background: #ffffff; border: 1px solid #d7dbe6; border-radius: 6px; padding: 6px 12px; color: #2b2d42; }
QPushButton:hover { background: #eef2fb; }
QPushButton:pressed { background: #dfe6f5; }
QPushButton:checked { background: #3b82f6; border-color: #3b82f6; color: white; }
QPushButton:disabled { color: #a0a4b8; background: #f7f8fc; border-color: #e3e6ef; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #ffffff; border: 1px solid #d7dbe6; border-radius: 5px; padding: 4px 8px; color: #2b2d42; selection-background-color: #3b82f6; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: #3b82f6; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView { background: #ffffff; color: #2b2d42; selection-background-color: #3b82f6; border: 1px solid #d7dbe6; }

QListWidget, QTableWidget, QPlainTextEdit, QTextEdit, QTreeWidget { background: #ffffff; border: 1px solid #d7dbe6; border-radius: 6px; color: #2b2d42; selection-background-color: #cfe0ff; selection-color: #1b3a6b; alternate-background-color: #f7f9fd; }
QListWidget::item { padding: 6px; border-radius: 4px; }
QListWidget::item:selected { background: #cfe0ff; color: #1b3a6b; }
QTableWidget::item { padding: 3px; }
QHeaderView::section { background: #eef2fa; color: #4a4f66; border: none; border-bottom: 1px solid #d7dbe6; padding: 6px; }
QTableWidget QTableCornerButton::section { background: #eef2fa; }

QGroupBox { border: 1px solid #d7dbe6; border-radius: 8px; margin-top: 14px; padding-top: 10px; background: #ffffff; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #2f5fb3; }

QLabel { color: #2b2d42; background: transparent; }
QLabel#pageTitle { color: #1b3a6b; font-size: 18px; font-weight: bold; }
QLabel#metricValue { color: #2f5fb3; font-size: 16px; font-weight: bold; }

QProgressBar { background: #eef2fa; border: 1px solid #d7dbe6; border-radius: 6px; text-align: center; color: #2b2d42; }
QProgressBar::chunk { background: #3b82f6; border-radius: 5px; }

QTabWidget::pane { border: 1px solid #d7dbe6; border-radius: 6px; top: -1px; background: #ffffff; }
QTabBar::tab { background: #eef2fa; color: #4a4f66; padding: 7px 16px; border: 1px solid #d7dbe6; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }
QTabBar::tab:selected { background: #ffffff; color: #2f5fb3; }

QScrollBar:vertical { background: #eef2fa; width: 10px; }
QScrollBar::handle:vertical { background: #c3cbe0; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #a9b4d0; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollBar:horizontal { background: #eef2fa; height: 10px; }
QScrollBar::handle:horizontal { background: #c3cbe0; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #a9b4d0; }

QCheckBox, QRadioButton { color: #2b2d42; spacing: 6px; background: transparent; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid #c3cbe0; background: #ffffff; }
QCheckBox::indicator:checked { background: #3b82f6; border-color: #3b82f6; }
QRadioButton::indicator { border-radius: 8px; }
QRadioButton::indicator:checked { background: #3b82f6; border-color: #3b82f6; }

QStatusBar { background: #ffffff; color: #6b7086; border-top: 1px solid #d7dbe6; }
QSplitter::handle { background: #d7dbe6; }
QToolTip { background: #ffffff; color: #2b2d42; border: 1px solid #3b82f6; }

QWidget#sideBar { background: #ffffff; border-right: 1px solid #d7dbe6; }
QListWidget#navList { background: transparent; border: none; font-size: 14px; }
QListWidget#navList::item { height: 44px; padding-left: 12px; margin: 2px 8px; border-radius: 6px; }
QListWidget#navList::item:selected { background: #cfe0ff; color: #1b3a6b; }
QListWidget#navList::item:hover { background: #eef2fa; }
QListWidget#navList::item:selected:hover { background: #cfe0ff; }

QLabel#logoTitle { color: #1b3a6b; font-size: 16px; font-weight: bold; }
QLabel#logoSub { color: #8a8fa8; font-size: 11px; }
)");
}

} // namespace Theme
