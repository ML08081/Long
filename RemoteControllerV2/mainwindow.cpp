#include <QSignalBlocker>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QMessageBox>
#include <QDateTime>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("遥控手柄控制器");

    // 初始化变量
    m_joystickConnected = false;
    m_bluetoothConnected = false;
    m_wifiConnected = false;
    m_bleConnected = false;
    m_currentBTPort = SERIAL_PORT;
    m_currentConnection = None;
    m_speed = 0;
    m_steering = 0;
    m_lastSpeed = 0;
    m_lastSteering = 0;
    m_controlMode = MODE_STOP;
    m_lastControlMode = MODE_STOP;
    m_speedLevel = 1; setSpeedLimit(500);

    // 遥测解析状态初始化
    m_teleState = TELE_IDLE;
    m_telemetryFrameCount = 0;
    m_telemetryErrorCount = 0;

    // 模拟手柄初始化
    m_simulatedMode = true;
    m_simForward = false;
    m_simBackward = false;
    m_simLeft = false;
    m_simRight = false;
    m_heartbeatCounter = 0;
    m_frameSimControl = nullptr;
    m_btnSimForward = nullptr;
    m_btnSimBackward = nullptr;
    m_btnSimLeft = nullptr;
    m_btnSimRight = nullptr;
    m_btnSimSlow = nullptr;
    m_btnSimMedium = nullptr;
    m_btnSimFast = nullptr;
    m_lblSimModeStatus = nullptr;

    // BLE 控件初始化
    m_frameBLEControl = nullptr;
    m_btnBLEScan = nullptr;
    m_btnBLEConnect = nullptr;
    m_btnBLEDisconnect = nullptr;
    m_btnBLEClearLog = nullptr;
    m_listBLEDevices = nullptr;
    m_lblBLEStatus = nullptr;
    m_txtBLEATCmd = nullptr;
    m_txtBLEATLog = nullptr;

    // 初始化摇杆数据
    for (int i = 0; i < 4; i++) {
        m_axisValues[i] = 0.0;
    }

    // 初始化按钮状态
    for (int i = 0; i < 16; i++) {
        m_buttonStates[i] = false;
    }

    // 创建组件
    m_joystickReader = new JoystickReader(this);
    m_bluetoothManager = new BluetoothManager(this);
    m_bleManager = new BleManager(this);
    m_wifiManager = new WiFiManager(this);
    m_protocol = new Protocol();
    m_updateTimer = new QTimer(this);

    // 连接信号槽 - 手柄
    connect(m_joystickReader, &JoystickReader::connected, this, &MainWindow::onJoystickConnected);
    connect(m_joystickReader, &JoystickReader::disconnected, this, &MainWindow::onJoystickDisconnected);
    connect(m_joystickReader, &JoystickReader::axisChanged, this, &MainWindow::onAxisChanged);
    connect(m_joystickReader, &JoystickReader::buttonChanged, this, &MainWindow::onButtonChanged);

    // 连接信号槽 - 蓝牙
    connect(m_bluetoothManager, &BluetoothManager::connected, this, &MainWindow::onSerialConnected);
    connect(m_bluetoothManager, &BluetoothManager::disconnected, this, &MainWindow::onSerialDisconnected);
    connect(m_bluetoothManager, &BluetoothManager::errorOccurred, this, &MainWindow::onBTError);
    connect(m_bluetoothManager, &BluetoothManager::dataReceived, this, [this](const QByteArray& data) {
        addBTLog(QString("收到数据: %1 字节").arg(data.size()));
        processTelemetryBytes(data);
    });

    // 连接信号槽 - BLE
    connect(m_bleManager, &BleManager::deviceDiscovered,
            this, &MainWindow::onBLEDeviceDiscovered);
    connect(m_bleManager, &BleManager::scanFinished,
            this, &MainWindow::onBLEScanFinished);
    connect(m_bleManager, &BleManager::scanError,
            this, &MainWindow::onBLEScanError);
    connect(m_bleManager, &BleManager::connected,
            this, &MainWindow::onBLEConnected);
    connect(m_bleManager, &BleManager::disconnected,
            this, &MainWindow::onBLEDisconnected);
    connect(m_bleManager, &BleManager::connectionError,
            this, &MainWindow::onBLEConnectionError);
    connect(m_bleManager, &BleManager::statusMessage, this, [this](const QString &msg) {
        addBLELog(msg);
    });
    connect(m_bleManager, &BleManager::dataReceived, this, [this](const QByteArray& data) {
        addBLELog(QString("收到数据: %1 字节").arg(data.size()));
        processTelemetryBytes(data);
    });

    // 连接信号槽 - WiFi
    connect(m_wifiManager, &WiFiManager::connected, this, &MainWindow::onWiFiConnected);
    connect(m_wifiManager, &WiFiManager::disconnected, this, &MainWindow::onWiFiDisconnected);
    connect(m_wifiManager, &WiFiManager::errorOccurred, this, &MainWindow::onWiFiError);
    connect(m_wifiManager, &WiFiManager::dataReceived, this, [this](const QByteArray& data) {
        addWiFiLog(QString("收到数据: %1").arg(QString::fromLatin1(data.toHex(' '))));
        processTelemetryBytes(data);
    });

    // 连接信号槽 - 定时器
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onTimerUpdate);

    // 连接信号槽 - UI按钮
    connect(ui->btnBTConnect, &QPushButton::clicked, this, &MainWindow::onBTConnectButtonClicked);
    connect(ui->btnBTDisconnect, &QPushButton::clicked, this, &MainWindow::onBTDisconnectButtonClicked);
    connect(ui->btnBTScan, &QPushButton::clicked, this, &MainWindow::onBTScanButtonClicked);

    connect(ui->btnWiFiConnect, &QPushButton::clicked, this, &MainWindow::onWiFiConnectButtonClicked);
    connect(ui->btnWiFiDisconnect, &QPushButton::clicked, this, &MainWindow::onWiFiDisconnectButtonClicked);
    connect(ui->btnWiFiPing, &QPushButton::clicked, this, &MainWindow::onWiFiPingButtonClicked);
    connect(ui->btnWiFiRefresh, &QPushButton::clicked, this, &MainWindow::onWiFiRefreshButtonClicked);
    connect(ui->comboPresetIPs, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWiFiPresetIPChanged);

    // 初始化UI
    updateJoystickStatus();
    updateBluetoothStatus();
    updateWiFiStatus();

    // 设置默认波特率为115200（匹配STM32 USART3配置）
    ui->comboBaudRate->setCurrentText("115200");

    // 添加蓝牙日志清空按钮（插入到 label_BTLog 之后）
    QPushButton *btnBTClearLog = new QPushButton("🗑 清空");
    btnBTClearLog->setFixedSize(70, 22);
    btnBTClearLog->setStyleSheet("font-size: 11px;");
    QHBoxLayout *btHeaderLayout = new QHBoxLayout();
    btHeaderLayout->addWidget(ui->label_BTLog);
    btHeaderLayout->addStretch();
    btHeaderLayout->addWidget(btnBTClearLog);
    QVBoxLayout *btVLayout = qobject_cast<QVBoxLayout*>(ui->label_BTLog->parentWidget()->layout());
    if (btVLayout) {
        int idx = btVLayout->indexOf(ui->label_BTLog);
        btVLayout->removeWidget(ui->label_BTLog);
        btVLayout->insertLayout(idx, btHeaderLayout);
    }
    connect(btnBTClearLog, &QPushButton::clicked, this, &MainWindow::onBTClearLogClicked);

    // BLE 已弃用：改为串口(UART)直连（USB-TTL / 龙芯 2K0300 UART）。
    // 不再创建 BLE 面板，蓝牙标签页现仅保留纯串口连接控件。
    // （BleManager 代码暂保留但不接入 UI；龙芯构建时连同 QtBluetooth 一并移除。）

    // 添加WiFi日志清空按钮
    QPushButton *btnWiFiClearLog = new QPushButton("🗑 清空");
    btnWiFiClearLog->setFixedSize(70, 22);
    btnWiFiClearLog->setStyleSheet("font-size: 11px;");
    QHBoxLayout *wifiHeaderLayout = new QHBoxLayout();
    wifiHeaderLayout->addWidget(ui->label_WiFiLog);
    wifiHeaderLayout->addStretch();
    wifiHeaderLayout->addWidget(btnWiFiClearLog);
    QVBoxLayout *wifiVLayout = qobject_cast<QVBoxLayout*>(ui->label_WiFiLog->parentWidget()->layout());
    if (wifiVLayout) {
        int idx = wifiVLayout->indexOf(ui->label_WiFiLog);
        wifiVLayout->removeWidget(ui->label_WiFiLog);
        wifiVLayout->insertLayout(idx, wifiHeaderLayout);
    }
    connect(btnWiFiClearLog, &QPushButton::clicked, this, &MainWindow::onWiFiClearLogClicked);

    // 添加更多预设IP地址
    ui->comboPresetIPs->addItem("192.168.1.200:8080");
    ui->comboPresetIPs->addItem("10.0.0.1:8080");
    ui->comboPresetIPs->addItem("172.16.0.1:8080");

    // 创建模拟手柄控制面板
    createSimulatedControlPanel();

    // 创建遥测回传显示面板
    createTelemetryDisplay();

    // 添加初始日志
    addBTLog("串口模块已初始化（USB-TTL / 龙芯 UART 直连）");
    addWiFiLog("WiFi模块已初始化");

    // 自动扫描可用串口
    onBTScanButtonClicked();

    // 启动定时器
    m_updateTimer->start(1000 / UPDATE_RATE);

    // 更新模式状态显示
    updateModeStatusDisplay();
}

MainWindow::~MainWindow()
{
    m_updateTimer->stop();

    m_bluetoothManager->disconnectDevice();
    m_bleManager->disconnectFromDevice();
    m_wifiManager->disconnectFromHost();

    delete ui;
    delete m_joystickReader;
    delete m_bluetoothManager;
    delete m_bleManager;
    delete m_wifiManager;
    delete m_protocol;
    delete m_updateTimer;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_updateTimer->stop();

    if (m_bluetoothConnected) {
        m_bluetoothManager->disconnectDevice();
    }
    if (m_bleConnected) {
        m_bleManager->disconnectFromDevice();
    }
    if (m_wifiConnected) {
        m_wifiManager->disconnectFromHost();
    }

    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_simulatedMode) {
        if (event->isAutoRepeat()) {
            QMainWindow::keyPressEvent(event);
            return;
        }

        switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            m_simForward = true;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_simBackward = true;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            m_simLeft = true;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            m_simRight = true;
            break;
        case Qt::Key_1:
            m_speedLevel = 0; setSpeedLimit(100);
            updateButtonDisplay();
            break;
        case Qt::Key_2:
            m_speedLevel = 1; setSpeedLimit(500);
            updateButtonDisplay();
            break;
        case Qt::Key_3:
            m_speedLevel = 2; setSpeedLimit(1500);
            updateButtonDisplay();
            break;
        case Qt::Key_Space:
            m_stopLatched=true;
            m_simForward = false;
            m_simBackward = false;
            m_simLeft = false;
            m_simRight = false;
            m_speed = 0;
            m_steering = 0;
            m_controlMode = MODE_STOP;
            m_speedLevel = 1; setSpeedLimit(500);
            addBTLog("⚠️ 紧急停止 (Space键)");
            addWiFiLog("⚠️ 紧急停止 (Space键)");
            {
                QByteArray frame = m_protocol->packCommand(0, 0, MODE_STOP);
                sendData(frame);
            }
            updateControlData();
            updateButtonDisplay();
            break;
        default:
            QMainWindow::keyPressEvent(event);
            return;
        }
    } else {
        if (event->key() == Qt::Key_Space) {
            m_speed = 0;
            m_steering = 0;
            m_controlMode = MODE_STOP;
            m_speedLevel = 1; setSpeedLimit(500);
            m_buttonStates[7] = true;
            addBTLog("⚠️ 紧急停止 (Space键)");
            addWiFiLog("⚠️ 紧急停止 (Space键)");
            {
                QByteArray frame = m_protocol->packCommand(0, 0, MODE_STOP);
                sendData(frame);
            }
            updateControlData();
            updateButtonDisplay();
            return;
        }
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_simulatedMode) {
        if (event->isAutoRepeat()) {
            QMainWindow::keyReleaseEvent(event);
            return;
        }

        switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            m_simForward = false;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_simBackward = false;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            m_simLeft = false;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            m_simRight = false;
            break;
        default:
            QMainWindow::keyReleaseEvent(event);
            return;
        }
    } else {
        QMainWindow::keyReleaseEvent(event);
    }
}

void MainWindow::onJoystickConnected()
{
    m_joystickConnected = true;
    m_simulatedMode = false;
    updateJoystickStatus();
    updateSimulatedModeUI();
    updateModeStatusDisplay();
    statusBar()->showMessage("手柄已连接 - 真实手柄模式", 3000);
}

void MainWindow::onJoystickDisconnected()
{
    m_joystickConnected = false;
    m_simulatedMode = true;
    m_simForward = false;
    m_simBackward = false;
    m_simLeft = false;
    m_simRight = false;
    updateJoystickStatus();
    updateSimulatedModeUI();
    updateModeStatusDisplay();
    statusBar()->showMessage("手柄已断开 - 模拟手柄模式", 3000);
}

void MainWindow::onSerialConnected()
{
    m_bluetoothConnected = true;
    m_currentConnection = Bluetooth;
    updateBluetoothStatus();
    addBTLog(QString("已连接到串口: %1").arg(m_currentBTPort));
    statusBar()->showMessage(QString("串口已连接: %1").arg(m_currentBTPort), 5000);
}

void MainWindow::onSerialDisconnected()
{
    m_bluetoothConnected = false;
    if (m_currentConnection == Bluetooth) {
        m_currentConnection = None;
    }
    updateBluetoothStatus();
    addBTLog("蓝牙连接已断开");
    statusBar()->showMessage("蓝牙已断开", 3000);
}

void MainWindow::onWiFiConnected()
{
    m_wifiConnected = true;
    m_currentConnection = WiFi;
    updateWiFiStatus();
    QString connInfo = QString("%1:%2").arg(m_wifiManager->getConnectedIp()).arg(m_wifiManager->getConnectedPort());
    addWiFiLog(QString("已连接到 %1").arg(connInfo));
    statusBar()->showMessage(QString("WiFi已连接: %1").arg(connInfo), 5000);
}

void MainWindow::onWiFiDisconnected()
{
    m_wifiConnected = false;
    if (m_currentConnection == WiFi) {
        m_currentConnection = None;
    }
    updateWiFiStatus();
    addWiFiLog("WiFi连接已断开");
    statusBar()->showMessage("WiFi已断开", 3000);
}

void MainWindow::onAxisChanged(int axis, double value)
{
    if (axis >= 0 && axis < 4) {
        if (fabs(value) < DEAD_ZONE) {
            m_axisValues[axis] = 0.0;
        } else {
            m_axisValues[axis] = value;
        }
        updateJoystickDisplay();
    }
}

void MainWindow::onButtonChanged(int button, bool pressed)
{
    if (button >= 0 && button < 16) {
        m_buttonStates[button] = pressed;

        if (button == 7 && pressed) {
            m_stopLatched=true;
            m_speed = 0;
            m_steering = 0;
            m_controlMode = MODE_STOP;
        }

        if (button == 4 && pressed) {
            setSpeedLimit(qMax(100,m_speedLimit-100));
        }
        if (button == 5 && pressed) {
            setSpeedLimit(qMin(1500,m_speedLimit+100));
        }
        if (button == 6 && pressed) {
            m_speedLevel = 1; setSpeedLimit(500);
        }

        updateButtonDisplay();
    }
}

void MainWindow::onTimerUpdate()
{
    int speedMax=m_speedLimit;
    static int runtimeHeartbeat=0;
    if(++runtimeHeartbeat>=30) {
        runtimeHeartbeat=0;
        QByteArray cfg(12,0); cfg[0]=char(0xAA); cfg[1]=0x63; cfg[2]=8;
        cfg[5]=char((m_speedLimit-100)/100); cfg[6]=1;
        cfg[11]=char(m_protocol->calculateChecksum(reinterpret_cast<const uint8_t*>(cfg.constData()),11));
        sendData(cfg); // runtime setting never starts movement
    }

    if (m_simulatedMode) {
        int16_t simSpeed = 0;
        int16_t simSteering = 0;

        if (m_simForward) {
            simSpeed = speedMax;
        } else if (m_simBackward) {
            simSpeed = -speedMax;
        }

        if (m_simLeft) {
            simSteering = -STEERING_SENSITIVITY;
        } else if (m_simRight) {
            simSteering = STEERING_SENSITIVITY;
        }

        m_speed = simSpeed;
        m_steering = simSteering;

        if (simSpeed != 0 || simSteering != 0) {
            m_controlMode = MODE_BUTTON;
        } else {
            m_controlMode = MODE_STOP;
        }
    } else {
        m_joystickReader->update();

        double rawSpeed = -m_axisValues[1];
        double rawSteering = m_axisValues[0];

        m_speed = static_cast<int16_t>(rawSpeed * speedMax);
        m_steering = static_cast<int16_t>(rawSteering * STEERING_SENSITIVITY);

        if (m_buttonStates[0]) {
            m_speed = speedMax;
            m_controlMode = MODE_BUTTON;
        } else if (m_buttonStates[1]) {
            m_speed = -speedMax;
            m_controlMode = MODE_BUTTON;
        } else if (m_buttonStates[2]) {
            m_steering = -STEERING_SENSITIVITY;
            m_controlMode = MODE_BUTTON;
        } else if (m_buttonStates[3]) {
            m_steering = STEERING_SENSITIVITY;
            m_controlMode = MODE_BUTTON;
        } else if (fabs(rawSpeed) > DEAD_ZONE || fabs(rawSteering) > DEAD_ZONE) {
            m_controlMode = MODE_JOYSTICK;
        } else {
            m_controlMode = MODE_STOP;
        }
    }

    if(m_stopLatched) {
        // A held stick/button cannot cancel emergency stop; return to neutral first.
        bool neutral=m_speed==0 && m_steering==0 && !m_buttonStates[7];
        m_speed=m_steering=0; m_controlMode=MODE_STOP;
        if(neutral) m_stopLatched=false;
    }
    updateControlData();
    drawJoystickCanvas();
    updateSimulatedButtonDisplay();

    bool dataChanged = (m_speed != m_lastSpeed || m_steering != m_lastSteering || m_controlMode != m_lastControlMode);
    bool isMoving = (m_speed != 0 || m_steering != 0);

    m_heartbeatCounter++;

    // 心跳重发周期（UPDATE_RATE=60Hz）：
    //   运动时   3 tick(~50ms)  → 控制实时；
    //   空闲时  15 tick(~250ms) → 维持 BLE 链路活跃，避免空闲断连，
    //                             且 <F4 的 510ms 超时，使 F4 持续保持“蓝牙在线”。
    int hbPeriod = isMoving ? 3 : 15;
    bool forceRefresh = (m_heartbeatCounter >= hbPeriod);

    if (dataChanged || forceRefresh) {
        QByteArray frame = m_protocol->packCommand(m_speed, m_steering, 0);
        if (dataChanged) {
            ui->txtSendFrame->setText(QString("发送帧: %1").arg(m_protocol->frameToHexString(frame)));
        }
        sendData(frame);
        m_lastSpeed = m_speed;
        m_lastSteering = m_steering;
        m_lastControlMode = m_controlMode;
        m_heartbeatCounter = 0;
    }
}

void MainWindow::sendData(const QByteArray& data)
{
    // 向所有已连接的传输通道同时发送
    bool sentAny = false;

    if (m_bluetoothManager->isConnected()) {
        m_bluetoothManager->sendData(data);
        sentAny = true;
    }

    if (m_bleManager->isConnected()) {
        m_bleManager->sendData(data);
        sentAny = true;
    }

    if (m_wifiManager->isConnected()) {
        m_wifiManager->sendData(data);
        sentAny = true;
    }

    Q_UNUSED(sentAny);
}

void MainWindow::onBTConnectButtonClicked()
{
    QString displayName = ui->comboBluetoothPorts->currentText();
    if (displayName.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择串口！");
        return;
    }

    QString port = displayName.split(" ").first();

    qint32 baudRate = ui->comboBaudRate->currentText().toInt();

    addBTLog(QString("正在连接到串口 %1, 波特率: %2...").arg(port).arg(baudRate));
    if (m_bluetoothManager->connectDevice(port, baudRate)) {
        m_currentBTPort = displayName;
    } else {
        addBTLog("⚠ 打开串口失败，请确认：① 串口号正确且未被其他程序占用 ② USB-TTL/UART 驱动正常 ③ 波特率与下位机一致(115200)");
    }
}

void MainWindow::onBTDisconnectButtonClicked()
{
    m_bluetoothManager->disconnectDevice();
}

void MainWindow::onBTScanButtonClicked()
{
    ui->comboBluetoothPorts->clear();
    QStringList ports = m_bluetoothManager->listAvailableDevices();
    ui->comboBluetoothPorts->addItems(ports);

    if (!ports.isEmpty()) {
        ui->comboBluetoothPorts->setCurrentText(m_currentBTPort);
    }
    addBTLog(QString("扫描完成，发现 %1 个串口").arg(ports.size()));
}

void MainWindow::onWiFiConnectButtonClicked()
{
    QString ip = ui->txtWiFiIP->text();
    QString portStr = ui->txtWiFiPort->text();

    if (ip.isEmpty() || portStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入IP地址和端口！");
        return;
    }

    bool ok;
    quint16 port = portStr.toUInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, "警告", "端口号无效！");
        return;
    }

    addWiFiLog(QString("正在连接到 %1:%2...").arg(ip).arg(port));
    m_wifiManager->connectToHost(ip, port);
}

void MainWindow::onWiFiDisconnectButtonClicked()
{
    m_wifiManager->disconnectFromHost();
}

void MainWindow::onWiFiPingButtonClicked()
{
    QString ip = ui->txtWiFiIP->text();
    QString portStr = ui->txtWiFiPort->text();

    if (ip.isEmpty() || portStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入IP地址和端口！");
        return;
    }

    bool ok;
    quint16 port = portStr.toUInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, "警告", "端口号无效！");
        return;
    }

    addWiFiLog(QString("正在测试连接 %1:%2 (超时2秒)...").arg(ip).arg(port));

    WiFiManager *wm = m_wifiManager;
    QMetaObject::Connection *conn = new QMetaObject::Connection();
    *conn = connect(wm, &WiFiManager::pingResult, this,
        [this, conn](bool success, const QString &ip, quint16 port, int ms) {
            if (success) {
                addWiFiLog(QString("✅ 连接测试成功! %1:%2 响应时间 %3ms").arg(ip).arg(port).arg(ms));
            } else {
                addWiFiLog(QString("❌ 连接测试失败! %1:%2 无响应 (%3ms)").arg(ip).arg(port).arg(ms));
            }
            disconnect(*conn);
            delete conn;
        });

    m_wifiManager->pingHost(ip, port);
}

void MainWindow::onWiFiRefreshButtonClicked()
{
    addWiFiLog("刷新预设地址列表");
}

void MainWindow::onWiFiPresetIPChanged(int index)
{
    QString preset = ui->comboPresetIPs->itemText(index);
    QStringList parts = preset.split(":");
    if (parts.size() == 2) {
        ui->txtWiFiIP->setText(parts[0]);
        ui->txtWiFiPort->setText(parts[1]);
        addWiFiLog(QString("选择预设地址: %1").arg(preset));
    }
}

void MainWindow::updateJoystickStatus()
{
    if (m_joystickConnected) {
        ui->label_JoystickStatus->setText("手柄状态: ✅ 已连接");
        ui->label_JoystickStatus->setStyleSheet("color: green;");
    } else {
        ui->label_JoystickStatus->setText("手柄状态: ❌ 未连接");
        ui->label_JoystickStatus->setStyleSheet("color: red;");
    }
}

void MainWindow::updateBluetoothStatus()
{
    if (m_bluetoothConnected) {
        ui->lblBluetoothStatus->setText("✅ 已连接");
        ui->lblBluetoothStatus->setStyleSheet("color: green;");
        ui->btnBTConnect->setEnabled(false);
        ui->btnBTDisconnect->setEnabled(true);
        ui->btnBTScan->setEnabled(false);
        ui->comboBluetoothPorts->setEnabled(false);
    } else {
        ui->lblBluetoothStatus->setText("❌ 未连接");
        ui->lblBluetoothStatus->setStyleSheet("color: red;");
        ui->btnBTConnect->setEnabled(true);
        ui->btnBTDisconnect->setEnabled(false);
        ui->btnBTScan->setEnabled(true);
        ui->comboBluetoothPorts->setEnabled(true);
    }
}

void MainWindow::updateWiFiStatus()
{
    if (m_wifiConnected) {
        ui->lblWiFiStatus->setText("✅ 已连接");
        ui->lblWiFiStatus->setStyleSheet("color: green;");
        ui->btnWiFiConnect->setEnabled(false);
        ui->btnWiFiDisconnect->setEnabled(true);
        ui->txtWiFiIP->setEnabled(false);
        ui->txtWiFiPort->setEnabled(false);
        ui->comboPresetIPs->setEnabled(false);
    } else {
        ui->lblWiFiStatus->setText("❌ 未连接");
        ui->lblWiFiStatus->setStyleSheet("color: red;");
        ui->btnWiFiConnect->setEnabled(true);
        ui->btnWiFiDisconnect->setEnabled(false);
        ui->txtWiFiIP->setEnabled(true);
        ui->txtWiFiPort->setEnabled(true);
        ui->comboPresetIPs->setEnabled(true);
    }
}

void MainWindow::updateJoystickDisplay()
{
    ui->txtLeftX->setText(QString::number(m_axisValues[0], 'f', 2));
    ui->txtLeftY->setText(QString::number(m_axisValues[1], 'f', 2));
}

void MainWindow::updateButtonDisplay()
{
    QColor pressedColor(255, 100, 100);
    QColor releasedColor(211, 211, 211);

    ui->btnA->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[0] ? pressedColor.name() : releasedColor.name()));
    ui->btnB->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[1] ? pressedColor.name() : releasedColor.name()));
    ui->btnX->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[2] ? pressedColor.name() : releasedColor.name()));
    ui->btnY->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[3] ? pressedColor.name() : releasedColor.name()));
    ui->btnUp->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[12] ? pressedColor.name() : releasedColor.name()));
    ui->btnDown->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[13] ? pressedColor.name() : releasedColor.name()));
    ui->btnLeft->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[14] ? pressedColor.name() : releasedColor.name()));
    ui->btnRight->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[15] ? pressedColor.name() : releasedColor.name()));
    ui->btnLB->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[4] ? pressedColor.name() : releasedColor.name()));
    ui->btnRB->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[5] ? pressedColor.name() : releasedColor.name()));
    ui->btnStart->setStyleSheet(QString("background-color: %1;").arg(m_buttonStates[7] ? pressedColor.name() : releasedColor.name()));

    ui->lblSpeedSlow->setStyleSheet(m_speedLevel == 0 ? "color: red; font-weight: bold;" : "color: gray;");
    ui->lblSpeedMed->setStyleSheet(m_speedLevel == 1 ? "color: red; font-weight: bold;" : "color: gray;");
    ui->lblSpeedFast->setStyleSheet(m_speedLevel == 2 ? "color: red; font-weight: bold;" : "color: gray;");
}

void MainWindow::updateControlData()
{
    ui->txtSpeed->setText(QString::number(m_speed));
    ui->txtSteering->setText(QString::number(m_steering));

    QString modeStr;
    switch (m_controlMode) {
    case MODE_STOP: modeStr = "停止"; break;
    case MODE_JOYSTICK: modeStr = "摇杆模式"; break;
    case MODE_BUTTON: modeStr = "按钮模式"; break;
    default: modeStr = "未知";
    }
    ui->txtMode->setText(modeStr);

    QStringList levelNames = {"🐢 低速", "🚗 中速", "⚡ 高速"};
    QString levelStr = (m_speedLevel >= 0 && m_speedLevel <= 2) ? levelNames[m_speedLevel] : "未知";
    statusBar()->showMessage(QString("档位: %1  |  速度: %2  转向: %3  模式: %4")
                                 .arg(levelStr)
                                 .arg(m_speed)
                                 .arg(m_steering)
                                 .arg(modeStr));
}

void MainWindow::drawJoystickCanvas()
{
    QPixmap pixmap(150, 150);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(QPen(Qt::gray, 2));
    painter.drawEllipse(75, 75, 140, 140);

    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawLine(75, 10, 75, 140);
    painter.drawLine(10, 75, 140, 75);

    int x = 75 + static_cast<int>(m_axisValues[0] * 60);
    int y = 75 + static_cast<int>(m_axisValues[1] * 60);

    painter.setBrush(QBrush(QColor(0, 150, 255)));
    painter.setPen(QPen(Qt::blue, 2));
    painter.drawEllipse(x - 15, y - 15, 30, 30);

    ui->canvasJoystick->setPixmap(pixmap);
}

void MainWindow::addBTLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->txtBTLog->append(QString("[%1] %2").arg(timestamp).arg(message));
}

void MainWindow::addWiFiLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->txtWiFiLog->append(QString("[%1] %2").arg(timestamp).arg(message));
}

void MainWindow::onWiFiError(const QString& error)
{
    addWiFiLog("错误: " + error);
    m_wifiConnected = false;
    m_currentConnection = None;
    updateWiFiStatus();
}

void MainWindow::onBTClearLogClicked()
{
    ui->txtBTLog->clear();
    addBTLog("日志已清空");
}

void MainWindow::onWiFiClearLogClicked()
{
    ui->txtWiFiLog->clear();
    addWiFiLog("日志已清空");
}

void MainWindow::onBTError(const QString& error)
{
    addBTLog("错误: " + error);
    m_bluetoothConnected = false;
    m_currentConnection = None;
    updateBluetoothStatus();
}

void MainWindow::createSimulatedControlPanel()
{
    QVBoxLayout* tabLayout = qobject_cast<QVBoxLayout*>(ui->tabControl->layout());
    if (!tabLayout) return;

    m_frameSimControl = new QFrame(ui->tabControl);
    m_frameSimControl->setFrameShape(QFrame::StyledPanel);
    m_frameSimControl->setFrameShadow(QFrame::Raised);
    m_frameSimControl->setStyleSheet("QFrame { background-color: #f0f0f0; border-radius: 8px; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(m_frameSimControl);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    QLabel* titleLabel = new QLabel("🎯 模拟手柄控制", m_frameSimControl);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    m_lblSimModeStatus = new QLabel("当前模式: 模拟手柄", m_frameSimControl);
    m_lblSimModeStatus->setAlignment(Qt::AlignCenter);
    m_lblSimModeStatus->setStyleSheet("color: blue; font-weight: bold;");
    mainLayout->addWidget(m_lblSimModeStatus);

    QGridLayout* dirGrid = new QGridLayout();
    dirGrid->setHorizontalSpacing(8);
    dirGrid->setVerticalSpacing(8);

    m_btnSimForward = new QPushButton("▲前进(W)", m_frameSimControl);
    m_btnSimForward->setMinimumHeight(40);
    m_btnSimForward->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 6px; } QPushButton:pressed { background-color: #45a049; }");

    m_btnSimBackward = new QPushButton("▼后退(S)", m_frameSimControl);
    m_btnSimBackward->setMinimumHeight(40);
    m_btnSimBackward->setStyleSheet("QPushButton { background-color: #ff9800; color: white; font-weight: bold; border-radius: 6px; } QPushButton:pressed { background-color: #e68900; }");

    m_btnSimLeft = new QPushButton("◀左转(A)", m_frameSimControl);
    m_btnSimLeft->setMinimumHeight(40);
    m_btnSimLeft->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; border-radius: 6px; } QPushButton:pressed { background-color: #0b7dda; }");

    m_btnSimRight = new QPushButton("▶右转(D)", m_frameSimControl);
    m_btnSimRight->setMinimumHeight(40);
    m_btnSimRight->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; border-radius: 6px; } QPushButton:pressed { background-color: #0b7dda; }");

    dirGrid->addWidget(m_btnSimForward, 0, 1);
    dirGrid->addWidget(m_btnSimLeft, 1, 0);
    dirGrid->addWidget(m_btnSimRight, 1, 2);
    dirGrid->addWidget(m_btnSimBackward, 2, 1);

    mainLayout->addLayout(dirGrid);

    QHBoxLayout* speedLayout = new QHBoxLayout();
    speedLayout->addWidget(new QLabel("速度档位:", m_frameSimControl));

    m_btnSimSlow = new QPushButton("低速(1)", m_frameSimControl);
    m_btnSimSlow->setCheckable(true);
    m_btnSimSlow->setStyleSheet("QPushButton { background-color: #ddd; border-radius: 4px; } QPushButton:checked { background-color: #f44336; color: white; }");

    m_btnSimMedium = new QPushButton("中速(2)", m_frameSimControl);
    m_btnSimMedium->setCheckable(true);
    m_btnSimMedium->setChecked(true);
    m_btnSimMedium->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 4px; } QPushButton:checked { background-color: #f44336; color: white; }");

    m_btnSimFast = new QPushButton("高速(3)", m_frameSimControl);
    m_btnSimFast->setCheckable(true);
    m_btnSimFast->setStyleSheet("QPushButton { background-color: #ddd; border-radius: 4px; } QPushButton:checked { background-color: #f44336; color: white; }");

    speedLayout->addWidget(m_btnSimSlow);
    speedLayout->addWidget(m_btnSimMedium);
    speedLayout->addWidget(m_btnSimFast);
    m_speedSetting=new QSpinBox(m_frameSimControl);
    m_speedSetting->setRange(100,1500); m_speedSetting->setSingleStep(100);
    m_speedSetting->setValue(m_speedLimit); m_speedSetting->setKeyboardTracking(false);
    m_speedSetting->setToolTip("所有遥控运动共用：100～1500，步进100，默认500");
    speedLayout->addWidget(m_speedSetting);
    connect(m_speedSetting,QOverload<int>::of(&QSpinBox::valueChanged),this,&MainWindow::setSpeedLimit);
    speedLayout->addStretch();

    QButtonGroup* speedGroup = new QButtonGroup(m_frameSimControl);
    speedGroup->addButton(m_btnSimSlow, 0);
    speedGroup->addButton(m_btnSimMedium, 1);
    speedGroup->addButton(m_btnSimFast, 2);
    m_btnSimMedium->setChecked(true);

    mainLayout->addLayout(speedLayout);

    QPushButton* btnSimStop = new QPushButton("⚠ 紧急停止 (Space)", m_frameSimControl);
    btnSimStop->setMinimumHeight(35);
    btnSimStop->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 13px; border-radius: 6px; } QPushButton:pressed { background-color: #d32f2f; }");
    mainLayout->addWidget(btnSimStop);

    QLabel* hintLabel = new QLabel("💡 提示: 可使用键盘 WASD / 方向键 / 1-3 / Space", m_frameSimControl);
    hintLabel->setStyleSheet("color: #666; font-size: 11px;");
    hintLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(hintLabel);

    int insertIdx = tabLayout->indexOf(ui->txtSendFrame);
    tabLayout->insertWidget(insertIdx, m_frameSimControl);

    connect(m_btnSimForward, &QPushButton::pressed, this, &MainWindow::onSimForwardPressed);
    connect(m_btnSimForward, &QPushButton::released, this, &MainWindow::onSimForwardReleased);
    connect(m_btnSimBackward, &QPushButton::pressed, this, &MainWindow::onSimBackwardPressed);
    connect(m_btnSimBackward, &QPushButton::released, this, &MainWindow::onSimBackwardReleased);
    connect(m_btnSimLeft, &QPushButton::pressed, this, &MainWindow::onSimLeftPressed);
    connect(m_btnSimLeft, &QPushButton::released, this, &MainWindow::onSimLeftReleased);
    connect(m_btnSimRight, &QPushButton::pressed, this, &MainWindow::onSimRightPressed);
    connect(m_btnSimRight, &QPushButton::released, this, &MainWindow::onSimRightReleased);
    connect(m_btnSimSlow, &QPushButton::clicked, this, &MainWindow::onSimSlowClicked);
    connect(m_btnSimMedium, &QPushButton::clicked, this, &MainWindow::onSimMediumClicked);
    connect(m_btnSimFast, &QPushButton::clicked, this, &MainWindow::onSimFastClicked);
    connect(btnSimStop, &QPushButton::clicked, this, [this]() {
        m_stopLatched=true;
        m_simForward = false;
        m_simBackward = false;
        m_simLeft = false;
        m_simRight = false;
        m_speed = 0;
        m_steering = 0;
        m_controlMode = MODE_STOP;
        m_speedLevel = 1; setSpeedLimit(500);
        m_btnSimMedium->setChecked(true);
        addBTLog("⚠️ 紧急停止");
        addWiFiLog("⚠️ 紧急停止");
        QByteArray frame = m_protocol->packCommand(0, 0, MODE_STOP);
        sendData(frame);
        updateControlData();
        updateButtonDisplay();
    });
}

// ═══════════════════════════════════════════════════════════════
//  📡 STM32 遥测回传显示面板
// ═══════════════════════════════════════════════════════════════
void MainWindow::createTelemetryDisplay()
{
    QVBoxLayout* tabLayout = qobject_cast<QVBoxLayout*>(ui->tabControl->layout());
    if (!tabLayout) return;

    QFrame* frame = new QFrame(ui->tabControl);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setFrameShadow(QFrame::Raised);
    frame->setStyleSheet("QFrame { background-color: #f5f5f0; border: 1px solid #999; border-radius: 8px; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(frame);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel("📡 STM32 遥测回传", frame);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; background: transparent;");
    mainLayout->addWidget(titleLabel);

    m_lblTeleStatus = new QLabel("⏳ 等待遥测数据...", frame);
    m_lblTeleStatus->setStyleSheet("color: #999; font-weight: bold; background: transparent;");
    mainLayout->addWidget(m_lblTeleStatus);

    QHBoxLayout* dataLayout = new QHBoxLayout();
    dataLayout->setSpacing(16);

    auto makeTeleLabel = [frame](const QString& label, const QString& initial, QLabel*& valueOut) -> QWidget* {
        QWidget* w = new QWidget(frame);
        w->setStyleSheet("background: transparent;");
        QHBoxLayout* lay = new QHBoxLayout(w);
        lay->setContentsMargins(0, 0, 0, 0);
        QLabel* lbl = new QLabel(label, w);
        lbl->setStyleSheet("font-weight: bold; color: #555; background: transparent;");
        valueOut = new QLabel(initial, w);
        valueOut->setStyleSheet("color: #222; font-weight: bold; background: transparent; font-size: 13px;");
        lay->addWidget(lbl);
        lay->addWidget(valueOut);
        return w;
    };

    dataLayout->addWidget(makeTeleLabel("指令（脉冲/s）:", "---", m_txtTeleSpeed));
    dataLayout->addWidget(makeTeleLabel("转向:", "---", m_txtTeleSteering));
    dataLayout->addWidget(makeTeleLabel("模式:", "---", m_txtTeleMode));
    dataLayout->addWidget(makeTeleLabel("帧数:", "0", m_txtTeleFrames));
    dataLayout->addStretch();

    mainLayout->addLayout(dataLayout);

    int insertIdx = tabLayout->indexOf(ui->txtSendFrame);
    tabLayout->insertWidget(insertIdx + 1, frame);
}

void MainWindow::processTelemetryBytes(const QByteArray& data)
{
    for (int i = 0; i < data.size(); i++) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        switch (m_teleState) {
        case TELE_IDLE:
            if (byte == FRAME_HEADER) {
                m_teleFrame.clear();
                m_teleFrame.append(static_cast<char>(byte));
                m_teleState = TELE_RECV;
            }
            break;

        case TELE_RECV:
            m_teleFrame.append(static_cast<char>(byte));
            if (m_teleFrame.size() == FRAME_LENGTH) {
                TelemetryData td;
                if (Protocol::unpackFrame(m_teleFrame, td)) {
                    m_telemetryFrameCount++;
                    updateTelemetryDisplay(td);
                } else {
                    m_telemetryErrorCount++;
                }
                m_teleState = TELE_IDLE;
            }
            break;
        }
    }
}

void MainWindow::updateTelemetryDisplay(const TelemetryData& td)
{
    if (!m_txtTeleSpeed) return;

    m_txtTeleSpeed->setText(QString::number(td.speed));
    m_txtTeleSpeed->setStyleSheet(
        td.speed != 0
            ? "color: #d32f2f; font-weight: bold; background: transparent; font-size: 13px;"
            : "color: #222; font-weight: bold; background: transparent; font-size: 13px;"
    );

    QString steerDir;
    if (td.steering < -50)      steerDir = " ←左";
    else if (td.steering > 50) steerDir = " 右→";
    else                        steerDir = " 直行";
    m_txtTeleSteering->setText(QString::number(td.steering) + steerDir);

    QString modeStr;
    switch (td.mode) {
    case 0:  modeStr = "手动/停止"; break;
    case 1:  modeStr = "自动";      break;
    case 2:  modeStr = "避障";      break;
    case 3:  modeStr = "巡航";      break;
    default: modeStr = QString("未知(0x%1)").arg(td.mode, 2, 16, QChar('0')); break;
    }
    m_txtTeleMode->setText(modeStr);

    m_txtTeleFrames->setText(QString::number(m_telemetryFrameCount));

    m_lblTeleStatus->setText("✅ 遥测活跃中");
    m_lblTeleStatus->setStyleSheet("color: #2e7d32; font-weight: bold; background: transparent;");
}

// ═══════════════════════════════════════════════════════════════
//  BLE 控制面板 UI
// ═══════════════════════════════════════════════════════════════
void MainWindow::createBLEControlPanel()
{
    QWidget *btTab = ui->tabWidget->widget(1);
    if (!btTab) return;

    QVBoxLayout *tabLayout = qobject_cast<QVBoxLayout*>(btTab->layout());
    if (!tabLayout) return;

    m_frameBLEControl = new QFrame(btTab);
    m_frameBLEControl->setFrameShape(QFrame::StyledPanel);
    m_frameBLEControl->setFrameShadow(QFrame::Raised);
    m_frameBLEControl->setStyleSheet(
        "QFrame#frameBLEControl { background-color: #e8f4e8; border: 1px solid #4CAF50; border-radius: 8px; }"
    );
    m_frameBLEControl->setObjectName("frameBLEControl");

    QVBoxLayout *bleLayout = new QVBoxLayout(m_frameBLEControl);
    bleLayout->setContentsMargins(10, 10, 10, 10);
    bleLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel("📡 BLE 设备扫描（JDY-x3）", m_frameBLEControl);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #2E7D32; background: transparent;");
    bleLayout->addWidget(titleLabel);

    m_listBLEDevices = new QListWidget(m_frameBLEControl);
    m_listBLEDevices->setMinimumHeight(100);
    m_listBLEDevices->setMaximumHeight(150);
    m_listBLEDevices->setStyleSheet("background: white; border: 1px solid #ccc; border-radius: 4px;");
    bleLayout->addWidget(m_listBLEDevices);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_btnBLEScan = new QPushButton("🔍 扫描 BLE", m_frameBLEControl);
    m_btnBLEScan->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 4px; padding: 4px 12px; }");

    m_btnBLEConnect = new QPushButton("🔗 连接", m_frameBLEControl);
    m_btnBLEConnect->setEnabled(false);
    m_btnBLEConnect->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 4px; padding: 4px 12px; } QPushButton:disabled { background-color: #ccc; }");

    m_btnBLEDisconnect = new QPushButton("📴 断开", m_frameBLEControl);
    m_btnBLEDisconnect->setEnabled(false);
    m_btnBLEDisconnect->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 4px; padding: 4px 12px; } QPushButton:disabled { background-color: #ccc; }");

    btnLayout->addWidget(m_btnBLEScan);
    btnLayout->addWidget(m_btnBLEConnect);
    btnLayout->addWidget(m_btnBLEDisconnect);
    btnLayout->addStretch();
    bleLayout->addLayout(btnLayout);

    m_lblBLEStatus = new QLabel("BLE 状态: ❌ 未连接", m_frameBLEControl);
    m_lblBLEStatus->setStyleSheet("font-weight: bold; color: #f44336; background: transparent;");
    bleLayout->addWidget(m_lblBLEStatus);

    // ── AT 命令配置区 ──
    QLabel *lblAT = new QLabel("⚙️ AT 指令配置（JDY-x3）", m_frameBLEControl);
    lblAT->setStyleSheet("font-weight: bold; color: #555; background: transparent; font-size: 11px;");
    bleLayout->addWidget(lblAT);

    QHBoxLayout *atBtnLayout = new QHBoxLayout();
    QPushButton *btnBaud115200 = new QPushButton("波特率 115200", m_frameBLEControl);
    btnBaud115200->setObjectName("btnBaud115200");
    btnBaud115200->setStyleSheet("QPushButton { background-color: #FF9800; color: white; border-radius: 4px; padding: 4px 8px; font-size: 11px; }");
    QPushButton *btnBaud9600 = new QPushButton("波特率 9600", m_frameBLEControl);
    btnBaud9600->setObjectName("btnBaud9600");
    btnBaud9600->setStyleSheet("QPushButton { background-color: #795548; color: white; border-radius: 4px; padding: 4px 8px; font-size: 11px; }");
    QPushButton *btnATQuery = new QPushButton("查询版本", m_frameBLEControl);
    btnATQuery->setObjectName("btnATQuery");
    btnATQuery->setStyleSheet("QPushButton { background-color: #607D8B; color: white; border-radius: 4px; padding: 4px 8px; font-size: 11px; }");
    atBtnLayout->addWidget(btnBaud115200);
    atBtnLayout->addWidget(btnBaud9600);
    atBtnLayout->addWidget(btnATQuery);
    atBtnLayout->addStretch();
    bleLayout->addLayout(atBtnLayout);

    QHBoxLayout *atCustomLayout = new QHBoxLayout();
    m_txtBLEATCmd = new QLineEdit(m_frameBLEControl);
    m_txtBLEATCmd->setPlaceholderText("输入自定义 AT 指令，如 AT+BAUD8");
    m_txtBLEATCmd->setStyleSheet("border: 1px solid #ccc; border-radius: 4px; padding: 3px;");
    QPushButton *btnATSend = new QPushButton("发送 AT", m_frameBLEControl);
    btnATSend->setObjectName("btnATSend");
    btnATSend->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 4px; padding: 4px 12px; }");
    atCustomLayout->addWidget(m_txtBLEATCmd);
    atCustomLayout->addWidget(btnATSend);
    bleLayout->addLayout(atCustomLayout);

    m_txtBLEATLog = new QTextEdit(m_frameBLEControl);
    m_txtBLEATLog->setObjectName("txtBLEATLog");
    m_txtBLEATLog->setReadOnly(true);
    m_txtBLEATLog->setMaximumHeight(60);
    m_txtBLEATLog->setStyleSheet("background: #fafafa; border: 1px solid #ddd; border-radius: 4px; font-size: 10px;");
    bleLayout->addWidget(m_txtBLEATLog);

    connect(btnBaud115200, &QPushButton::clicked, this, &MainWindow::onBLESetBaud115200);
    connect(btnBaud9600, &QPushButton::clicked, this, &MainWindow::onBLESetBaud9600);
    connect(btnATQuery, &QPushButton::clicked, this, [this]() {
        m_bleManager->sendATCommand(QByteArray("AT+VERSION\r\n"));
        addBLELog("📤 AT: AT+VERSION");
    });
    connect(btnATSend, &QPushButton::clicked, this, &MainWindow::onBLESendATCommand);
    connect(m_txtBLEATCmd, &QLineEdit::returnPressed, this, &MainWindow::onBLESendATCommand);

    QLabel *lblLog = new QLabel("📝 BLE 日志", m_frameBLEControl);
    lblLog->setObjectName("label_BLELog");
    lblLog->setStyleSheet("font-weight: bold; background: transparent;");
    bleLayout->addWidget(lblLog);

    QTextEdit *txtLog = new QTextEdit(m_frameBLEControl);
    txtLog->setObjectName("txtBLELog");
    txtLog->setReadOnly(true);
    txtLog->setMinimumHeight(100);
    txtLog->setMaximumHeight(150);
    txtLog->setStyleSheet("background: white; border: 1px solid #ccc; border-radius: 4px;");
    bleLayout->addWidget(txtLog);

    QFrame *btLogFrame = btTab->findChild<QFrame*>("frame_BTLog");
    if (btLogFrame) {
        int insertIdx = tabLayout->indexOf(btLogFrame);
        if (insertIdx >= 0) {
            tabLayout->insertWidget(insertIdx, m_frameBLEControl);
        } else {
            tabLayout->addWidget(m_frameBLEControl);
        }
    } else {
        tabLayout->addWidget(m_frameBLEControl);
    }

    connect(m_btnBLEScan, &QPushButton::clicked, this, &MainWindow::onBLEScanButtonClicked);
    connect(m_btnBLEConnect, &QPushButton::clicked, this, &MainWindow::onBLEConnectButtonClicked);
    connect(m_btnBLEDisconnect, &QPushButton::clicked, this, &MainWindow::onBLEDisconnectButtonClicked);
    connect(m_listBLEDevices, &QListWidget::itemClicked, this, &MainWindow::onBLEDeviceListClicked);

    addBLELog("BLE 模块已初始化");
}

// ═══════════════════════════════════════════════════════════════
//  BLE 槽函数实现
// ═══════════════════════════════════════════════════════════════

void MainWindow::onBLEScanButtonClicked()
{
    m_bleDeviceNames.clear();
    m_bleDeviceAddresses.clear();
    m_listBLEDevices->clear();
    m_btnBLEConnect->setEnabled(false);

    m_bleManager->startScan(BLE_SCAN_TIMEOUT);
}

void MainWindow::onBLEConnectButtonClicked()
{
    int row = m_listBLEDevices->currentRow();
    if (row < 0 || row >= m_bleDeviceAddresses.size()) {
        addBLELog("⚠️ 请先选择要连接的设备");
        return;
    }

    QString address = m_bleDeviceAddresses[row];
    QString name = (row < m_bleDeviceNames.size()) ? m_bleDeviceNames[row] : address;

    addBLELog(QString("🔗 正在连接到 %1 (%2)...").arg(name, address));
    m_btnBLEScan->setEnabled(false);
    m_btnBLEConnect->setEnabled(false);
    m_listBLEDevices->setEnabled(false);

    m_bleManager->connectToDevice(address);
}

void MainWindow::onBLEDisconnectButtonClicked()
{
    m_bleManager->disconnectFromDevice();
}

void MainWindow::onBLEClearLogClicked()
{
    QTextEdit *logEdit = findChild<QTextEdit*>("txtBLELog");
    if (logEdit) {
        logEdit->clear();
    }
    addBLELog("日志已清空");
}

// ═══════════════════════════════════════════════════════════════
//  BLE AT 指令配置
// ═══════════════════════════════════════════════════════════════
void MainWindow::onBLESendATCommand()
{
    QString cmd = m_txtBLEATCmd->text().trimmed();
    if (cmd.isEmpty()) {
        addBLELog("⚠️ 请输入 AT 指令");
        return;
    }
    // 确保以 \r\n 结尾
    QByteArray atCmd;
    if (!cmd.endsWith("\r\n"))
        atCmd = cmd.toLatin1() + "\r\n";
    else
        atCmd = cmd.toLatin1();

    m_txtBLEATLog->append(QString("📤 %1").arg(cmd));
    m_bleManager->sendATCommand(atCmd);
    addBLELog(QString("📤 AT 指令: %1").arg(cmd));
}

void MainWindow::onBLESetBaud115200()
{
    m_bleManager->sendATCommand(QByteArray("AT+BAUD8\r\n"));
    m_txtBLEATLog->append("📤 AT+BAUD8 → 设置 UART 波特率为 115200");
    addBLELog("📤 AT+BAUD8 → 设置 UART 波特率为 115200");
}

void MainWindow::onBLESetBaud9600()
{
    m_bleManager->sendATCommand(QByteArray("AT+BAUD4\r\n"));
    m_txtBLEATLog->append("📤 AT+BAUD4 → 设置 UART 波特率为 9600");
    addBLELog("📤 AT+BAUD4 → 设置 UART 波特率为 9600");
}

void MainWindow::onBLEDeviceDiscovered(const QString &name, const QString &address, int rssi)
{
    if (m_bleDeviceAddresses.contains(address)) return;

    m_bleDeviceNames << name;
    m_bleDeviceAddresses << address;

    QString rssiStr;
    QString rssiNote;
    if (rssi >= -50) {
        rssiStr = "📶 强";
    } else if (rssi >= -70) {
        rssiStr = "📶 中";
    } else if (rssi >= -90) {
        rssiStr = "📶 弱";
    } else {
        rssiStr = "📡 极弱";
        rssiNote = " ⚠️ 信号极弱，可能是缓存设备";
    }

    QString display = QString("%1 [%2] %3 dBm%4")
                          .arg(name, rssiStr)
                          .arg(rssi)
                          .arg(rssiNote);
    m_listBLEDevices->addItem(display);

    if (name.contains("JDY", Qt::CaseInsensitive) ||
        name.contains("YYY", Qt::CaseInsensitive) ||
        name.contains("HC-", Qt::CaseInsensitive) ||
        name.contains("HM-", Qt::CaseInsensitive)) {
        QString confidence = (rssi >= -80) ? "✅" : "⚠️ 信号弱";
        addBLELog(QString("%1 可能是目标模块: %2 (%3) %4 dBm")
                      .arg(confidence, name, address).arg(rssi));
    }
}

void MainWindow::onBLEScanFinished()
{
    m_btnBLEScan->setEnabled(true);
    if (m_listBLEDevices->count() == 0) {
        addBLELog("⚠️ 未发现任何 BLE 设备");
    } else {
        addBLELog(QString("🔍 扫描完成，发现 %1 个设备").arg(m_listBLEDevices->count()));
    }
}

void MainWindow::onBLEScanError(const QString &error)
{
    m_btnBLEScan->setEnabled(true);
    addBLELog(QString("❌ 扫描错误: %1").arg(error));
}

void MainWindow::onBLEConnected()
{
    m_bleConnected = true;
    m_currentConnection = BluetoothLE;
    updateBLEStatus();
    addBLELog("✅ BLE 连接成功!");
    m_btnBLEScan->setEnabled(false);
    m_btnBLEConnect->setEnabled(false);
    m_btnBLEDisconnect->setEnabled(true);
    m_listBLEDevices->setEnabled(false);
    statusBar()->showMessage(
        QString("BLE 已连接: %1").arg(m_bleManager->getConnectedDeviceName()), 5000);
}

void MainWindow::onBLEDisconnected()
{
    m_bleConnected = false;
    if (m_currentConnection == BluetoothLE) {
        m_currentConnection = None;
    }
    updateBLEStatus();
    addBLELog("📴 BLE 连接已断开");
    m_btnBLEScan->setEnabled(true);
    m_btnBLEConnect->setEnabled(false);
    m_btnBLEDisconnect->setEnabled(false);
    m_listBLEDevices->setEnabled(true);
    statusBar()->showMessage("BLE 已断开", 3000);
}

void MainWindow::onBLEConnectionError(const QString &error)
{
    addBLELog(QString("❌ 连接失败: %1").arg(error));
    m_btnBLEScan->setEnabled(true);
    m_btnBLEDisconnect->setEnabled(false);
    m_listBLEDevices->setEnabled(true);
    updateBLEStatus();
}

void MainWindow::onBLEDeviceListClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    m_btnBLEConnect->setEnabled(true);
}

void MainWindow::updateBLEStatus()
{
    if (m_lblBLEStatus) {
        if (m_bleConnected) {
            QString name = m_bleManager->getConnectedDeviceName();
            m_lblBLEStatus->setText(QString("BLE 状态: ✅ 已连接 (%1)").arg(name));
            m_lblBLEStatus->setStyleSheet("font-weight: bold; color: #4CAF50; background: transparent;");
        } else {
            m_lblBLEStatus->setText("BLE 状态: ❌ 未连接");
            m_lblBLEStatus->setStyleSheet("font-weight: bold; color: #f44336; background: transparent;");
        }
    }
}

void MainWindow::addBLELog(const QString& message)
{
    QTextEdit *logEdit = findChild<QTextEdit*>("txtBLELog");
    if (logEdit) {
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
        logEdit->append(QString("[%1] %2").arg(timestamp, message));
    }
}

void MainWindow::updateSimulatedModeUI()
{
    if (!m_frameSimControl) return;

    if (m_simulatedMode) {
        m_frameSimControl->show();
    } else {
        m_frameSimControl->hide();
    }
}

void MainWindow::updateModeStatusDisplay()
{
    if (m_lblSimModeStatus) {
        if (m_simulatedMode) {
            m_lblSimModeStatus->setText("当前模式: 🎮 模拟手柄 (无物理手柄)");
            m_lblSimModeStatus->setStyleSheet("color: blue; font-weight: bold;");
        } else {
            m_lblSimModeStatus->setText("当前模式: 🎯 真实手柄");
            m_lblSimModeStatus->setStyleSheet("color: green; font-weight: bold;");
        }
    }
}

void MainWindow::updateSimulatedButtonDisplay()
{
    if (!m_simulatedMode) return;
    if (!m_btnSimForward) return;

    auto setBtnActive = [](QPushButton* btn, bool active) {
        if (!btn) return;
        QString base = btn->text().left(2);
        QString color = active ? "#ffc107" : (base == "▲" || base == "▼" ? (base == "▲" ? "#4CAF50" : "#ff9800") : "#2196F3");
        btn->setStyleSheet(QString("QPushButton { background-color: %1; color: white; font-weight: bold; border-radius: 6px; }").arg(color));
    };

    setBtnActive(m_btnSimForward, m_simForward);
    setBtnActive(m_btnSimBackward, m_simBackward);
    setBtnActive(m_btnSimLeft, m_simLeft);
    setBtnActive(m_btnSimRight, m_simRight);
}

void MainWindow::onSimForwardPressed() { m_simForward = true; }
void MainWindow::onSimForwardReleased() { m_simForward = false; }
void MainWindow::onSimBackwardPressed() { m_simBackward = true; }
void MainWindow::onSimBackwardReleased() { m_simBackward = false; }
void MainWindow::onSimLeftPressed() { m_simLeft = true; }
void MainWindow::onSimLeftReleased() { m_simLeft = false; }
void MainWindow::onSimRightPressed() { m_simRight = true; }
void MainWindow::onSimRightReleased() { m_simRight = false; }
void MainWindow::onSimSlowClicked() {
    m_speedLevel = 0; setSpeedLimit(100);
    updateButtonDisplay();
    addBTLog("速度档位: 🐢 低速 (100)");
    addWiFiLog("速度档位: 🐢 低速 (100)");
}
void MainWindow::onSimMediumClicked() {
    m_speedLevel = 1; setSpeedLimit(500);
    updateButtonDisplay();
    addBTLog("速度档位: 🚗 中速 (500)");
    addWiFiLog("速度档位: 🚗 中速 (500)");
}
void MainWindow::onSimFastClicked() {
    m_speedLevel = 2; setSpeedLimit(1500);
    updateButtonDisplay();
    addBTLog("速度档位: ⚡ 高速 (1500)");
    addWiFiLog("速度档位: ⚡ 高速 (1500)");
}

void MainWindow::setSpeedLimit(int value)
{
    m_speedLimit=qBound(100,100+((value-100+50)/100)*100,1500);
    if(m_speedSetting) { QSignalBlocker b(m_speedSetting); m_speedSetting->setValue(m_speedLimit); }
}
