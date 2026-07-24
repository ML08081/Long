#include "netclient.h"
#include <QtEndian>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QHostAddress>
#include <algorithm>

// ─── 热成像伪彩映射：t∈[0,1] → 蓝-青-绿-黄-红 ────────────────────────────
static QRgb heatColor(double t)
{
    t = qBound(0.0, t, 1.0);
    double r, g, b;
    if (t < 0.25)      { r = 0;              g = 4.0 * t;          b = 1.0; }
    else if (t < 0.50) { r = 0;              g = 1.0;             b = 1.0 - 4.0 * (t - 0.25); }
    else if (t < 0.75) { r = 4.0 * (t - 0.5); g = 1.0;             b = 0; }
    else               { r = 1.0;            g = 1.0 - 4.0 * (t - 0.75); b = 0; }
    return qRgb(int(r * 255), int(g * 255), int(b * 255));
}

NetClient::NetClient(QObject *parent) : QObject(parent)
{
    // 全局禁用系统代理：VMware/WSL 等虚拟网卡 + 系统代理环境下，Qt 的代理子系统
    // 会对原始 TCP 直连报 "The proxy type is invalid for this operation" 或引入
    // 间歇性查询延迟。进程级关闭系统代理配置，是虚拟网卡环境最稳的根治手段。
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    m_socket = new QTcpSocket(this);
    m_socket->setProxy(QNetworkProxy::NoProxy);   // 再对本 socket 显式无代理（双保险）
    connect(m_socket, &QTcpSocket::connected,    this, &NetClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &NetClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetClient::onError);

    // 单次连接超时计时器：超时即判定本次尝试失败，触发自动重试。
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &NetClient::onConnectTimeout);
}

NetClient::~NetClient()
{
    disconnectFromHost();
}

void NetClient::connectToHost(const QString &ip, quint16 port)
{
    m_ip = ip;
    m_port = port;
    m_attempt = 0;              // 新的一次连接请求，重置重试计数
    attemptConnect();
}

// ── UDP 自动发现：监听龙芯广播，龙芯 IP 变化也能自动跟上 ──────────────────────
void NetClient::startDiscovery(quint16 port)
{
    if (m_discovery) return;    // 已启动
    m_discovery = new QUdpSocket(this);
    // ShareAddress：允许同机多开 LongLook 共享监听发现端口，不互相占用
    if (!m_discovery->bind(QHostAddress::AnyIPv4, port,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logMessage(QString("UDP 自动发现启动失败（端口 %1 被占用？）").arg(port));
        m_discovery->deleteLater();
        m_discovery = nullptr;
        return;
    }
    connect(m_discovery, &QUdpSocket::readyRead, this, &NetClient::onDiscoveryDatagram);
    emit logMessage(QString("UDP 自动发现已启动：监听龙芯广播（端口 %1）").arg(port));
}

void NetClient::onDiscoveryDatagram()
{
    while (m_discovery && m_discovery->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(int(m_discovery->pendingDatagramSize()));
        m_discovery->readDatagram(buf.data(), buf.size());
        // 期望格式: "PATROL|<version>|<ip>|<tcp_port>"
        const QString s = QString::fromUtf8(buf).trimmed();
        if (!s.startsWith("PATROL|")) continue;
        const QStringList parts = s.split('|');
        if (parts.size() < 4) continue;
        const QString version = parts.at(1);
        const QString ip      = parts.at(2);
        bool ok = false;
        const quint16 port = quint16(parts.at(3).toUInt(&ok));
        if (!ok || ip.isEmpty() || ip == QLatin1String("0.0.0.0")) continue;
        emit discovered(ip, port, version);
    }
}

void NetClient::attemptConnect()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_buf.clear();
    m_connecting = true;
    ++m_attempt;
    m_socket->setProxy(QNetworkProxy::NoProxy);   // 每次尝试前确保无代理
    emit connecting(m_attempt, kMaxAttempts);
    emit logMessage(QString("正在连接开发板 %1:%2 ...（尝试 %3/%4）")
                    .arg(m_ip).arg(m_port).arg(m_attempt).arg(kMaxAttempts));
    m_connectTimer->start(kConnectTimeoutMs);
    m_socket->connectToHost(m_ip, m_port);
}

void NetClient::scheduleRetryOrFail(const QString &reason)
{
    if (!m_connecting) return;    // 已成功或已处理，忽略（防止 abort 触发的二次进入）
    m_connecting = false;
    m_connectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    if (m_attempt < kMaxAttempts) {
        emit logMessage(QString("连接失败（%1），%2ms 后自动重试…")
                        .arg(reason).arg(kRetryDelayMs));
        QTimer::singleShot(kRetryDelayMs, this, [this]{ attemptConnect(); });
    } else {
        emit logMessage(QString("连接失败（%1），已尝试 %2 次仍无法连接开发板，请检查板子/网络后重试")
                        .arg(reason).arg(kMaxAttempts));
        emit disconnected();      // 通知 UI 复位连接按钮状态
    }
}

void NetClient::disconnectFromHost()
{
    m_connecting = false;
    m_attempt = 0;
    m_connectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool NetClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool NetClient::sendRaw(const QByteArray &data)
{
    if (!isConnected()) return false;
    qint64 n = m_socket->write(data);
    m_socket->flush();
    return n == data.size();
}

bool NetClient::sendCommand(quint8 cmdId, quint8 value)
{
    QByteArray payload;
    payload.append(char(cmdId));
    payload.append(char(value));

    QByteArray frame;
    frame.append(char(LL::SOF));
    frame.append(char(LL::FRAME_COMMAND));
    uchar lenLE[4];
    qToLittleEndian<quint32>(quint32(payload.size()), lenLE);
    frame.append(reinterpret_cast<const char *>(lenLE), 4);
    frame.append(payload);

    bool ok = sendRaw(frame);
    emit logMessage(QString("下发命令 cmd=0x%1 val=%2 %3")
                    .arg(cmdId, 2, 16, QChar('0')).arg(value)
                    .arg(ok ? "OK" : "未连接"));
    return ok;
}

bool NetClient::sendDrive(qint16 speed, qint16 steering)
{
    // payload：speed i16 LE + steering i16 LE（龙芯据此切 MANUAL 并转发同步命令给 F4）
    QByteArray payload;
    uchar le[4];
    qToLittleEndian<qint16>(speed,    le);
    qToLittleEndian<qint16>(steering, le + 2);
    payload.append(reinterpret_cast<const char *>(le), 4);

    QByteArray frame;
    frame.append(char(LL::SOF));
    frame.append(char(LL::FRAME_DRIVE));
    uchar lenLE[4];
    qToLittleEndian<quint32>(quint32(payload.size()), lenLE);
    frame.append(reinterpret_cast<const char *>(lenLE), 4);
    frame.append(payload);

    bool ok = sendRaw(frame);
    emit logMessage(QString("下发驱动 speed=%1 steering=%2 %3")
                    .arg(speed).arg(steering).arg(ok ? "OK" : "未连接"));
    return ok;
}

bool NetClient::sendVision(quint8 count, quint8 maxConf, quint8 flags, const QString &topClass)
{
    if (!isConnected()) return false;
    QByteArray name = topClass.left(64).toUtf8();   // 限长，避免过大

    QByteArray payload;
    payload.append(char(count));
    payload.append(char(maxConf));
    payload.append(char(flags));
    payload.append(char(quint8(name.size())));
    payload.append(name);

    QByteArray frame;
    frame.append(char(LL::SOF));
    frame.append(char(LL::FRAME_VISION));
    uchar lenLE[4];
    qToLittleEndian<quint32>(quint32(payload.size()), lenLE);
    frame.append(reinterpret_cast<const char *>(lenLE), 4);
    frame.append(payload);
    return sendRaw(frame);
}

void NetClient::onConnected()
{
    m_connecting = false;
    m_connectTimer->stop();
    m_attempt = 0;
    // 建链后开启低延迟 + keepalive：龙芯断电/WiFi 掉线时不会发 FIN，
    // 靠 keepalive 探测让本端及时 disconnected，而不是长时间"看似已连接却无数据"。
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    emit logMessage(QString("已连接开发板 %1:%2").arg(m_ip).arg(m_port));
    emit connected();
}

void NetClient::onDisconnected()
{
    m_buf.clear();
    // 建链后被动断开才提示；连接阶段的失败由 scheduleRetryOrFail 处理
    if (!m_connecting) {
        emit logMessage("连接已断开");
        emit disconnected();
    }
}

void NetClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_connecting) {
        // 连接阶段出错 → 自动重试（虚拟网卡首次握手常瞬时失败）
        scheduleRetryOrFail(m_socket->errorString());
    } else {
        // 已建链后的错误 → 仅记录，后续 disconnected 会处理
        emit logMessage(QString("网络错误: %1").arg(m_socket->errorString()));
    }
}

void NetClient::onConnectTimeout()
{
    if (!m_connecting) return;
    scheduleRetryOrFail(QStringLiteral("连接超时"));
}

void NetClient::onReadyRead()
{
    m_buf.append(m_socket->readAll());
    parseBuffer();
}

// ─── 流式帧解析：处理 TCP 粘包/拆包，按 SOF 重新同步 ──────────────────────
void NetClient::parseBuffer()
{
    for (;;) {
        // 1) 同步到 SOF
        int sof = m_buf.indexOf(char(LL::SOF));
        if (sof < 0) { m_buf.clear(); return; }      // 无 SOF，丢弃全部
        if (sof > 0) m_buf.remove(0, sof);            // 丢弃 SOF 之前的杂数据

        // 2) 头部是否完整
        if (m_buf.size() < LL::HEADER_SIZE) return;   // 等待更多数据

        const uchar *p = reinterpret_cast<const uchar *>(m_buf.constData());
        quint8  type = p[1];
        quint32 len  = qFromLittleEndian<quint32>(p + 2);

        // 3) 长度异常 → 视为失步，丢一个字节重新找 SOF
        if (len > LL::MAX_PAYLOAD) {
            m_buf.remove(0, 1);
            continue;
        }

        // 4) 负载是否完整
        if (m_buf.size() < LL::HEADER_SIZE + int(len)) return; // 等待更多数据

        QByteArray payload = m_buf.mid(LL::HEADER_SIZE, int(len));
        m_buf.remove(0, LL::HEADER_SIZE + int(len));
        dispatch(type, payload);
    }
}

void NetClient::dispatch(quint8 type, const QByteArray &payload)
{
    switch (type) {
    case LL::FRAME_SENSOR:
        parseSensor(payload);
        break;
    case LL::FRAME_VIDEO: {
        QImage img;
        if (img.loadFromData(payload)) {      // 自动识别 JPEG/PNG/BMP
            emit videoFrame(img);
        } else {
            emit logMessage(QString("视频帧解码失败 (%1 字节)").arg(payload.size()));
        }
        break;
    }
    case LL::FRAME_THERMAL:
        parseThermal(payload);
        break;
    case LL::FRAME_TEXT:
        emit textFrame(QString::fromUtf8(payload));
        break;
    default:
        emit logMessage(QString("未知帧类型 0x%1 (%2 字节)")
                        .arg(type, 2, 16, QChar('0')).arg(payload.size()));
        break;
    }
}

void NetClient::parseSensor(const QByteArray &payload)
{
    // 变长容忍：按规范顺序逐字段读取，越界字段取 0（便于协议后续扩展/裁剪）
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());
    const int n = payload.size();
    int off = 0;
    auto rdU8  = [&]() -> quint8  { quint8  v = (off + 1 <= n) ? p[off] : 0;                          off += 1; return v; };
    auto rdI16 = [&]() -> qint16  { qint16  v = (off + 2 <= n) ? qFromLittleEndian<qint16 >(p + off) : 0; off += 2; return v; };
    auto rdU16 = [&]() -> quint16 { quint16 v = (off + 2 <= n) ? qFromLittleEndian<quint16>(p + off) : 0; off += 2; return v; };
    auto rdI32 = [&]() -> qint32  { qint32  v = (off + 4 <= n) ? qFromLittleEndian<qint32 >(p + off) : 0; off += 4; return v; };
    auto rdU32 = [&]() -> quint32 { quint32 v = (off + 4 <= n) ? qFromLittleEndian<quint32>(p + off) : 0; off += 4; return v; };

    LL::SensorData s;
    s.timestamp_ms    = rdU32();
    s.temperature_01c = rdI16();
    s.humidity_01     = rdU16();
    s.gas_ppm         = rdU16();
    s.pressure_pa     = rdU32();
    s.distance_cm     = rdU16();
    s.encoder1        = rdI32();
    s.encoder2        = rdI32();
    s.speed_L         = rdI16();
    s.speed_R         = rdI16();
    s.servo_us        = rdU16();
    s.voltage_mV      = rdU16();
    s.mode            = rdU8();
    s.fault           = rdU8();
    s.risk_level      = rdU8();
    s.flags           = rdU8();
    s.fan             = rdU8();
    s.buzzer          = rdU8();
    s.relay           = rdU8();
    s.led             = rdU8();
    s.laser_cm        = rdU16();   // v3 扩展：越界(旧龙芯40B)自动取 0
    emit sensorUpdated(s);
}

void NetClient::parseThermal(const QByteArray &payload)
{
    if (payload.size() < 4) return;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());
    quint16 w = qFromLittleEndian<quint16>(p + 0);
    quint16 h = qFromLittleEndian<quint16>(p + 2);
    if (w == 0 || h == 0) return;

    const int need = 4 + int(w) * int(h) * 2;
    if (payload.size() < need) {
        emit logMessage(QString("热成像帧过短: %1/%2 字节").arg(payload.size()).arg(need));
        return;
    }

    const int n = int(w) * int(h);
    const uchar *d = p + 4;

    // 自动量程：找最小/最大温度（单位 0.01°C）
    qint16 vmin = 32767, vmax = -32768;
    for (int i = 0; i < n; ++i) {
        qint16 v = qFromLittleEndian<qint16>(d + i * 2);
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    const double span = (vmax > vmin) ? double(vmax - vmin) : 1.0;

    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            qint16 v = qFromLittleEndian<qint16>(d + (y * w + x) * 2);
            double t = (double(v) - vmin) / span;
            QRgb c = heatColor(t);
            img.setPixel(x, y, c);
        }
    }
    emit thermalFrame(img, vmin / 100.0, vmax / 100.0);
}
