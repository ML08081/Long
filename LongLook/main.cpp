#include <QApplication>
#include <QEvent>
#include <QStringList>
#include <QStyleHints>
#include "mainwindow.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>

// QSS 不作用于系统标题栏；统一主窗口和遥控面板的原生窗口配色。
class DarkTitleBarFilter final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            auto *window = qobject_cast<QWidget *>(object);
            if (window && window->isWindow()
                && (window->windowType() == Qt::Window || window->windowType() == Qt::Dialog)) {
                const HWND hwnd = reinterpret_cast<HWND>(window->winId());
                const BOOL dark = TRUE;
                // 20 为 Windows 10 20H1 起的暗色标题栏属性，19 兼容更早的版本。
                if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
                    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));

                // Windows 11 支持指定边框、标题栏背景和文字颜色。
                const COLORREF border = RGB(0x2c, 0x36, 0x50);
                const COLORREF caption = RGB(0x16, 0x1b, 0x26);
                const COLORREF text = RGB(0xe8, 0xec, 0xf4);
                DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
                DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
                DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
            }
        }
        return QObject::eventFilter(object, event);
    }
};
#endif

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif
    app.setStyleSheet(QString::fromUtf8(kStyle));
#ifdef Q_OS_WIN
    DarkTitleBarFilter titleBarFilter;
    app.installEventFilter(&titleBarFilter);
#endif

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
