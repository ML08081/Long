#include "remotepanel.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

RemotePanel::RemotePanel(NetClient *net, QWidget *parent)
    : QWidget(parent), m_net(net)
{
    setWindowTitle("LongLook 遥控面板");
    setObjectName("Root");
    resize(980, 720);

    // 全局 QSS 未覆盖 QGroupBox / 单选钮，这里补齐，保证与主界面同一套暗色风格
    setStyleSheet(R"QSS(
QGroupBox { color: #5cc8ff; font-weight: bold; border: 1px solid #2c3650;
            border-radius: 8px; margin-top: 10px; padding-top: 8px; background: #1d2433; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QGroupBox:disabled { color: #5a6378; border: 1px solid #232a38; }
QRadioButton { color: #e8ecf4; padding: 4px 8px; }
QRadioButton:checked { color: #5cc8ff; font-weight: bold; }
QCheckBox { color: #e8ecf4; }
QSlider::groove:horizontal { height: 6px; background: #11161f; border-radius: 3px; }
QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px; background: #2f6df0; }
QSlider::handle:horizontal:disabled { background: #3a4760; }
)QSS");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    m_lblConn = new QLabel("连接状态：未连接");
    m_lblConn->setObjectName("StatusBad");
    root->addWidget(m_lblConn);

    // 避障锁横幅：平时隐藏，一旦 F4 封锁前进就整条醒目提示并说明解锁方法。
    // 放在最顶部是因为被锁时操作者会以为"车坏了/没反应"，必须第一眼看到原因。
    m_lblObsLock = new QLabel();
    m_lblObsLock->setWordWrap(true);
    m_lblObsLock->setStyleSheet(
        "background:#b71c1c; color:#ffffff; font-weight:bold;"
        "padding:8px; border-radius:4px;");
    m_lblObsLock->setVisible(false);
    root->addWidget(m_lblObsLock);

    root->addWidget(buildWayGroup());
    root->addWidget(buildModeGroup());

    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(10);
    ctrlRow->addWidget(buildManualGroup(), 1);
    ctrlRow->addWidget(buildPadGroup(), 1);
    root->addLayout(ctrlRow);

    root->addWidget(buildTelemetryGroup());
    root->addWidget(buildLogGroup(), 1);

    QPushButton *estop = new QPushButton("紧急停止");
    estop->setObjectName("Estop");
    estop->setMinimumHeight(42);
    estop->setCursor(Qt::PointingHandCursor);
    connect(estop, &QPushButton::clicked, this, &RemotePanel::onEstop);
    root->addWidget(estop);

    // 界面遥控的持续下发：5Hz
    m_contTimer = new QTimer(this);
    m_contTimer->setInterval(200);
    connect(m_contTimer, &QTimer::timeout, this, &RemotePanel::onContinuousTick);

    // 手柄轮询：60Hz，与《遥控手柄V2》UPDATE_RATE 一致
    m_pad = new JoystickReader(this);
    connect(m_pad, &JoystickReader::connected,     this, &RemotePanel::onPadConnected);
    connect(m_pad, &JoystickReader::disconnected,  this, &RemotePanel::onPadDisconnected);
    connect(m_pad, &JoystickReader::axisChanged,   this, &RemotePanel::onPadAxis);
    connect(m_pad, &JoystickReader::buttonChanged, this, &RemotePanel::onPadButton);

    m_padTimer = new QTimer(this);
    m_padTimer->setInterval(kPollMs);
    connect(m_padTimer, &QTimer::timeout, this, &RemotePanel::onPadTick);

    m_rateTimer = new QTimer(this);
    m_rateTimer->setInterval(1000);
    connect(m_rateTimer, &QTimer::timeout, this, &RemotePanel::onRateTick);
    m_rateTimer->start();

    if (m_net) {
        connect(m_net, &NetClient::sensorUpdated, this, &RemotePanel::onSensor);
        connect(m_net, &NetClient::logMessage,    this, &RemotePanel::onLog);
        connect(m_net, &NetClient::connected,     this, &RemotePanel::onConnected);
        connect(m_net, &NetClient::disconnected,  this, &RemotePanel::onDisconnected);
        if (m_net->isConnected())
            onConnected();
    }

    onWayChanged();   // 应用初始方式（界面遥控）
}

// ---------------------------------------------------------------- 界面构建

QWidget *RemotePanel::buildWayGroup()
{
    QGroupBox *box = new QGroupBox("遥控方式");
    QHBoxLayout *layout = new QHBoxLayout(box);

    m_radUi  = new QRadioButton("界面遥控");
    m_radPad = new QRadioButton("手柄遥控");
    m_radUi->setChecked(true);

    QButtonGroup *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(m_radUi);
    group->addButton(m_radPad);

    layout->addWidget(m_radUi);
    layout->addWidget(m_radPad);

    QLabel *hint = new QLabel("两种方式互斥，同一时刻只有一路下发控制指令；"
                              "手柄掉线会先停车再自动切回界面遥控");
    hint->setObjectName("Hint");
    hint->setWordWrap(true);
    layout->addWidget(hint, 1);

    connect(m_radUi,  &QRadioButton::toggled, this, &RemotePanel::onWayChanged);
    connect(m_radPad, &QRadioButton::toggled, this, &RemotePanel::onWayChanged);
    return box;
}

QWidget *RemotePanel::buildModeGroup()
{
    QGroupBox *box = new QGroupBox("运行模式");
    QHBoxLayout *layout = new QHBoxLayout(box);

    struct ModeButton { const char *name; int value; };
    // 模式值不连续，三端必须一致：0 遥控 / 1 循迹 / 3 循迹+视觉
    const ModeButton modes[] = {
        {"遥控（手动）", 0},
        {"循迹",         1},
        {"循迹+视觉",    3},
    };
    for (const ModeButton &mode : modes) {
        QPushButton *button = new QPushButton(mode.name);
        button->setMinimumHeight(34);
        connect(button, &QPushButton::clicked, this, [this, mode]() { onModeClicked(mode.value); });
        layout->addWidget(button);
    }
    return box;
}

QWidget *RemotePanel::buildManualGroup()
{
    m_boxUi = new QGroupBox("界面遥控");
    QVBoxLayout *layout = new QVBoxLayout(m_boxUi);

    QHBoxLayout *speedRow = new QHBoxLayout();
    QLabel *kSpeed = new QLabel("速度");
    kSpeed->setObjectName("Key");
    speedRow->addWidget(kSpeed);
    m_sldSpeed = new QSlider(Qt::Horizontal);
    m_sldSpeed->setRange(-kSteerRange, kSteerRange);
    connect(m_sldSpeed, &QSlider::valueChanged, this, &RemotePanel::onSpeedChanged);
    speedRow->addWidget(m_sldSpeed, 1);
    m_lblSpeed = new QLabel("0");
    m_lblSpeed->setObjectName("Value");
    m_lblSpeed->setMinimumWidth(52);
    m_lblSpeed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    speedRow->addWidget(m_lblSpeed);
    layout->addLayout(speedRow);

    QHBoxLayout *steerRow = new QHBoxLayout();
    QLabel *kSteer = new QLabel("转向");
    kSteer->setObjectName("Key");
    steerRow->addWidget(kSteer);
    m_sldSteer = new QSlider(Qt::Horizontal);
    m_sldSteer->setRange(-kSteerRange, kSteerRange);
    connect(m_sldSteer, &QSlider::valueChanged, this, &RemotePanel::onSteerChanged);
    steerRow->addWidget(m_sldSteer, 1);
    m_lblSteer = new QLabel("0");
    m_lblSteer->setObjectName("Value");
    m_lblSteer->setMinimumWidth(52);
    m_lblSteer->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    steerRow->addWidget(m_lblSteer);
    layout->addLayout(steerRow);

    QGridLayout *buttons = new QGridLayout();
    struct DriveButton { const char *text; int speed; int steer; int row; int col; };
    const DriveButton driveButtons[] = {
        {"前进", 400,    0, 0, 1},
        {"左转", 300, -600, 1, 0},
        {"停止",   0,    0, 1, 1},
        {"右转", 300,  600, 1, 2},
        {"后退", -400,   0, 2, 1},
    };
    for (const DriveButton &drive : driveButtons) {
        QPushButton *button = new QPushButton(drive.text);
        button->setMinimumHeight(34);
        connect(button, &QPushButton::clicked, this, [this, drive]() {
            m_sldSpeed->setValue(drive.speed);
            m_sldSteer->setValue(drive.steer);
            sendCurrent();
        });
        buttons->addWidget(button, drive.row, drive.col);
    }
    layout->addLayout(buttons);

    QHBoxLayout *actions = new QHBoxLayout();
    QPushButton *send = new QPushButton("下发一次");
    send->setObjectName("Primary");
    send->setMinimumHeight(34);
    connect(send, &QPushButton::clicked, this, &RemotePanel::sendCurrent);
    actions->addWidget(send);

    m_chkContinuous = new QCheckBox("持续下发（5Hz）");
    connect(m_chkContinuous, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled && padActive()) {          // 手柄模式下不允许界面持续下发
            m_chkContinuous->setChecked(false);
            return;
        }
        if (enabled) m_contTimer->start();
        else         m_contTimer->stop();
    });
    actions->addWidget(m_chkContinuous);

    QPushButton *stop = new QPushButton("停车");
    stop->setMinimumHeight(34);
    connect(stop, &QPushButton::clicked, this, &RemotePanel::onStop);
    actions->addWidget(stop);

    layout->addLayout(actions);
    return m_boxUi;
}

QWidget *RemotePanel::buildPadGroup()
{
    m_boxPad = new QGroupBox("手柄遥控");
    QVBoxLayout *layout = new QVBoxLayout(m_boxPad);

    auto addRow = [&](const QString &name, QLabel *&value, const QString &init) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *k = new QLabel(name);
        k->setObjectName("Key");
        k->setMinimumWidth(64);
        row->addWidget(k);
        value = new QLabel(init);
        value->setObjectName("Value");
        row->addWidget(value, 1);
        layout->addLayout(row);
    };

    addRow("手柄状态", m_lblPadState, "未启用");
    addRow("速度档位", m_lblPadLevel, "中档 600");
    addRow("摇杆",     m_lblPadStick, "左 X=0.00  Y=0.00");
    addRow("当前输出", m_lblPadOut,   "速度=0  转向=0");

    QLabel *help = new QLabel(
        "键位（与遥控手柄V2 一致）：\n"
        "左摇杆    上下=速度，左右=转向\n"
        "A / 方向上    全速前进\n"
        "B / 方向下    全速后退\n"
        "X / 方向左    左满舵\n"
        "Y / 方向右    右满舵\n"
        "LB    低档 300 与中档 600 切换\n"
        "RB    高档 1000 与中档 600 切换\n"
        "Back  档位回到中档 600\n"
        "Start 紧急停止");
    help->setObjectName("Hint");
    layout->addWidget(help);
    layout->addStretch(1);
    return m_boxPad;
}

QWidget *RemotePanel::buildTelemetryGroup()
{
    QGroupBox *box = new QGroupBox("遥测");
    QGridLayout *layout = new QGridLayout(box);
    int idx = 0;
    auto addCell = [&](const QString &name, QLabel *&value) {
        const int row = idx / 3, col = (idx % 3) * 2;
        QLabel *k = new QLabel(name);
        k->setObjectName("Key");
        layout->addWidget(k, row, col);
        value = new QLabel("--");
        value->setObjectName("Value");
        layout->addWidget(value, row, col + 1);
        ++idx;
    };

    addCell("左右轮速", m_tSpeed);
    addCell("当前模式", m_tMode);
    addCell("超声波",   m_tDist);
    addCell("激光测距", m_tLaser);
    addCell("编码器左", m_tEnc1);
    addCell("编码器右", m_tEnc2);
    addCell("遥测速率", m_tHz);
    addCell("时间戳",   m_tTs);
    addCell("故障标志", m_tFault);
    return box;
}

QWidget *RemotePanel::buildLogGroup()
{
    QGroupBox *box = new QGroupBox("日志");
    QVBoxLayout *layout = new QVBoxLayout(box);
    m_log = new QTextEdit();
    m_log->setObjectName("Log");
    m_log->setReadOnly(true);
    layout->addWidget(m_log);
    return box;
}

// ---------------------------------------------------------------- 方式切换

bool RemotePanel::padActive() const
{
    return m_radPad && m_radPad->isChecked();
}

void RemotePanel::stopAllOutput()
{
    if (m_contTimer) m_contTimer->stop();
    if (m_padTimer)  m_padTimer->stop();
    if (m_chkContinuous) m_chkContinuous->setChecked(false);
    if (m_net) m_net->sendDrive(0, 0);
}

void RemotePanel::onWayChanged()
{
    // toggled 会成对触发（取消选中 + 选中），只在选中那次真正处理
    if (!m_radUi || !m_radPad) return;
    if (!m_radUi->isChecked() && !m_radPad->isChecked()) return;

    stopAllOutput();

    const bool pad = padActive();
    if (m_boxUi)  m_boxUi->setEnabled(!pad);
    if (m_boxPad) m_boxPad->setEnabled(pad);

    if (pad) {
        m_padLastSpeed = 32767;
        m_padLastSteer = 32767;
        m_padHeartbeat = 0;
        m_padLevel = 1;                       // 每次进入手柄模式回到中档，避免残留高档误冲
        for (double &axis : m_padAxes)  axis = 0.0;
        for (bool &button : m_padButtons) button = false;

        if (m_net) m_net->sendCommand(LL::CMD_MODE, 0);   // 遥控必须在手动模式下
        if (m_padTimer) m_padTimer->start();
        if (m_lblPadState) m_lblPadState->setText("搜索手柄中");
        onLog("已切换到手柄遥控，等待手柄接入");
    } else {
        m_sldSpeed->setValue(0);
        m_sldSteer->setValue(0);
        if (m_lblPadState) m_lblPadState->setText("未启用");
        onLog("已切换到界面遥控");
    }
    refreshPadLabels();
}

// ---------------------------------------------------------------- 界面遥控

void RemotePanel::onSpeedChanged(int value)
{
    m_lblSpeed->setText(QString::number(value));
    if (!padActive() && m_chkContinuous && m_chkContinuous->isChecked())
        sendCurrent();
}

void RemotePanel::onSteerChanged(int value)
{
    m_lblSteer->setText(QString::number(value));
    if (!padActive() && m_chkContinuous && m_chkContinuous->isChecked())
        sendCurrent();
}

void RemotePanel::sendCurrent()
{
    if (!m_net || padActive()) return;
    m_net->sendDrive(static_cast<qint16>(m_sldSpeed->value()),
                     static_cast<qint16>(m_sldSteer->value()));
}

void RemotePanel::onContinuousTick()
{
    sendCurrent();
}

void RemotePanel::onStop()
{
    m_sldSpeed->setValue(0);
    m_sldSteer->setValue(0);
    if (m_net) m_net->sendDrive(0, 0);
}

// ---------------------------------------------------------------- 手柄遥控

int RemotePanel::currentSpeedMax() const
{
    switch (m_padLevel) {
    case 0:  return kSpeedSlow;
    case 2:  return kSpeedFast;
    default: return kSpeedMedium;
    }
}

void RemotePanel::onPadTick()
{
    if (!padActive() || !m_pad) return;
    m_pad->update();
    applyPadState();
}

void RemotePanel::applyPadState()
{
    const int speedMax = currentSpeedMax();

    // 左摇杆：Y 轴向上为正，取负得到“前进为正”；X 轴直接映射转向
    const double rawSpeed = -m_padAxes[1];
    const double rawSteer =  m_padAxes[0];

    qint16 speed = static_cast<qint16>(qBound(-kSteerRange, int(rawSpeed * speedMax), kSteerRange));
    qint16 steer = static_cast<qint16>(qBound(-kSteerRange, int(rawSteer * kSteerRange), kSteerRange));

    // 按键为“满量程”覆盖：A/B 走当前档位速度，X/Y 打满舵；方向键为 LongLook 扩展
    if (m_padButtons[0] || m_padButtons[10]) speed = static_cast<qint16>(speedMax);
    if (m_padButtons[1] || m_padButtons[11]) speed = static_cast<qint16>(-speedMax);
    if (m_padButtons[2] || m_padButtons[12]) steer = -kSteerRange;
    if (m_padButtons[3] || m_padButtons[13]) steer =  kSteerRange;

    // 变化就发；不变则按心跳补发，静止时降到 1Hz 左右，避免刷屏又不丢保活
    const bool changed = (speed != m_padLastSpeed || steer != m_padLastSteer);
    const bool moving  = (speed != 0 || steer != 0);
    const int heartbeatPeriod = moving ? 3 : 30;
    ++m_padHeartbeat;

    if (changed || m_padHeartbeat >= heartbeatPeriod) {
        if (m_net) m_net->sendDrive(speed, steer);
        m_padLastSpeed = speed;
        m_padLastSteer = steer;
        m_padHeartbeat = 0;
    }

    if (m_lblPadOut)
        m_lblPadOut->setText(QString("速度=%1  转向=%2").arg(speed).arg(steer));
    if (m_lblPadStick)
        m_lblPadStick->setText(QString("左 X=%1  Y=%2")
                               .arg(m_padAxes[0], 0, 'f', 2)
                               .arg(m_padAxes[1], 0, 'f', 2));
}

void RemotePanel::refreshPadLabels()
{
    if (!m_lblPadLevel) return;
    static const char *names[] = {"低档", "中档", "高档"};
    const int lv = qBound(0, m_padLevel, 2);
    m_lblPadLevel->setText(QString("%1 %2").arg(names[lv]).arg(currentSpeedMax()));
}

void RemotePanel::onPadConnected()
{
    if (m_lblPadState) m_lblPadState->setText("已连接");
    onLog("手柄已连接");
}

void RemotePanel::onPadDisconnected()
{
    // 先停车，再回退到界面遥控——对应遥控手柄V2 的“模拟手柄模式”，不留失控窗口
    if (m_net) m_net->sendDrive(0, 0);
    if (m_lblPadState) m_lblPadState->setText("已断开");
    onLog("手柄已断开，已下发停车并切回界面遥控");
    if (m_radUi) m_radUi->setChecked(true);
}

void RemotePanel::onPadAxis(int axis, double value)
{
    if (axis < 0 || axis >= 4) return;
    m_padAxes[axis] = value;
}

void RemotePanel::onPadButton(int button, bool pressed)
{
    if (button < 0 || button >= 16) return;
    m_padButtons[button] = pressed;
    if (!pressed) return;

    // 键位语义与遥控手柄V2 原版一致（索引：4=LB 5=RB 6=Back 7=Start）
    switch (button) {
    case 7:                                       // Start：急停
        onEstop();
        break;
    case 4:                                       // LB：低档 <-> 中档
        m_padLevel = (m_padLevel == 0) ? 1 : 0;
        refreshPadLabels();
        onLog(QString("速度档位切换：%1").arg(currentSpeedMax()));
        break;
    case 5:                                       // RB：高档 <-> 中档
        m_padLevel = (m_padLevel == 2) ? 1 : 2;
        refreshPadLabels();
        onLog(QString("速度档位切换：%1").arg(currentSpeedMax()));
        break;
    case 6:                                       // Back：回中档
        m_padLevel = 1;
        refreshPadLabels();
        onLog("速度档位回到中档 600");
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- 通用

void RemotePanel::onModeClicked(int mode)
{
    if (m_net)
        m_net->sendCommand(LL::CMD_MODE, static_cast<quint8>(mode));
    onLog(QString("模式切换 -> %1（%2）").arg(modeName(static_cast<quint8>(mode))).arg(mode));
}

void RemotePanel::onEstop()
{
    if (m_net) {
        m_net->sendCommand(LL::CMD_ESTOP, 1);
        m_net->sendDrive(0, 0);
    }
    m_sldSpeed->setValue(0);
    m_sldSteer->setValue(0);
    m_padLastSpeed = 0;
    m_padLastSteer = 0;
    onLog("已下发紧急停止");
}

QString RemotePanel::modeName(quint8 mode)
{
    switch (mode & 0x7F) {
    case 0:  return "遥控";
    case 1:  return "循迹";
    case 2:  return "避障";
    case 3:  return "循迹+视觉";
    default: return QString("未知(%1)").arg(mode);
    }
}

void RemotePanel::onSensor(const LL::SensorData &data)
{
    ++m_sensorCount;
    m_tSpeed->setText(QString("%1 / %2").arg(data.speed_L).arg(data.speed_R));
    m_tMode->setText(QString("%1（%2）").arg(modeName(data.mode)).arg(data.mode));
    m_tDist->setText(data.distance_cm ? QString("%1 cm").arg(data.distance_cm) : "-- cm");
    m_tLaser->setText(data.laserOk() && data.laser_cm ? QString("%1 cm").arg(data.laser_cm) : "-- cm");
    m_tEnc1->setText(QString::number(data.encoder1));
    m_tEnc2->setText(QString::number(data.encoder2));
    m_tTs->setText(QString("%1 ms").arg(data.timestamp_ms));
    m_tFault->setText(data.fault ? QString("故障 0x%1").arg(data.fault, 2, 16, QChar('0')) : "正常");

    // 避障锁状态：锁定时给出明确的解锁指引，避免被误判为"遥控失灵"
    if (m_lblObsLock) {
        const bool locked = data.obsLock();
        if (locked != m_obsLockShown) {
            m_obsLockShown = locked;
            if (locked) {
                onLog("避障锁触发：前方障碍过近，前进已被下位机封锁。");
            } else {
                onLog("避障锁已解除，可正常前进。");
            }
        }
        m_lblObsLock->setVisible(locked);
        if (locked) {
            m_lblObsLock->setText(
                QString("避障锁定中：前方障碍 ≤8cm，前进已被下位机封锁（后退仍可用于脱困）。\n"
                        "解锁方法：先把车退开到 12cm 以外，然后松开前进、再推前进，连续两次。\n"
                        "当前前方：超声波 %1 cm / 激光 %2 cm")
                    .arg(data.distance_cm ? QString::number(data.distance_cm) : "--")
                    .arg(data.laserOk() && data.laser_cm ? QString::number(data.laser_cm) : "--"));
        }
    }
}

void RemotePanel::onLog(const QString &message)
{
    if (!m_log) return;
    m_log->append(QString("[%1] %2")
                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                  .arg(message));
}

void RemotePanel::onConnected()
{
    m_lblConn->setText("连接状态：已连接");
    m_lblConn->setObjectName("StatusOk");
    m_lblConn->style()->polish(m_lblConn);
}

void RemotePanel::onDisconnected()
{
    m_lblConn->setText("连接状态：未连接");
    m_lblConn->setObjectName("StatusBad");
    m_lblConn->style()->polish(m_lblConn);
    stopAllOutput();
}

void RemotePanel::onRateTick()
{
    m_tHz->setText(QString("%1 Hz").arg(m_sensorCount));
    m_sensorCount = 0;
}

void RemotePanel::closeEvent(QCloseEvent *event)
{
    // 关窗即收口：停掉两路定时器并下发停车，杜绝“窗口关了车还在跑”
    stopAllOutput();
    QWidget::closeEvent(event);
}
