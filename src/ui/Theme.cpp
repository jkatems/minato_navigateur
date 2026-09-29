#include "Theme.h"
#include <QApplication>
void applyTheme(bool light) {
    const QString bg = light ? "#f1f5f7" : "#101820", panel = light ? "#ffffff" : "#192630",
                  text = light ? "#172d38" : "#e7f1f3", muted = light ? "#526872" : "#9eb1bc",
                  border = light ? "#cddce2" : "#2d414d", accent = light ? "#16796d" : "#70debd";
    Q_INIT_RESOURCE(minato);
    qApp->setStyle("Fusion");
    qApp->setStyleSheet(QString(R"(
 QWidget { background:%1; color:%3; font-family:'Segoe UI','Noto Sans',sans-serif; font-size:13px; }
 QMainWindow, QDialog { background:%1; }
 QLineEdit,QComboBox { background:%2; border:1px solid %5; border-radius:9px; padding:10px; selection-background-color:%6; }
 QLineEdit:focus { border:1px solid %6; }
 QPushButton { background:%2; border:1px solid %5; border-radius:8px; padding:9px 13px; }
 QPushButton#toolButton { padding:0; font-size:18px; }
 QPushButton::menu-indicator { width:0; height:0; }
 QPushButton:hover { border-color:%6; color:%6; }
 QPushButton:disabled { color:%4; border-color:transparent; }
 QPushButton#primary { background:%6; color:%1; font-weight:600; }
 QTabWidget::pane { border:0; }
 QTabBar::tab { background:%1; color:%4; padding:12px 18px; min-width:120px; max-width:200px; border-bottom:2px solid transparent; }
 QTabBar::tab:selected { background:%2; color:%3; border-bottom:2px solid %6; }
 QTabBar::close-button { subcontrol-position:right; image:url(:/icons/close.svg); width:14px; height:14px; }
 QTabBar::close-button:hover { background:%5; border-radius:4px; }
 QProgressBar { border:0; background:%1; height:3px; }
 QProgressBar::chunk { background:%6; }
 QLabel#heading { font-size:38px; font-weight:650; padding-top:15px; }
 QLabel#subtitle { font-size:17px; color:%4; padding-bottom:16px; }
 QLabel#eyebrow { color:%6; font-size:12px; font-weight:600; letter-spacing:2px; }
 QLabel#muted { color:%4; padding-top:10px; }
 QLabel#errorBanner { background:#613a30; color:#fff3df; padding:12px; }
 QLineEdit#homeSearch { font-size:18px; padding:19px; margin-bottom:24px; }
 QTableWidget { background:%2; border:1px solid %5; border-radius:8px; gridline-color:%5; selection-background-color:%5; }
 QHeaderView::section { background:%2; color:%4; border:0; padding:12px; }
 QMenu { background:%2; border:1px solid %5; padding:6px; }
 QMenu::item { padding:8px 24px; }
 QMenu::item:selected { background:%5; }
 QStatusBar { color:%4; font-size:11px; }
 QScrollArea { border:0; }
 QScrollBar:vertical { background:%1; width:10px; margin:0; }
 QScrollBar::handle:vertical { background:%5; min-height:24px; border-radius:4px; }
 QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
 QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:none; }
 )")
                            .arg(bg, panel, text, muted, border, accent));
}
