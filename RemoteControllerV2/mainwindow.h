#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSpinBox>
#include <QColor>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QStringListModel>

#include "joystickreader.h"
#include "bluetoothmanager.h"
#include "bledevicemanager.h"
#include "wifimanager.h"
#include "protocol.h"
#include "config.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void onJoystickConnected();
    void onJoystickDisconnected();
    void onSerialConnected();
    void onSerialDisconnected();
    void onWiFiConnected();
    void onWiFiDisconnected();
    void onAxisChanged(int axis, double value);
    void onButtonChanged(int button, bool pressed);
    void onTimerUpdate();

    // 蓝牙相关槽函数（经典 SPP）
    void onBTConnectButtonClicked();
    void onBTDisconnectButtonClicked();
    void onBTScanButtonClicked();
    void onBTClearLogClicked();
    void onBTError(const QString& error);

    // BLE 相关槽函数
    void onBLEScanButtonClicked();
    void onBLEConnectButtonClicked();
    void onBLEDisconnectButtonClicked();
    void onBLEClearLogClicked();
    void onBLESendATCommand();
    void onBLESetBaud115200();
    void onBLESetBaud9600();
    void onBLEDeviceDiscovered(const QString &name, const QString &address, int rssi);
    void onBLEScanFinished();
    void onBLEScanError(const QString &error);
    void onBLEConnected();
    void onBLEDisconnected();
    void onBLEConnectionError(const QString &error);
    void onBLEDeviceListClicked(QListWidgetItem *item);

    // WiFi相关槽函数
    void onWiFiConnectButtonClicked();
    void onWiFiDisconnectButtonClicked();
    void onWiFiPingButtonClicked();
    void onWiFiRefreshButtonClicked();
    void onWiFiPresetIPChanged(int index);
    void onWiFiClearLogClicked();
    void onWiFiError(const QString& error);

    void onSimForwardPressed();
    void onSimForwardReleased();
    void onSimBackwardPressed();
    void onSimBackwardReleased();
    void onSimLeftPressed();
    void onSimLeftReleased();
    void onSimRightPressed();
    void onSimRightReleased();
    void onSimSlowClicked();
    void onSimMediumClicked();
    void onSimFastClicked();

private:
    Ui::MainWindow *ui;
    JoystickReader* m_joystickReader;
    BluetoothManager* m_bluetoothManager;
    BleManager* m_bleManager;
    WiFiManager* m_wifiManager;
    Protocol* m_protocol;
    QTimer* m_updateTimer;

    // 状态变量
    bool m_joystickConnected;
    bool m_bluetoothConnected;
    bool m_wifiConnected;
    QString m_currentBTPort;

    // 当前使用的连接类型
    enum ConnectionType {
        None,
        Bluetooth,
        WiFi,
        BluetoothLE
    };
    ConnectionType m_currentConnection;

    // 摇杆数据
    double m_axisValues[4];
    bool m_buttonStates[16];

    // 控制数据
    int16_t m_speed;
    int16_t m_steering;
    int16_t m_lastSpeed;
    int16_t m_lastSteering;
    uint8_t m_controlMode;
    uint8_t m_lastControlMode;
    QSpinBox *m_speedSetting=nullptr;
    int m_speedLimit=500;
    bool m_stopLatched=false;
    void setSpeedLimit(int value);
    int m_speedLevel; // 0=低速, 1=中速, 2=高速

    // 模拟手柄模式
    bool m_simulatedMode;
    bool m_simForward;
    bool m_simBackward;
    bool m_simLeft;
    bool m_simRight;

    // 帧刷新计数器（用于心跳保持，运动时周期性重发）
    int m_heartbeatCounter;

    // 模拟控件UI指针
    QFrame* m_frameSimControl;
    QPushButton* m_btnSimForward;
    QPushButton* m_btnSimBackward;
    QPushButton* m_btnSimLeft;
    QPushButton* m_btnSimRight;
    QPushButton* m_btnSimSlow;
    QPushButton* m_btnSimMedium;
    QPushButton* m_btnSimFast;
    QLabel* m_lblSimModeStatus;

    void createSimulatedControlPanel();
    void updateSimulatedModeUI();
    void updateModeStatusDisplay();
    void updateSimulatedButtonDisplay();

    // BLE 相关状态
    bool m_bleConnected;
    QStringList m_bleDeviceNames;
    QStringList m_bleDeviceAddresses;

    // BLE 控件UI指针（在蓝牙标签页中构建）
    QFrame* m_frameBLEControl;
    QPushButton* m_btnBLEScan;
    QPushButton* m_btnBLEConnect;
    QPushButton* m_btnBLEDisconnect;
    QPushButton* m_btnBLEClearLog;
    QListWidget* m_listBLEDevices;
    QLabel* m_lblBLEStatus;
    QLineEdit* m_txtBLEATCmd;
    QTextEdit* m_txtBLEATLog;

    void createBLEControlPanel();
    void createTelemetryDisplay();

    // 更新UI显示
    void updateJoystickStatus();
    void updateBluetoothStatus();
    void updateBLEStatus();
    void updateWiFiStatus();
    void updateJoystickDisplay();
    void updateButtonDisplay();
    void updateControlData();
    void drawJoystickCanvas();
    void addBTLog(const QString& message);
    void addWiFiLog(const QString& message);
    void addBLELog(const QString& message);
    void sendData(const QByteArray& data);

    // 遥测解析与显示
    void processTelemetryBytes(const QByteArray& data);
    void updateTelemetryDisplay(const TelemetryData& td);

    // 遥测帧解析状态（字节级状态机）
    enum TeleState { TELE_IDLE, TELE_RECV };
    TeleState  m_teleState;
    QByteArray m_teleFrame;
    int        m_telemetryFrameCount;
    int        m_telemetryErrorCount;

    // 遥测显示UI指针
    QLabel* m_txtTeleSpeed;
    QLabel* m_txtTeleSteering;
    QLabel* m_txtTeleMode;
    QLabel* m_txtTeleFrames;
    QLabel* m_lblTeleStatus;
};

#endif // MAINWINDOW_H
