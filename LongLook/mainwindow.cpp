#include "mainwindow.h"
#include "remotepanel.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QLabel>
#include <QFrame>
#include <QDateTime>
#include <QIntValidator>
#include <QStyle>
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QRegularExpression>

// ─── 小工具：构建一个 checkable 控制按钮 ─────────────────────────────
static QPushButton *mkToggle(const QString &text)
{
    QPushButton *b = new QPushButton(text);
    b->setObjectName("Toggle");
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(34);
    return b;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("LongLook — 智能巡检系统 监控前端（龙芯 2K0300）");
    resize(1320, 840);

    m_net = new NetClient(this);
    connect(m_net, &NetClient::connected,     this, &MainWindow::onConnected);
    connect(m_net, &NetClient::disconnected,  this, &MainWindow::onDisconnected);
    connect(m_net, &NetClient::logMessage,    this, &MainWindow::onLog);
    connect(m_net, &NetClient::sensorUpdated, this, &MainWindow::onSensorUpdated);
    connect(m_net, &NetClient::videoFrame,    this, &MainWindow::onVideoFrame);
    connect(m_net, &NetClient::thermalFrame,  this, &MainWindow::onThermalFrame);
    connect(m_net, &NetClient::textFrame,     this, &MainWindow::onTextFrame);
    connect(m_net, &NetClient::discovered,    this, &MainWindow::onDiscovered);
    connect(m_net, &NetClient::connecting,    this, &MainWindow::onConnecting);
    // 注意：startDiscovery 会同步 emit logMessage → onLog，而 onLog 依赖 m_log(日志控件)。
    // 此刻 UI 尚未构建、m_log 仍为空，若在此启动发现会对空指针调用而崩溃。
    // 因此推迟到构造函数末尾、UI 全部就绪后再启动发现（见文件底部）。

    // 视觉识别边车（预留接口，默认关闭）。模型/解释器可在此处切换。
    m_vision = new VisionProcessor(this);
    // m_vision->setModelPath("F:/projectForXN/NGFXM/best.pt");  // 换模型改这里即可
    connect(m_vision, &VisionProcessor::detections,   this, &MainWindow::onVisionDetections);
    connect(m_vision, &VisionProcessor::statusChanged, this, &MainWindow::onVisionStatus);
    connect(m_vision, &VisionProcessor::logMessage,   this, &MainWindow::onLog);

    // 手柄读取与手动驱动统一收归遥控面板（RemotePanel），主窗口不再直接下发 drive，
    // 避免两处同时下发 FRAME_DRIVE 互相打架。

    QWidget *central = new QWidget(this);
    central->setObjectName("Root");
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    root->addWidget(buildTopBar());

    // 中部：左列卡片 | 视频 | (热成像 + 控制)
    QHBoxLayout *mid = new QHBoxLayout();
    mid->setSpacing(8);
    mid->addWidget(buildLeftColumn(), 0);
    mid->addWidget(buildVideoCard(), 3);

    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->setSpacing(8);
    rightCol->addWidget(buildThermalCard(), 1);
    rightCol->addWidget(buildControlCard(), 0);
    QWidget *rightWrap = new QWidget();
    rightWrap->setLayout(rightCol);
    rightWrap->setMinimumWidth(330);
    mid->addWidget(rightWrap, 2);

    root->addLayout(mid, 1);
    root->addWidget(buildLogCard(), 0);

    setCentralWidget(central);

    m_rateTimer = new QTimer(this);
    connect(m_rateTimer, &QTimer::timeout, this, &MainWindow::onRateTick);
    m_rateTimer->start(1000);

    setConnectedUi(false);
    setRisk(0);
    onLog("LongLook 已启动。选择开发板预设(龙芯)或填入 IP/端口后点击「连接」。");

    // UI 已全部就绪，此刻再启动 UDP 自动发现：其 logMessage 才能安全写入日志控件。
    m_net->startDiscovery(8083);   // 龙芯 IP 变化也能自动连上
}

// ─── 顶栏 ───────────────────────────────────────────────────────
QWidget *MainWindow::buildTopBar()
{
    QFrame *bar = new QFrame();
    bar->setObjectName("TopBar");
    QHBoxLayout *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(14, 8, 14, 8);

    QLabel *title = new QLabel("智能巡检系统 · 监控前端");
    title->setObjectName("Title");
    lay->addWidget(title);
    lay->addSpacing(24);

    // 预设开发板地址：龙芯 / 本机。选中即填入右侧 IP 框（仍可手动改）。
    lay->addWidget(new QLabel("开发板:"));
    m_cmbPreset = new QComboBox();
    m_cmbPreset->addItem("预设地址…",      QString());
    m_cmbPreset->addItem("龙芯 B410",      QStringLiteral("192.168.3.100"));
    m_cmbPreset->addItem("本机 localhost", QStringLiteral("127.0.0.1"));
    m_cmbPreset->setFixedWidth(140);
    m_cmbPreset->setToolTip("选择开发板预设 IP（龙芯 / 本机），或直接在右侧手动输入");
    lay->addWidget(m_cmbPreset);
    connect(m_cmbPreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPresetChanged);

    lay->addWidget(new QLabel("IP:"));
    m_editIp = new QLineEdit("192.168.3.172");
    m_editIp->setFixedWidth(140);
    lay->addWidget(m_editIp);
    lay->addWidget(new QLabel("端口:"));
    m_editPort = new QLineEdit("8080");
    m_editPort->setFixedWidth(64);
    m_editPort->setValidator(new QIntValidator(1, 65535, m_editPort));
    lay->addWidget(m_editPort);

    m_btnConnect = new QPushButton("连接");
    m_btnConnect->setObjectName("Primary");
    m_btnDisconnect = new QPushButton("断开");
    m_btnRemote = new QPushButton("遥控面板");
    lay->addWidget(m_btnConnect);
    lay->addWidget(m_btnDisconnect);
    lay->addWidget(m_btnRemote);
    connect(m_btnConnect,    &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_btnRemote,     &QPushButton::clicked, this, &MainWindow::onOpenRemotePanel);

    m_lblConnStatus = new QLabel("● 未连接");
    m_lblConnStatus->setObjectName("StatusBad");
    lay->addWidget(m_lblConnStatus);

    // 自动发现指示：局域网内发现龙芯广播即显示其 IP/版本（IP 变了也能看见）
    m_lblDiscovered = new QLabel("🔍 未发现开发板");
    m_lblDiscovered->setObjectName("Discovered");
    lay->addSpacing(12);
    lay->addWidget(m_lblDiscovered);

    lay->addStretch();

    m_lblRisk = new QLabel("风险: 安全");
    m_lblRisk->setObjectName("RiskBadge");
    m_lblRisk->setMinimumWidth(140);
    m_lblRisk->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_lblRisk);

    return bar;
}

// ─── 左列：环境 / 运动 / 视觉风险 三张卡片 ───────────────────────────
QWidget *MainWindow::buildLeftColumn()
{
    QWidget *col = new QWidget();
    QVBoxLayout *v = new QVBoxLayout(col);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 环境感知
    Card *env = new Card("环境感知");
    m_valTemp     = env->addValue("温度", "— °C");
    m_valHumi     = env->addValue("湿度", "— %");
    m_valGas      = env->addValue("烟雾/气体", "—");
    m_valDistance = env->addValue("超声波距离", "— cm");
    m_valLaser    = env->addValue("激光测距", "— cm");
    v->addWidget(env);

    // 运动状态
    Card *mot = new Card("运动状态");
    m_valEnc1    = mot->addValue("编码器1", "—");
    m_valEnc2    = mot->addValue("编码器2", "—");
    m_valSpeedL  = mot->addValue("左轮速度", "—");
    m_valSpeedR  = mot->addValue("右轮速度", "—");
    m_valServo   = mot->addValue("转向舵机", "— us");
    m_valVoltage = mot->addValue("电池电压", "— V");
    m_valMode    = mot->addValue("工作模式", "—");
    m_valFault   = mot->addValue("故障标志", "—");
    v->addWidget(mot);

    // 视觉与风险
    Card *vis = new Card("视觉与风险");
    m_valRisk     = vis->addValue("风险等级", "—");
    m_valFlame    = vis->addValue("火焰识别", "—");
    m_valSmoke    = vis->addValue("烟雾识别", "—");
    m_valGasAlarm = vis->addValue("气体报警", "—");
    v->addWidget(vis);

    // 链路信息（健康/新鲜度指示：一眼判断"连着却没数据"这种冻结掉线）
    Card *sys = new Card("链路健康");
    m_valLink      = sys->addValue("链路状态", "○ 未连接");
    m_valRemoteRx  = sys->addValue("龙芯RX", "tele=— env=—");
    m_valSensorHz  = sys->addValue("遥测帧率", "0 Hz");
    m_valTimestamp = sys->addValue("系统时间", "— ms");
    v->addWidget(sys);

    v->addStretch();

    // 滚动容器，窗口变矮时左列可滚动
    QScrollArea *scroll = new QScrollArea();
    scroll->setObjectName("LeftScroll");
    scroll->setWidget(col);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(300);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

QWidget *MainWindow::buildVideoCard()
{
    Card *c = new Card("视频（高清摄像头）");

    // 视觉识别开关行（YOLO，可开可关）
    QHBoxLayout *visRow = new QHBoxLayout();
    m_btnVision = mkToggle("视觉识别 (YOLO)");
    m_lblVision = new QLabel("视觉: 关闭");
    m_lblVision->setObjectName("Hint");
    visRow->addWidget(m_btnVision);
    visRow->addWidget(m_lblVision, 1);
    c->body()->addLayout(visRow);
    connect(m_btnVision, &QPushButton::toggled, this, &MainWindow::onVisionToggled);

    m_videoView = new ImageView("等待视频流...\n龙芯下发 0x10 JPEG 帧");
    c->body()->addWidget(m_videoView, 1);
    return c;
}

QWidget *MainWindow::buildThermalCard()
{
    Card *c = new Card("热成像（红外测温 MLX90640）");
    m_thermalView = new ImageView("等待热成像...\nF4 经龙芯下发 0x20 温度帧");
    c->body()->addWidget(m_thermalView, 1);
    m_lblThermalRange = new QLabel("量程: —");
    m_lblThermalRange->setObjectName("Hint");
    m_lblThermalRange->setAlignment(Qt::AlignCenter);
    c->body()->addWidget(m_lblThermalRange);
    return c;
}

// ─── 执行与联动控制 ──────────────────────────────────────────────
QWidget *MainWindow::buildControlCard()
{
    Card *c = new Card("执行与联动控制（未接入）");

    QGridLayout *g = new QGridLayout();
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(8);
    m_btnFan    = mkToggle("风扇");
    m_btnBuzzer = mkToggle("蜂鸣器");
    m_btnRelay  = mkToggle("继电器");
    m_btnLed    = mkToggle("LED");
    g->addWidget(m_btnFan,    0, 0);
    g->addWidget(m_btnBuzzer, 0, 1);
    g->addWidget(m_btnRelay,  1, 0);
    g->addWidget(m_btnLed,    1, 1);
    c->body()->addLayout(g);
    connect(m_btnFan,    &QPushButton::toggled, this, &MainWindow::onFanToggled);
    connect(m_btnBuzzer, &QPushButton::toggled, this, &MainWindow::onBuzzerToggled);
    connect(m_btnRelay,  &QPushButton::toggled, this, &MainWindow::onRelayToggled);
    connect(m_btnLed,    &QPushButton::toggled, this, &MainWindow::onLedToggled);

    QHBoxLayout *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("工作模式:"));
    m_cmbMode = new QComboBox();
    // ★ 2026-07-21 三模式（与 F4 protocol.h / 龙芯 Protocol.h 的编码值一致）。
    //   注意值是 0/1/3（跳过 2=避障，那是 F4 内部的避障【状态】不是用户模式），
    //   因此不能再用下拉索引直接当模式值，必须用 itemData 携带真实值。
    m_cmbMode->addItem("遥控模式",          0);  // MODE_REMOTE
    m_cmbMode->addItem("循迹模式（自动巡检）", 1);  // MODE_LINE_ONLY
    m_cmbMode->addItem("循迹 + 视觉模式",    3);  // MODE_LINE_VISION
    m_cmbMode->setToolTip(
        "遥控模式：完全由上位机下发速度/转向\n"
        "循迹模式：F4 本地 TCRT 循线自动巡检（断网也能跑）\n"
        "循迹+视觉：循线仍在 F4 本地闭环，龙芯视觉(YOLO/ArUco)作为叠加约束介入");
    modeRow->addWidget(m_cmbMode, 1);
    c->body()->addLayout(modeRow);
    connect(m_cmbMode, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);

    m_btnEstop = new QPushButton("紧急停止");
    m_btnEstop->setObjectName("Estop");
    m_btnEstop->setMinimumHeight(40);
    m_btnEstop->setCursor(Qt::PointingHandCursor);
    c->body()->addWidget(m_btnEstop);
    connect(m_btnEstop, &QPushButton::clicked, this, &MainWindow::onEstop);

    QPushButton *btnRemote = new QPushButton("打开遥控面板");
    btnRemote->setMinimumHeight(34);
    c->body()->addWidget(btnRemote);
    connect(btnRemote, &QPushButton::clicked, this, &MainWindow::onOpenRemotePanel);

    QLabel *remoteHint = new QLabel("界面遥控与手柄遥控均在遥控面板中选择");
    remoteHint->setObjectName("Hint");
    remoteHint->setWordWrap(true);
    c->body()->addWidget(remoteHint);

    return c;
}

QWidget *MainWindow::buildLogCard()
{
    // 双栏日志：左=上位机本地日志，右=龙芯端日志（TCP 文本帧）。便于分别排错。
    Card *c = new Card("日志 / 报警");

    QHBoxLayout *row = new QHBoxLayout();
    row->setSpacing(8);

    QVBoxLayout *localCol = new QVBoxLayout();
    QLabel *lblLocal = new QLabel("上位机日志");
    lblLocal->setObjectName("Hint");
    m_log = new QTextEdit();
    m_log->setObjectName("Log");
    m_log->setReadOnly(true);
    localCol->addWidget(lblLocal);
    localCol->addWidget(m_log);

    QVBoxLayout *remoteCol = new QVBoxLayout();
    QLabel *lblRemote = new QLabel("龙芯日志（TCP）");
    lblRemote->setObjectName("Hint");
    m_logRemote = new QTextEdit();
    m_logRemote->setObjectName("Log");
    m_logRemote->setReadOnly(true);
    remoteCol->addWidget(lblRemote);
    remoteCol->addWidget(m_logRemote);

    row->addLayout(localCol, 1);
    row->addLayout(remoteCol, 1);

    QWidget *wrap = new QWidget();
    wrap->setLayout(row);
    wrap->setFixedHeight(140);
    c->body()->addWidget(wrap);
    return c;
}

// ─── 连接控制 ────────────────────────────────────────────────────
void MainWindow::connectTo(const QString &ip, quint16 port)
{
    m_editIp->setText(ip);
    m_editPort->setText(QString::number(port));
    onConnectClicked();
}

// 选择预设开发板地址（龙芯 / 本机）→ 填入 IP 框；端口不变，仍可手动改。
void MainWindow::onPresetChanged(int index)
{
    const QString ip = m_cmbPreset->itemData(index).toString();
    if (ip.isEmpty()) return;              // index 0："预设地址…" 占位项
    m_editIp->setText(ip);
    onLog(QString("已选预设「%1」→ %2").arg(m_cmbPreset->itemText(index)).arg(ip));
}

void MainWindow::onConnectClicked()
{
    const QString ip = m_editIp->text().trimmed();
    const quint16 port = quint16(m_editPort->text().toUInt());
    if (ip.isEmpty() || port == 0) { onLog("请填写有效的 IP 和端口"); return; }
    m_net->connectToHost(ip, port);
}

void MainWindow::onDisconnectClicked() { m_userClosedLink = true; m_net->disconnectFromHost(); }

void MainWindow::onConnected()
{
    m_userClosedLink = false;
    m_autoConnectPending = false;
    setConnectedUi(true);
    // 重置链路新鲜度：等待首帧 F4 数据
    m_lastSensorMs = 0;
    m_remoteRxInit = false;
    m_valRemoteRx->setText("tele=— env=—");
    m_valRemoteRx->setStyleSheet("");
    m_valLink->setText("● 已连接 · 等待F4数据");
    m_valLink->setStyleSheet("color:#ffb020; font-weight:bold;");
}

// 连接尝试进行中（含自动重试）：橙色"连接中 (N/5)"，避免重复点击。
void MainWindow::onConnecting(int attempt, int maxAttempts)
{
    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_editIp->setEnabled(false);
    m_editPort->setEnabled(false);
    if (m_cmbPreset) m_cmbPreset->setEnabled(false);
    setConnLabel(QString("● 连接中 (%1/%2)").arg(attempt).arg(maxAttempts), "StatusWarn");
}

void MainWindow::onDisconnected()
{
    m_autoConnectPending = false;   // 允许下次发现重新自动连
    setConnectedUi(false);
    m_videoView->clearImage();
    m_thermalView->clearImage();
    m_lblThermalRange->setText("量程: —");
    setRisk(0);
    m_lastFrame = QImage();
    m_lastDets.clear();
    m_thermalAccum = QImage();
    // 链路健康复位
    m_lastSensorMs = 0;
    m_remoteRxInit = false;
    m_valLink->setText("○ 未连接");
    m_valLink->setStyleSheet("color:#8a94a8;");
    m_valRemoteRx->setText("tele=— env=—");
    m_valRemoteRx->setStyleSheet("");
}

// UDP 自动发现龙芯：IP 变化(如切到手机热点 DHCP)也能自动跟上并连接。
void MainWindow::onDiscovered(const QString &ip, quint16 port, const QString &version)
{
    m_lblDiscovered->setText(QString("🔍 %1 v%2").arg(ip).arg(version));   // 顶栏持续显示最新发现
    if (version != m_discoveredVer) {   // 版本变化才打日志，避免每 2s 刷屏
        m_discoveredVer = version;
        onLog(QString("发现开发板 %1:%2（v%3）").arg(ip).arg(port).arg(version));
    }
    // 未连接、非用户主动断开、且没有正在进行的自动连接 → 自动填入并连接
    if (!m_net->isConnected() && !m_userClosedLink && !m_autoConnectPending) {
        m_autoConnectPending = true;
        m_editIp->setText(ip);
        m_editPort->setText(QString::number(port));
        onLog(QString("自动连接到发现的开发板 %1:%2").arg(ip).arg(port));
        onConnectClicked();
    }
}

void MainWindow::setConnectedUi(bool connected)
{
    m_btnConnect->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
    m_editIp->setEnabled(!connected);
    m_editPort->setEnabled(!connected);
    if (m_cmbPreset) m_cmbPreset->setEnabled(!connected);
    setConnLabel(connected ? "● 已连接" : "● 未连接", connected ? "StatusOk" : "StatusBad");
}

// 更新连接状态标签（文本 + 由 objectName 决定的配色 StatusOk/StatusBad/StatusWarn）。
void MainWindow::setConnLabel(const QString &text, const char *objName)
{
    m_lblConnStatus->setText(text);
    m_lblConnStatus->setObjectName(objName);
    m_lblConnStatus->style()->unpolish(m_lblConnStatus);   // objectName 改变后需刷新样式
    m_lblConnStatus->style()->polish(m_lblConnStatus);
}

void MainWindow::onLog(const QString &msg)
{
    if (!m_log) return;   // 防护：UI 尚未构建时(如构造早期的回调)不写日志，避免空指针崩溃
    m_log->append(QString("[%1] %2")
                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(msg));
}

// ─── 数据回调 ────────────────────────────────────────────────────
QString MainWindow::modeName(quint8 mode)
{
    // ★ 与三模式定义对齐（F4 protocol.h / 龙芯 Protocol.h）。
    //   2 不是用户可选模式，而是 F4 本地避障【状态】的回显，故单列。
    switch (mode & 0x7F) {
    case 0:  return "遥控";
    case 1:  return "循迹";
    case 2:  return "避障中";
    case 3:  return "循迹+视觉";
    default: return QString("未知(%1)").arg(mode);
    }
}

void MainWindow::setRisk(quint8 level)
{
    static const char *txt[]   = { "安全", "注意", "警告", "危险" };
    static const char *bg[]    = { "#2e7d32", "#1565c0", "#ef6c00", "#c62828" };
    int l = qMin<int>(level, 3);
    m_lblRisk->setText(QString("风险: %1").arg(txt[l]));
    m_lblRisk->setStyleSheet(QString(
        "#RiskBadge{background:%1;color:white;border-radius:6px;padding:4px 10px;font-weight:bold;}")
        .arg(bg[l]));
}

void MainWindow::onSensorUpdated(const LL::SensorData &d)
{
    m_sensorCount++;
    m_lastSensorMs = QDateTime::currentMSecsSinceEpoch();   // 链路新鲜度打时间戳

    // 环境（F4 经 0x5C 帧上报：DHT11 温湿度 / MQ2 气体 / VL53L0X 激光）
    // DHT 有效位未置位 = F4 侧温湿度传感器无读数（并非上位机没刷新）。明确标注为"传感器无数据"，
    // 避免与"链路断开"混淆——链路断开由下方链路健康区单独提示。
    if (d.dhtOk()) {
        m_valTemp->setText(QString("%1 °C").arg(d.temperature_01c / 10.0, 0, 'f', 1));
        m_valHumi->setText(QString("%1 %").arg(d.humidity_01 / 10.0, 0, 'f', 1));
        m_valTemp->setStyleSheet("");
        m_valHumi->setStyleSheet("");
    } else {
        m_valTemp->setText("传感器无数据");
        m_valHumi->setText("传感器无数据");
        m_valTemp->setStyleSheet("color:#9e9e9e;");
        m_valHumi->setStyleSheet("color:#9e9e9e;");
    }
    // MQ2 原始 ADC 值（0~4095，非真实 ppm）+ 报警提示
    m_valGas->setText(QString("%1%2").arg(d.gas_ppm)
                        .arg(d.gasAlarm() ? "  报警" : ""));
    m_valGas->setStyleSheet(d.gasAlarm() ? "color:#e53935; font-weight:bold;" : "");
    // 超声波距离：F4 真实数据
    m_valDistance->setText(d.distance_cm ? QString("%1 cm").arg(d.distance_cm) : "-- cm");
    // 激光测距（VL53L0X）：F4 真实数据
    m_valLaser->setText((d.laserOk() && d.laser_cm) ? QString("%1 cm").arg(d.laser_cm) : "-- cm");

    // 运动
    m_valEnc1->setText(QString::number(d.encoder1));
    m_valEnc2->setText(QString::number(d.encoder2));
    m_valSpeedL->setText(QString::number(d.speed_L));
    m_valSpeedR->setText(QString::number(d.speed_R));
    m_valServo->setText("未接入");
    m_valVoltage->setText("未接入");
    m_valMode->setText(QString("%1 (%2)").arg(modeName(d.mode)).arg(d.mode));
    // 下拉框回显：让 UI 跟随下位机【真实】模式（可能被龙芯/急停改过），
    // 用 m_modeSyncing 抑制 currentIndexChanged 再把命令发回去，避免回环。
    {
        const int real = d.mode & 0x7F;
        const int idx  = m_cmbMode->findData(real);
        if (idx >= 0 && idx != m_cmbMode->currentIndex()) {
            m_modeSyncing = true;
            m_cmbMode->setCurrentIndex(idx);
            m_modeSyncing = false;
        }
    }
    if (d.fault == 0) {
        m_valFault->setText("正常");
        m_valFault->setStyleSheet("#Value{}");  // 用默认
        m_valFault->setStyleSheet("color:#43a047;");
    } else {
        m_valFault->setText(QString("故障 0x%1").arg(d.fault, 2, 16, QChar('0')));
        m_valFault->setStyleSheet("color:#e53935; font-weight:bold;");
    }

    // 视觉/风险
    static const char *riskTxt[] = { "安全", "注意", "警告", "危险" };
    m_valRisk->setText(riskTxt[qMin<int>(d.risk_level, 3)]);
    auto setBool = [](QLabel *l, bool on, const QString &onTxt, const QString &offTxt) {
        l->setText(on ? onTxt : offTxt);
        l->setStyleSheet(on ? "color:#e53935; font-weight:bold;" : "color:#43a047;");
    };
    // 火焰/气体：F4 传感器（0x5C flags）真实数据；障碍：激光+超声波双重确认
    setBool(m_valFlame,    d.flame(),    "检测到火焰", "正常");
    setBool(m_valGasAlarm, d.gasAlarm(), "气体超阈",   "正常");
    // "烟雾识别"复用为 F4 障碍确认（VL53+超声波双重验证）
    setBool(m_valSmoke,    d.obstacle(), "障碍确认",   "无障碍");
    setRisk(d.risk_level);   // 风险等级：龙芯按超声波距离推导

    // 时间戳
    m_valTimestamp->setText(QString("%1 ms").arg(d.timestamp_ms));

    // 联动状态回读 → 刷新按钮（不触发再次下发）
    setToggle(m_btnFan,    d.fan);
    setToggle(m_btnBuzzer, d.buzzer);
    setToggle(m_btnRelay,  d.relay);
    setToggle(m_btnLed,    d.led);
}

void MainWindow::onVideoFrame(const QImage &img)
{
    m_lastFrame = img;
    if (m_vision->isEnabled())
        m_vision->submitFrame(img);   // 内部背压丢帧，识别慢也不阻塞视频
    refreshVideo();
}

// 用最近一帧 + 最近检测框刷新视频显示（识别关闭时即原始画面）
void MainWindow::refreshVideo()
{
    if (m_lastFrame.isNull()) return;
    if (m_vision->isEnabled() && !m_lastDets.isEmpty())
        m_videoView->setImage(overlayDetections(m_lastFrame, m_lastDets));
    else
        m_videoView->setImage(m_lastFrame);
}

// 在画面上叠加检测框 + 标签（坐标为送入图像像素坐标，与本帧同分辨率）
QImage MainWindow::overlayDetections(const QImage &src, const QVector<Detection> &dets)
{
    QImage img = src.convertToFormat(QImage::Format_RGB888);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, false);
    QFont f = pt.font();
    f.setPointSize(qMax(8, img.height() / 30));
    f.setBold(true);
    pt.setFont(f);
    for (const Detection &d : dets) {
        QRect box(d.x, d.y, d.w, d.h);
        pt.setPen(QPen(QColor("#00e676"), 2));
        pt.drawRect(box);
        QString label = QString("%1 %2%").arg(d.cls).arg(int(d.conf * 100));
        QRect tr = pt.fontMetrics().boundingRect(label).adjusted(-3, -2, 3, 2);
        tr.moveTopLeft(QPoint(d.x, qMax(0, d.y - tr.height())));
        pt.fillRect(tr, QColor(0, 0, 0, 160));
        pt.setPen(QColor("#00e676"));
        pt.drawText(tr, Qt::AlignCenter, label);
    }
    pt.end();
    return img;
}

void MainWindow::onVisionToggled(bool on)
{
    m_vision->setEnabled(on);
    if (!on) {
        m_lastDets.clear();
        refreshVideo();        // 恢复原始画面
    }
    m_lblVision->setText(on ? "视觉: 启动中..." : "视觉: 关闭");
}

void MainWindow::onVisionDetections(const QVector<Detection> &dets)
{
    m_lastDets = dets;
    refreshVideo();

    // 汇总并回传龙芯：龙芯才是"大脑"，由它据此判断（本地仅叠加显示）。
    quint8 flags = 0, maxConf = 0;
    QString topClass;
    for (const Detection &d : dets) {
        int c = qBound(0, int(d.conf * 100), 100);
        if (c >= maxConf) { maxConf = quint8(c); topClass = d.cls; }
        const QString cl = d.cls.toLower();
        if (cl.contains("person") || cl.contains("people") || cl.contains("人"))
            flags |= LL::VIS_PERSON;
        if (cl.contains("fire") || cl.contains("flame") || cl.contains("火"))
            flags |= LL::VIS_FIRE;
    }
    m_net->sendVision(quint8(qMin(dets.size(), 255)), maxConf, flags, topClass);

    m_lblVision->setText(dets.isEmpty()
        ? "视觉: 运行中 · 无目标"
        : QString("视觉: 运行中 · 目标 %1 · %2 %3%")
              .arg(dets.size()).arg(topClass).arg(maxConf));
}

void MainWindow::onVisionStatus(bool running)
{
    if (!running && m_btnVision->isChecked()) {
        // 边车异常退出：复位按钮，避免界面与实际状态不一致
        setToggle(m_btnVision, false);
        m_lblVision->setText("视觉: 关闭（边车已退出）");
        m_lastDets.clear();
        refreshVideo();
    } else if (running) {
        m_lblVision->setText("视觉: 运行中");
    }
}

void MainWindow::onThermalFrame(const QImage &img, double minC, double maxC)
{
    // 时域 EMA 混合：热像帧率低(~1-2fps)，直接刷会跳变卡顿；与上一帧按 0.5 混合，
    // 再平滑放大，观感明显更流畅（下位机帧率不变，纯显示端优化）。
    QImage cur = img.convertToFormat(QImage::Format_RGB888);
    if (m_thermalAccum.size() != cur.size()) {
        m_thermalAccum = cur;
    } else {
        const int n = cur.width() * cur.height() * 3;
        uchar *a = m_thermalAccum.bits();
        const uchar *b = cur.constBits();
        for (int i = 0; i < n; ++i)
            a[i] = uchar((int(a[i]) + int(b[i])) / 2);
    }
    // 平滑放大到较大尺寸，避免像素块状（24x32 -> 更细腻）
    QImage shown = m_thermalAccum.scaled(m_thermalAccum.width() * 8,
                                         m_thermalAccum.height() * 8,
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_thermalView->setImage(shown);
    m_lblThermalRange->setText(QString("量程: %1 ~ %2 °C   (%3x%4)")
                               .arg(minC, 0, 'f', 1).arg(maxC, 0, 'f', 1)
                               .arg(img.width()).arg(img.height()));
}

void MainWindow::onTextFrame(const QString &text)
{
    // 龙芯端日志 -> 独立的"龙芯日志"栏
    m_logRemote->append(QString("[%1] %2")
                        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(text));

    // 解析龙芯 statusLine 的 RX 计数（形如 "RX[tele=123 env=45 ...]"）：
    //   计数在涨 = "F4→龙芯"链路活跃；停增 = F4 静默/串口死。据此可与"龙芯→PC"问题区分：
    //   若本栏 tele/env 在涨、但左侧遥测帧率=0，则问题在龙芯→上位机转发，而非 F4。
    static const QRegularExpression reTele("tele=(\\d+)");
    static const QRegularExpression reEnv("env=(\\d+)");
    const auto mt = reTele.match(text);
    const auto me = reEnv.match(text);
    if (mt.hasMatch() && me.hasMatch()) {
        const quint32 tele = mt.captured(1).toUInt();
        const quint32 env  = me.captured(1).toUInt();
        if (m_remoteRxInit) {
            const int dTele = int(tele - m_remoteTele);
            const int dEnv  = int(env - m_remoteEnv);
            const bool alive = (dTele > 0 || dEnv > 0);
            m_valRemoteRx->setText(QString("tele=%1(+%2) env=%3(+%4)")
                                   .arg(tele).arg(dTele).arg(env).arg(dEnv));
            m_valRemoteRx->setStyleSheet(alive ? "color:#43a047;"
                                               : "color:#e53935; font-weight:bold;");
        } else {
            m_valRemoteRx->setText(QString("tele=%1 env=%2").arg(tele).arg(env));
        }
        m_remoteTele = tele; m_remoteEnv = env; m_remoteRxInit = true;
    }
}

void MainWindow::onRateTick()
{
    const int hz = m_sensorCount;
    m_valSensorHz->setText(QString("%1 Hz").arg(hz));
    m_sensorCount = 0;

    // ★链路健康：连着却长时间收不到传感器帧 = F4 数据冻结（正是"连接在、数值不动"的掉线）。
    if (!m_net->isConnected()) {
        m_valLink->setText("○ 未连接");
        m_valLink->setStyleSheet("color:#8a94a8;");
        return;
    }
    const qint64 age = m_lastSensorMs ? (QDateTime::currentMSecsSinceEpoch() - m_lastSensorMs) : -1;
    if (age < 0) {
        m_valLink->setText("● 已连接 · 等待F4数据");
        m_valLink->setStyleSheet("color:#ffb020; font-weight:bold;");
    } else if (age < 1500) {
        m_valLink->setText(QString("● 正常 · %1 Hz").arg(hz));
        m_valLink->setStyleSheet("color:#43a047; font-weight:bold;");
    } else {
        m_valLink->setText(QString("● F4数据冻结 %1s").arg(age / 1000));
        m_valLink->setStyleSheet("color:#e53935; font-weight:bold;");
    }
}

// ─── 控制命令 ────────────────────────────────────────────────────
void MainWindow::setToggle(QPushButton *b, bool on)
{
    if (!b) return;
    b->blockSignals(true);
    b->setChecked(on);
    b->blockSignals(false);
}

void MainWindow::onFanToggled(bool on)    { m_net->sendCommand(LL::CMD_FAN,    on ? 1 : 0); }
void MainWindow::onBuzzerToggled(bool on) { m_net->sendCommand(LL::CMD_BUZZER, on ? 1 : 0); }
void MainWindow::onRelayToggled(bool on)  { m_net->sendCommand(LL::CMD_RELAY,  on ? 1 : 0); }
void MainWindow::onLedToggled(bool on)    { m_net->sendCommand(LL::CMD_LED,    on ? 1 : 0); }
// ★ 三模式值为 0/1/3（不连续），必须取 itemData 而非下拉索引，否则"循迹+视觉"会误发成 2(避障)。
void MainWindow::onModeChanged(int index)
{
    if (m_modeSyncing) return;        // 回显同步引起的变更，不要再发回下位机
    const int mode = m_cmbMode->itemData(index).toInt();
    m_net->sendCommand(LL::CMD_MODE, quint8(mode));
    onLog(QString("切换工作模式 -> %1 (值=%2)").arg(m_cmbMode->itemText(index)).arg(mode));
}
void MainWindow::onEstop()
{
    m_net->sendCommand(LL::CMD_ESTOP, 1);
    m_net->sendDrive(0, 0);
    onLog("已下发紧急停止");
}

void MainWindow::onOpenRemotePanel()
{
    if (!m_remotePanel) {
        m_remotePanel = new RemotePanel(m_net);      // 顶层窗口，复用同一 NetClient
        m_remotePanel->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_remotePanel->show();
    m_remotePanel->raise();
    m_remotePanel->activateWindow();
}
