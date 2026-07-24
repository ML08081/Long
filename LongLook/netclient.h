#ifndef NETCLIENT_H
#define NETCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QByteArray>
#include <QImage>
#include <QTimer>
#include "protocol.h"

/*
 * NetClient — 龙芯 WiFi/TCP 客户端 + 帧解析器
 *
 * 连接龙芯的 TCP 服务端，按 protocol.h 定义的帧格式解析流式数据，
 * 分类派发为：传感器遥测 / 视频帧 / 热成像帧 / 文本。
 * 解析在 readyRead 中累积缓冲并循环抽取完整帧，正确处理 TCP 粘包/拆包。
 */
class NetClient : public QObject
{
    Q_OBJECT
public:
    explicit NetClient(QObject *parent = nullptr);
    ~NetClient();

    void connectToHost(const QString &ip, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    // 启动 UDP 自动发现：监听龙芯广播("PATROL|版本|IP|端口")，发现后 emit discovered()。
    //   龙芯 IP 变化(如切到手机热点 DHCP)也能自动跟上，无需死记固定 IP。
    void startDiscovery(quint16 port);

    // 前端 → 龙芯 下行（均为 TCP 侧 0xA5 帧；F4 底层帧由龙芯打包，前端不涉及）
    bool sendRaw(const QByteArray &data);
    bool sendCommand(quint8 cmdId, quint8 value);        // 执行与联动 / 模式 / 急停
    bool sendDrive(qint16 speed, qint16 steering);       // 手动驱动意图（龙芯据此转发 F4）
    // 视觉识别结果回传龙芯（供"大脑"判断）
    bool sendVision(quint8 count, quint8 maxConf, quint8 flags, const QString &topClass);

signals:
    void connected();
    void disconnected();
    // 连接尝试进行中（第 attempt/maxAttempts 次）——供 UI 显示"连接中/重试中"状态
    void connecting(int attempt, int maxAttempts);
    void logMessage(const QString &msg);
    void sensorUpdated(const LL::SensorData &data);
    void videoFrame(const QImage &img);
    void thermalFrame(const QImage &img, double minC, double maxC);
    void textFrame(const QString &text);
    // 局域网发现到龙芯（UDP 广播解析结果）
    void discovered(const QString &ip, quint16 port, const QString &version);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onConnectTimeout();
    void onDiscoveryDatagram();      // 收到龙芯 UDP 广播

private:
    void parseBuffer();
    void dispatch(quint8 type, const QByteArray &payload);
    void parseSensor(const QByteArray &payload);
    void parseThermal(const QByteArray &payload);

    void attemptConnect();                           // 发起一次连接尝试（含超时计时）
    void scheduleRetryOrFail(const QString &reason); // 连接失败：自动重试或最终报错

    // 连接可靠性参数：虚拟网卡(VMware/WSL)环境下首次握手常瞬时失败，
    // 用"超时 + 自动重试"把手动重连自动化，做到点一次即连上。
    static constexpr int kMaxAttempts     = 5;    // 最多尝试次数
    static constexpr int kConnectTimeoutMs = 2500; // 单次连接超时
    static constexpr int kRetryDelayMs     = 400;  // 重试间隔

    QTcpSocket *m_socket = nullptr;
    QUdpSocket *m_discovery = nullptr;   // UDP 自动发现监听
    QByteArray  m_buf;          // 接收累积缓冲
    QString     m_ip;
    quint16     m_port = 0;

    QTimer     *m_connectTimer = nullptr;  // 单次连接超时计时器
    bool        m_connecting   = false;    // 正在连接阶段（区分建链后断开）
    int         m_attempt      = 0;        // 当前已尝试次数
};

#endif // NETCLIENT_H
