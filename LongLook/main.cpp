#include <QApplication>
#include <QStringList>
#include "mainwindow.h"

// 暗色监控大屏主题
static const char *kStyle = R"QSS(
* { font-family: "Microsoft YaHei", "Segoe UI", sans-serif; font-size: 13px; }
QWidget#Root, QMainWindow { background: #161b26; }
QScrollArea#LeftScroll, QScrollArea#LeftScroll > QWidget > QWidget { background: transparent; }

/* 顶栏 */
QFrame#TopBar { background: #1d2433; border: 1px solid #2c3650; border-radius: 8px; }
QLabel#Title { color: #5cc8ff; font-size: 18px; font-weight: bold; }
QFrame#TopBar QLabel { color: #aab4c8; }

/* 模块卡片 */
QFrame#Card { background: #1d2433; border: 1px solid #2c3650; border-radius: 8px; }
QLabel#CardTitle { color: #5cc8ff; font-size: 14px; font-weight: bold;
                   border-bottom: 1px solid #2c3650; padding-bottom: 5px; }
QLabel#Key   { color: #8a94a8; }
QLabel#Value { color: #e8ecf4; font-family: Consolas, "Courier New", monospace; font-size: 13px; }
QLabel#Hint  { color: #8a94a8; font-size: 12px; }

/* 连接状态 */
QLabel#StatusOk   { color: #43d17a; font-weight: bold; }
QLabel#StatusBad  { color: #ff6b6b; font-weight: bold; }
QLabel#StatusWarn { color: #ffb020; font-weight: bold; }
QLabel#Discovered { color: #5cc8ff; font-size: 12px; }

/* 输入框 / 下拉 */
QLineEdit, QComboBox {
    background: #11161f; color: #e8ecf4; border: 1px solid #2c3650;
    border-radius: 5px; padding: 4px 6px;
}
QLineEdit:disabled { color: #6b7384; }
QComboBox QAbstractItemView { background: #1d2433; color: #e8ecf4; selection-background-color: #2f6df0; }

/* 普通按钮 */
QPushButton {
    background: #2a3346; color: #e8ecf4;
    border: 1px solid #3a4760; border-radius: 6px; padding: 6px 12px;
}
QPushButton:hover { background: #34405a; }
QPushButton:disabled { background: #232a38; color: #5a6378; }
QPushButton#Primary { background: #2f6df0; border: none; font-weight: bold; }
QPushButton#Primary:hover { background: #3f7dff; }

/* 联动开关按钮 (checkable) */
QPushButton#Toggle { background: #232a38; border: 1px solid #3a4760; }
QPushButton#Toggle:checked { background: #2f8f4e; border: 1px solid #36b863; color: white; font-weight: bold; }
QPushButton#Toggle:hover { border: 1px solid #5cc8ff; }

/* 急停 */
QPushButton#Estop { background: #c62828; border: none; color: white; font-weight: bold; font-size: 15px; }
QPushButton#Estop:hover { background: #e53935; }

/* 风险徽章 */
QLabel#RiskBadge { background: #2e7d32; color: white; border-radius: 6px; padding: 4px 10px; font-weight: bold; }

/* 日志 */
QTextEdit#Log { background: #11161f; color: #cfd6e4; border: 1px solid #2c3650;
                border-radius: 6px; font-family: Consolas, monospace; font-size: 12px; }

QScrollBar:vertical { background: #11161f; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #3a4760; border-radius: 5px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)QSS";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LongLook");
    app.setStyleSheet(QString::fromUtf8(kStyle));

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.size() >= 2 && args.at(1).contains(':')) {
        const QString a = args.at(1);
        const QString ip = a.section(':', 0, 0);
        const quint16 port = quint16(a.section(':', 1, 1).toUInt());
        if (!ip.isEmpty() && port != 0)
            w.connectTo(ip, port);
    }

    return app.exec();
}
