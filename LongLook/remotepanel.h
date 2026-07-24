#ifndef REMOTEPANEL_H
#define REMOTEPANEL_H

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

#include "joystickreader.h"
#include "netclient.h"
#include "protocol.h"

/*
 * RemotePanel — 遥控面板（替代原“链路验证”页）
 *
 * 两种互斥的遥控方式，同一时刻只有一路在下发 FRAME_DRIVE，避免两路互相打架：
 *   界面遥控：滑条 + 方向按钮 + 持续下发
 *   手柄遥控：USB/XInput 手柄，60Hz 轮询
 *
 * 手柄键位与档位语义沿用《遥控手柄V2》(RemoteController) 原版，保证同一只手柄
 * 在两个上位机上手感一致；仅修正 V2 中扳机键索引 8/9 与 LS/RS 相撞的缺陷
 * （本工程 JoystickReader 已将扳机改为 14/15），并保留 LongLook 的方向键扩展。
 *
 * 手柄掉线时先下发停车再自动回退到界面遥控，对应 V2 的“模拟手柄模式”，不会失控。
 */
class RemotePanel : public QWidget
{
    Q_OBJECT
public:
    explicit RemotePanel(NetClient *net, QWidget *parent = nullptr);

    // 与《遥控手柄V2》config.h 同源，勿单方面改动，否则两个上位机手感会不一致
    static constexpr int kSpeedSlow   = 300;
    static constexpr int kSpeedMedium = 600;
    static constexpr int kSpeedFast   = 1000;
    static constexpr int kSteerRange  = 1000;   // STEERING_SENSITIVITY
    static constexpr int kPollMs      = 16;     // UPDATE_RATE 60Hz

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onWayChanged();                 // 遥控方式切换（界面/手柄）
    void onSpeedChanged(int value);
    void onSteerChanged(int value);
    void sendCurrent();
    void onContinuousTick();
    void onModeClicked(int mode);
    void onStop();
    void onEstop();
    void onSensor(const LL::SensorData &data);
    void onLog(const QString &message);
    void onConnected();
    void onDisconnected();
    void onRateTick();

    void onPadTick();
    void onPadConnected();
    void onPadDisconnected();
    void onPadAxis(int axis, double value);
    void onPadButton(int button, bool pressed);

private:
    QWidget *buildWayGroup();
    QWidget *buildModeGroup();
    QWidget *buildManualGroup();
    QWidget *buildPadGroup();
    QWidget *buildTelemetryGroup();
    QWidget *buildLogGroup();

    bool padActive() const;              // 当前是否处于手柄遥控
    void stopAllOutput();                // 停表 + 下发停车（切换/关闭时统一收口）
    void applyPadState();                // 由手柄状态算出 speed/steer 并按需下发
    void refreshPadLabels();
    int  currentSpeedMax() const;
    static QString modeName(quint8 mode);

    NetClient *m_net = nullptr;
    QLabel    *m_lblConn = nullptr;
    QLabel    *m_lblObsLock = nullptr;    // 避障锁横幅（锁定时才显示）
    bool       m_obsLockShown = false;    // 上一帧的锁状态，用于只在跳变时打日志

    // 遥控方式
    QRadioButton *m_radUi  = nullptr;
    QRadioButton *m_radPad = nullptr;

    // 界面遥控
    QGroupBox *m_boxUi = nullptr;
    QSlider   *m_sldSpeed = nullptr;
    QSlider   *m_sldSteer = nullptr;
    QLabel    *m_lblSpeed = nullptr;
    QLabel    *m_lblSteer = nullptr;
    QCheckBox *m_chkContinuous = nullptr;
    QTimer    *m_contTimer = nullptr;

    // 手柄遥控
    QGroupBox      *m_boxPad = nullptr;
    JoystickReader *m_pad = nullptr;
    QTimer         *m_padTimer = nullptr;
    QLabel         *m_lblPadState = nullptr;
    QLabel         *m_lblPadLevel = nullptr;
    QLabel         *m_lblPadStick = nullptr;
    QLabel         *m_lblPadOut = nullptr;
    double          m_padAxes[4] = {0, 0, 0, 0};
    bool            m_padButtons[16] = {};
    int             m_padLevel = 1;            // 0=低 1=中 2=高（V2 默认中档）
    qint16          m_padLastSpeed = 32767;    // 取不可能出现的值，保证首帧必发
    qint16          m_padLastSteer = 32767;
    int             m_padHeartbeat = 0;

    // 遥测
    QLabel *m_tSpeed = nullptr;
    QLabel *m_tMode = nullptr;
    QLabel *m_tDist = nullptr;
    QLabel *m_tLaser = nullptr;
    QLabel *m_tEnc1 = nullptr;
    QLabel *m_tEnc2 = nullptr;
    QLabel *m_tHz = nullptr;
    QLabel *m_tTs = nullptr;
    QLabel *m_tFault = nullptr;

    QTextEdit *m_log = nullptr;
    QTimer    *m_rateTimer = nullptr;
    int        m_sensorCount = 0;
};

#endif // REMOTEPANEL_H
