#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>

#include "netclient.h"
#include "imageview.h"
#include "card.h"
#include "protocol.h"
#include "visionprocessor.h"

#include <QImage>
#include <QVector>

class RemotePanel;

/*
 * LongLook 主窗口 — 龙芯巡检系统 WiFi/TCP 监控前端
 *  顶栏：标题 + 连接(IP/端口/连接/断开/状态) + 风险等级徽章
 *  左列：模块卡片（环境感知 / 运动状态 / 视觉与风险）
 *  中：视频   右上：热成像   右下：执行与联动控制
 *  底：日志/报警
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void connectTo(const QString &ip, quint16 port);  // 命令行自动连接

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onConnected();
    void onDisconnected();
    void onLog(const QString &msg);
    void onSensorUpdated(const LL::SensorData &d);
    void onVideoFrame(const QImage &img);
    void onThermalFrame(const QImage &img, double minC, double maxC);
    void onTextFrame(const QString &text);
    void onRateTick();
    void onDiscovered(const QString &ip, quint16 port, const QString &version);  // UDP 自动发现开发板
    void onConnecting(int attempt, int maxAttempts);   // 连接尝试进行中 → 状态显示"连接中/重试中"
    void onPresetChanged(int index);                   // 选择预设地址(龙芯/本机)→ 填入 IP

    // 执行与联动控制
    void onFanToggled(bool on);
    void onBuzzerToggled(bool on);
    void onRelayToggled(bool on);
    void onLedToggled(bool on);
    void onModeChanged(int index);
    void onEstop();
    void onOpenRemotePanel();   // 打开遥控面板（界面遥控 / 手柄遥控）

    // 视觉识别（YOLO）
    void onVisionToggled(bool on);
    void onVisionDetections(const QVector<Detection> &dets);
    void onVisionStatus(bool running);

private:
    QWidget *buildTopBar();
    QWidget *buildLeftColumn();
    QWidget *buildVideoCard();
    QWidget *buildThermalCard();
    QWidget *buildControlCard();
    QWidget *buildLogCard();
    void     setConnectedUi(bool connected);
    void     setConnLabel(const QString &text, const char *objName);  // 连接状态标签(文本+配色)
    void     setToggle(QPushButton *b, bool on);     // 不触发信号地刷新按钮状态
    void     setRisk(quint8 level);
    static QString modeName(quint8 mode);
    void     refreshVideo();                                  // 用最新帧+检测框刷新视频显示
    static QImage overlayDetections(const QImage &src, const QVector<Detection> &dets);

    NetClient *m_net = nullptr;
    bool       m_userClosedLink = false;   // 用户主动断开→不被 UDP 自动发现重连
    bool       m_autoConnectPending = false; // 正在自动连接中→避免每次广播都重复发起
    QString    m_discoveredVer;            // 最近发现的龙芯版本(用于日志去重)

    // 顶栏
    QComboBox   *m_cmbPreset = nullptr;   // 预设开发板地址(龙芯/本机)
    QLineEdit   *m_editIp = nullptr;
    QLineEdit   *m_editPort = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDisconnect = nullptr;
    QPushButton *m_btnRemote = nullptr;
    QLabel      *m_lblConnStatus = nullptr;
    QLabel      *m_lblDiscovered = nullptr;   // 自动发现到的龙芯(IP/版本)
    QLabel      *m_lblRisk = nullptr;

    // 显示区
    ImageView *m_videoView = nullptr;
    ImageView *m_thermalView = nullptr;
    QLabel    *m_lblThermalRange = nullptr;

    // 环境感知
    QLabel *m_valTemp = nullptr;
    QLabel *m_valHumi = nullptr;
    QLabel *m_valGas = nullptr;
    QLabel *m_valDistance = nullptr;
    QLabel *m_valLaser = nullptr;
    // 运动状态
    QLabel *m_valEnc1 = nullptr;
    QLabel *m_valEnc2 = nullptr;
    QLabel *m_valSpeedL = nullptr;
    QLabel *m_valSpeedR = nullptr;
    QLabel *m_valServo = nullptr;
    QLabel *m_valVoltage = nullptr;
    QLabel *m_valMode = nullptr;
    QLabel *m_valFault = nullptr;
    // 视觉与风险
    QLabel *m_valRisk = nullptr;
    QLabel *m_valFlame = nullptr;
    QLabel *m_valSmoke = nullptr;
    QLabel *m_valGasAlarm = nullptr;
    // 链路健康
    QLabel *m_valLink     = nullptr;   // 连接 + F4 数据新鲜度（连接却冻结→红字告警）
    QLabel *m_valRemoteRx = nullptr;   // 龙芯 statusLine 的 RX 计数(tele/env, 带增量)
    QLabel *m_valSensorHz = nullptr;
    QLabel *m_valTimestamp = nullptr;

    // 执行与联动控制
    QPushButton *m_btnFan = nullptr;
    QPushButton *m_btnBuzzer = nullptr;
    QPushButton *m_btnRelay = nullptr;
    QPushButton *m_btnLed = nullptr;
    QComboBox   *m_cmbMode = nullptr;
    bool         m_modeSyncing = false;  // 抑制"模式回显"引发的命令回环
    QPushButton *m_btnEstop = nullptr;

    // 日志（双栏：本地 / 龙芯）
    QTextEdit *m_log = nullptr;         // 上位机本地日志
    QTextEdit *m_logRemote = nullptr;   // 龙芯端日志（经 TCP FRAME_TEXT 收到）

    // 速率统计 + 链路新鲜度
    QTimer *m_rateTimer = nullptr;
    int     m_sensorCount = 0;
    qint64  m_lastSensorMs = 0;          // 最近一次收到传感器帧的时刻(ms)；0=尚未收到
    // 龙芯 statusLine 的 RX 计数解析（判定"F4→龙芯"链路是否活跃，区别于"龙芯→PC"）
    quint32 m_remoteTele = 0, m_remoteEnv = 0;
    bool    m_remoteRxInit = false;

    // 遥控面板（懒创建，顶层窗口，复用同一 NetClient）
    RemotePanel *m_remotePanel = nullptr;

    // 视觉识别（YOLO）
    VisionProcessor    *m_vision = nullptr;
    QPushButton        *m_btnVision = nullptr;
    QLabel             *m_lblVision = nullptr;   // 状态/检测数
    QImage              m_lastFrame;             // 最近一帧原始视频（用于叠加）
    QVector<Detection>  m_lastDets;              // 最近一次检测结果

    // 热成像时域平滑：对相邻帧做像素 EMA 混合，低帧率下不跳变、观感更流畅
    QImage              m_thermalAccum;          // 累积混合图（与热像同尺寸）
};

#endif // MAINWINDOW_H
