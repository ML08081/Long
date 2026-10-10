#include "bledevicemanager.h"
#include <QBluetoothAddress>
#include <QLowEnergyDescriptor>
#include <QCoreApplication>
#include <QDebug>

// ─── NUS (Nordic UART Service) 标准 UUID ──────────────────────
static const QBluetoothUuid NUS_SERVICE(QString("6e400001-b5a3-f393-e0a9-e50e24dcca9e"));
static const QBluetoothUuid NUS_TX_CHAR (QString("6e400002-b5a3-f393-e0a9-e50e24dcca9e"));
static const QBluetoothUuid NUS_RX_CHAR (QString("6e400003-b5a3-f393-e0a9-e50e24dcca9e"));

// ─── 静态接口 ─────────────────────────────────────────────────
QBluetoothUuid BleManager::nusServiceUuid() { return NUS_SERVICE; }
QBluetoothUuid BleManager::nusTxCharUuid()  { return NUS_TX_CHAR; }
QBluetoothUuid BleManager::nusRxCharUuid()  { return NUS_RX_CHAR; }

// ═══════════════════════════════════════════════════════════════
BleManager::BleManager(QObject *parent)
    : QObject(parent)
{
    m_scanTimer = new QTimer(this);
    m_scanTimer->setSingleShot(true);
    connect(m_scanTimer, &QTimer::timeout, this, &BleManager::onScanTimeout);

    m_keepaliveTimer = new QTimer(this);
    m_keepaliveTimer->setSingleShot(false);
    connect(m_keepaliveTimer, &QTimer::timeout, this, [this]() {
        // BLE 连接由协议栈维持，无需应用层保活。
        // 切勿在此向透传特征写 "AT\r\n"——连接态下模块不解析 AT，
        // 这些字节会原样注入 STM32 数据流，污染协议解析。
    });

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_connected || m_pendingAddress.isEmpty()) return;
        m_reconnectAttempts++;
        QString addr = m_pendingAddress;
        m_pendingAddress.clear();
        emit statusMessage(QString("🔄 正在自动重连 (第 %1 次)...").arg(m_reconnectAttempts));
        connectToDevice(addr);
    });

    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, [this]() {
        if (m_connected) return;
        emit connectionError("连接超时，设备不在范围内或已关机");
        emit statusMessage("❌ 连接超时，请确认模块已上电且在范围内");
        clearController();
    });
}

BleManager::~BleManager()
{
    disconnectFromDevice();
    delete m_discoveryAgent;
}

// ═══════════════════════════════════════════════════════════════
//  扫描
// ═══════════════════════════════════════════════════════════════

void BleManager::startScan(int timeoutMs)
{
    resetDiscovery();

    if (m_discoveryAgent) {
        if (m_discoveryAgent->isActive()) {
            m_discoveryAgent->stop();
        }
        m_discoveryAgent->deleteLater();
        m_discoveryAgent = nullptr;
    }
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BleManager::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BleManager::onScanFinished);
    connect(m_discoveryAgent,
            static_cast<void(QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(
                &QBluetoothDeviceDiscoveryAgent::error),
            this, &BleManager::onScanError);

    m_discoveryAgent->setLowEnergyDiscoveryTimeout(timeoutMs);
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);

    m_scanning = true;
    emit scanStarted();
    emit statusMessage(QString("⏳ 正在扫描 BLE 设备（超时 %1 秒）...")
                       .arg(timeoutMs / 1000));

    if (timeoutMs > 0) {
        m_scanTimer->start(timeoutMs + 2000);
    }
}

void BleManager::stopScan()
{
    if (m_discoveryAgent && m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
    m_scanTimer->stop();
    m_scanning = false;
}

bool BleManager::isScanning() const
{
    return m_scanning;
}

void BleManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    QString name = info.name().trimmed();
    QString address = info.address().toString();

    if (name.isEmpty()) return;
    if (m_deviceAddresses.contains(address)) return;

    int rssi = info.rssi();
    m_deviceNames << name;
    m_deviceAddresses << address;
    m_deviceRssis << rssi;

    emit deviceDiscovered(name, address, rssi);
}

void BleManager::onScanFinished()
{
    m_scanning = false;
    m_scanTimer->stop();
    emit scanFinished();
    emit statusMessage(QString("🔍 扫描完成，发现 %1 个设备").arg(m_deviceNames.size()));
}

void BleManager::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    Q_UNUSED(error);
    m_scanning = false;
    m_scanTimer->stop();
    QString err = m_discoveryAgent ? m_discoveryAgent->errorString() : "未知错误";
    emit scanError(err);
    emit statusMessage(QString("❌ 扫描错误: %1").arg(err));
}

void BleManager::onScanTimeout()
{
    if (m_discoveryAgent && m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
    m_scanning = false;
    emit scanFinished();
    emit statusMessage(QString("⏰ 扫描超时，发现 %1 个设备").arg(m_deviceNames.size()));
}

// ═══════════════════════════════════════════════════════════════
//  连接
// ═══════════════════════════════════════════════════════════════

void BleManager::connectToDevice(const QString &address)
{
    if (m_connected) {
        emit statusMessage("⚠️ 已连接到其他设备，请先断开");
        return;
    }

    QBluetoothDeviceInfo deviceInfo(QBluetoothAddress(address), "", 0);
    clearController();

    m_controller = QLowEnergyController::createCentral(deviceInfo, this);
    if (!m_controller) {
        emit connectionError("无法创建 BLE 控制器");
        emit statusMessage("❌ 无法创建 BLE 控制器");
        return;
    }

    connect(m_controller, &QLowEnergyController::connected,
            this, &BleManager::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &BleManager::onControllerDisconnected);
    connect(m_controller,
            static_cast<void(QLowEnergyController::*)(QLowEnergyController::Error)>(
                &QLowEnergyController::error),
            this, &BleManager::onControllerError);
    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &BleManager::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BleManager::onServiceScanDone);

    m_connectedAddress = address;
    m_connectedName = address;

    int idx = m_deviceAddresses.indexOf(address);
    if (idx >= 0 && idx < m_deviceNames.size()) {
        m_connectedName = m_deviceNames[idx];
    }

    emit statusMessage(QString("🔗 正在连接 %1 (%2)...").arg(m_connectedName, address));
    m_connectTimer->start(10000);
    m_controller->connectToDevice();
}

void BleManager::disconnectFromDevice()
{
    m_connected = false;
    m_serviceFound = false;
    m_keepaliveTimer->stop();
    m_reconnectTimer->stop();
    m_connectTimer->stop();
    m_pendingAddress.clear();
    m_reconnectAttempts = 0;

    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }

    if (m_controller) {
        if (m_controller->state() != QLowEnergyController::UnconnectedState) {
            m_controller->disconnectFromDevice();
        }
        m_controller->deleteLater();
        m_controller = nullptr;
    }

    m_connectedAddress.clear();
    m_connectedName.clear();
    m_txChar = QLowEnergyCharacteristic();
    m_rxChar = QLowEnergyCharacteristic();
    m_discoveredServices.clear();
    m_serviceIndex = 0;

    emit disconnected();
    emit statusMessage("📴 BLE 已断开");
}

bool BleManager::isConnected() const
{
    return m_connected && m_serviceFound;
}

void BleManager::onControllerConnected()
{
    m_connectTimer->stop();

    if (!m_controller || m_controller->state() != QLowEnergyController::ConnectedState) {
        emit connectionError("连接状态异常，可能是缓存连接");
        emit statusMessage("❌ 连接状态异常，设备可能不在范围内");
        clearController();
        return;
    }
    emit statusMessage("✅ BLE 已连接（已验证），正在发现服务...");
    if (m_controller) {
        m_controller->discoverServices();
    }
}

void BleManager::onControllerDisconnected()
{
    bool wasConnected = m_connected;
    m_connected = false;
    m_serviceFound = false;
    m_keepaliveTimer->stop();

    if (wasConnected) {
        emit disconnected();
        emit statusMessage("📴 BLE 连接已断开");
        if (!m_connectedAddress.isEmpty() && m_reconnectAttempts < 3) {
            m_pendingAddress = m_connectedAddress;
            int delay = 2000 + m_reconnectAttempts * 1000;
            m_reconnectTimer->start(delay);
            emit statusMessage(QString("🔄 %1 秒后自动重连...").arg(delay / 1000));
        }
    }
}

void BleManager::onControllerError(QLowEnergyController::Error error)
{
    Q_UNUSED(error);
    QString err = m_controller ? m_controller->errorString() : "未知错误";
    emit connectionError(err);
    emit statusMessage(QString("❌ BLE 错误: %1").arg(err));
}

void BleManager::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    if (uuid == NUS_SERVICE) {
        emit statusMessage("📡 发现 NUS UART 服务!");
    } else {
        emit statusMessage(QString("  发现服务: %1").arg(uuid.toString()));
    }
}

void BleManager::onServiceScanDone()
{
    if (!m_controller) return;

    m_discoveredServices = m_controller->services();
    m_serviceIndex = 0;
    emit statusMessage(QString("📋 发现 %1 个服务:").arg(m_discoveredServices.size()));

    for (const QBluetoothUuid &uuid : m_discoveredServices) {
        QString flag = (uuid == NUS_SERVICE) ? "  ⭐ NUS!" : "";
        emit statusMessage(QString("  ─ %1%2").arg(uuid.toString(), flag));
    }

    if (m_discoveredServices.isEmpty()) {
        emit connectionError("设备无可用服务");
        emit statusMessage("❌ 设备无可用服务");
        return;
    }

    for (int i = 0; i < m_discoveredServices.size(); ++i) {
        if (m_discoveredServices[i] == NUS_SERVICE) {
            m_serviceIndex = i;
            break;
        }
    }

    tryService(m_serviceIndex);
}

void BleManager::tryService(int index)
{
    if (!m_controller || index >= m_discoveredServices.size()) {
        emit connectionError("所有服务均无可用 RX 写特征");
        emit statusMessage("❌ 所有服务均无可用 RX 写特征，设备不兼容");
        return;
    }

    m_serviceIndex = index;
    QBluetoothUuid uuid = m_discoveredServices[index];

    emit statusMessage(QString("🔍 尝试服务 %1/%2: %3")
                       .arg(index + 1).arg(m_discoveredServices.size())
                       .arg(uuid.toString()));

    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }

    m_service = m_controller->createServiceObject(uuid);
    if (!m_service) {
        emit statusMessage("  ⚠️ 无法创建服务对象，尝试下一个...");
        tryService(index + 1);
        return;
    }

    connect(m_service, &QLowEnergyService::stateChanged,
            this, &BleManager::onServiceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicChanged,
            this, &BleManager::onCharacteristicChanged);
    connect(m_service, &QLowEnergyService::descriptorWritten,
            this, &BleManager::onDescriptorWritten);

    // 写入确认（仅“带响应写”会触发）：用于区分“GATT 写已到模块” vs “模块未转发到 UART”
    connect(m_service, &QLowEnergyService::characteristicWritten, this,
            [this](const QLowEnergyCharacteristic &c, const QByteArray &v) {
                Q_UNUSED(c);
                emit statusMessage(QString("✅ 写入已确认(模块已收到 GATT 写, %1 字节)").arg(v.size()));
            });
    // 服务级错误（写失败等）
    connect(m_service,
            static_cast<void(QLowEnergyService::*)(QLowEnergyService::ServiceError)>(&QLowEnergyService::error),
            this, [this](QLowEnergyService::ServiceError e) {
                emit statusMessage(QString("⚠️ 服务错误(写入可能失败): code=%1").arg(static_cast<int>(e)));
            });

    m_service->discoverDetails();
}

void BleManager::onServiceStateChanged(QLowEnergyService::ServiceState newState)
{
    if (newState == QLowEnergyService::ServiceDiscovered) {
        if (!m_service) return;

        QList<QLowEnergyCharacteristic> chars = m_service->characteristics();
        emit statusMessage(QString("📋 发现 %1 个特征:").arg(chars.size()));

        // ── 按“属性”识别收发特征（不要按 UUID 标签，避免 NUS TX/RX 命名歧义）──
        //  m_rxChar = 我们要“写入”的特征：必须含 Write / WriteWithoutResponse
        //      标准 NUS = 6e400002；模块据此把数据从自身 UART TX 发往 STM32 的 RX(PB11)
        //  m_txChar = 我们要“订阅通知”的特征：必须含 Notify / Indicate
        //      标准 NUS = 6e400003；用于接收 STM32 回传的遥测帧
        //  注：HM-10/CC2541(FFE0) 模块的单个 FFE1 特征同时具备 Write+Notify，本逻辑同样适用。
        m_rxChar = QLowEnergyCharacteristic();
        m_txChar = QLowEnergyCharacteristic();
        bool rxIsCombined = false;   // 当前写特征是否“可写且可通知”(透传管道, 如 FFE1)
        for (const QLowEnergyCharacteristic &ch : chars) {
            QBluetoothUuid uuid = ch.uuid();
            QString props;
            if (ch.properties() & QLowEnergyCharacteristic::Read) props += "Read ";
            if (ch.properties() & QLowEnergyCharacteristic::Write) props += "Write ";
            if (ch.properties() & QLowEnergyCharacteristic::WriteNoResponse) props += "WriteNoRsp ";
            if (ch.properties() & QLowEnergyCharacteristic::Notify) props += "Notify ";
            if (ch.properties() & QLowEnergyCharacteristic::Indicate) props += "Indicate ";

            emit statusMessage(QString("  ─ %1  [%2]").arg(uuid.toString(), props));

            bool canWrite  = ch.properties() & (QLowEnergyCharacteristic::Write |
                                                QLowEnergyCharacteristic::WriteNoResponse);
            bool canNotify = ch.properties() & (QLowEnergyCharacteristic::Notify |
                                                QLowEnergyCharacteristic::Indicate);

            // 通知特征（← 接收遥测）：含 Notify/Indicate
            if (canNotify) {
                m_txChar = ch;
                emit statusMessage(QString("    ✅ 通知特征(接收遥测): %1").arg(uuid.toString()));
            }
            // 写特征（→ 发送命令）：优先选“可写且可通知”的透传管道(HM-10 FFE1)，
            // 否则才用纯写特征(NUS 6e400002)。这样不会被 FFE2 之类的纯写特征误覆盖。
            if (canWrite) {
                if (canNotify && !rxIsCombined) {
                    m_rxChar = ch;
                    rxIsCombined = true;
                    emit statusMessage(QString("    ✅ 写特征(发送命令, 透传管道): %1").arg(uuid.toString()));
                } else if (!rxIsCombined && !m_rxChar.isValid()) {
                    m_rxChar = ch;
                    emit statusMessage(QString("    ✅ 写特征(发送命令): %1").arg(uuid.toString()));
                }
            }
        }

        if (!m_rxChar.isValid()) {
            emit statusMessage("  ⚠️ 该服务无 RX 写特征");
            if (m_service) {
                m_service->deleteLater();
                m_service = nullptr;
            }
            tryService(m_serviceIndex + 1);
            return;
        }

        if (m_txChar.isValid()) {
            QLowEnergyDescriptor cccd = m_txChar.descriptor(
                QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
            if (cccd.isValid()) {
                m_service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
                emit statusMessage("📩 已启用 TX 通知");
            } else {
                emit statusMessage("⚠️ 未找到 CCCD 描述符，通知可能无法工作");
            }
        }

        bool hasNus = false;
        for (const QBluetoothUuid &svc : m_discoveredServices) {
            if (svc == NUS_SERVICE) {
                hasNus = true;
                break;
            }
        }
        if (!hasNus) {
            QBluetoothUuid ffe0(QString("0000ffe0-0000-1000-8000-00805f9b34fb"));
            for (const QBluetoothUuid &svc : m_discoveredServices) {
                if (svc == ffe0) {
                    hasNus = true;
                    break;
                }
            }
        }
        if (!hasNus) {
            emit statusMessage("⚠️ 警告: 该设备未发现标准 UART 透传服务 (6E400001 或 FFE0)");
            emit statusMessage("⚠️ 请确认连接的是正确的 BLE UART 模块，而非其他蓝牙设备");
        }

        m_connected = true;
        m_serviceFound = true;
        m_reconnectAttempts = 0;
        m_pendingAddress.clear();
        emit connected();

        m_keepaliveTimer->start(30000);

        QString devName = m_connectedName.isEmpty() ? m_connectedAddress : m_connectedName;
        emit statusMessage(QString("✅ BLE 已就绪: %1").arg(devName));

        // 连通性自检：发送一帧合法的 STOP 命令 (AA 00 00 00 00 00 AA)。
        // F4 端应解析为有效命令帧 (ok+1)，可在 STM32 调试串口确认链路打通。
        // 注意：不要在此发 "AT\r\n" —— 连接态下模块透传，AT 会被当作数据注入 STM32。
        QTimer::singleShot(500, this, [this]() {
            if (!m_connected || !m_serviceFound || !m_service || !m_rxChar.isValid()) return;
            QByteArray testFrame;
            testFrame.resize(7);
            testFrame[0] = 0xAA;
            testFrame[1] = 0x00; testFrame[2] = 0x00;
            testFrame[3] = 0x00; testFrame[4] = 0x00;
            testFrame[5] = 0x00;
            testFrame[6] = 0xAA;
            // 自检改用“带响应写”：成功会触发 characteristicWritten(✅)，
            // 失败会触发服务错误(⚠️)，据此判断模块是否真的收下了 GATT 写。
            QLowEnergyService::WriteMode wm =
                (m_rxChar.properties() & QLowEnergyCharacteristic::Write)
                    ? QLowEnergyService::WriteWithResponse
                    : QLowEnergyService::WriteWithoutResponse;
            emit statusMessage(QString("🧪 发送连通性自检帧 (STOP, %1): AA 00 00 00 00 00 AA")
                               .arg(wm == QLowEnergyService::WriteWithResponse ? "带响应" : "无响应"));
            m_service->writeCharacteristic(m_rxChar, testFrame, wm);
        });

    } else if (newState == QLowEnergyService::DiscoveringServices) {
        emit statusMessage("⏳ 正在获取服务详情...");
    }
}

// ═══════════════════════════════════════════════════════════════
//  数据收发
// ═══════════════════════════════════════════════════════════════

bool BleManager::sendData(const QByteArray &data)
{
    if (!m_connected || !m_serviceFound || !m_service) {
        emit statusMessage("⚠️ BLE 未就绪，无法发送");
        return false;
    }
    if (!m_rxChar.isValid()) {
        emit statusMessage("⚠️ RX 特征无效，无法发送");
        return false;
    }

    QString hexStr;
    for (int i = 0; i < data.size(); ++i)
        hexStr += QString::number(static_cast<uint8_t>(data[i]), 16).rightJustified(2, '0') + " ";
    QString uuidStr = m_rxChar.uuid().toString();
    emit statusMessage(QString("📤 写入 %1 (特征: %2): %3")
                       .arg(data.size()).arg(uuidStr).arg(hexStr.trimmed()));

    QLowEnergyService::WriteMode mode;
    if (m_rxChar.properties() & QLowEnergyCharacteristic::WriteNoResponse) {
        mode = QLowEnergyService::WriteWithoutResponse;
        emit statusMessage("  ↳ 模式: WriteWithoutResponse (推荐)");
    } else if (m_rxChar.properties() & QLowEnergyCharacteristic::Write) {
        mode = QLowEnergyService::WriteWithResponse;
        emit statusMessage("  ↳ 模式: WriteWithResponse");
    } else {
        mode = QLowEnergyService::WriteWithoutResponse;
        emit statusMessage("  ⚠️ 特征无写入属性，强制 WriteWithoutResponse");
    }

    m_service->writeCharacteristic(m_rxChar, data, mode);
    return true;
}

void BleManager::onCharacteristicChanged(
    const QLowEnergyCharacteristic &characteristic,
    const QByteArray &newValue)
{
    QString hexStr;
    for (int i = 0; i < newValue.size(); ++i)
        hexStr += QString::number(static_cast<uint8_t>(newValue[i]), 16).rightJustified(2, '0') + " ";
    emit statusMessage(QString("📩 收到通知 [%1]: %2")
                       .arg(characteristic.uuid().toString(), hexStr.trimmed()));
    emit dataReceived(newValue);
}

void BleManager::onDescriptorWritten(
    const QLowEnergyDescriptor &descriptor,
    const QByteArray &data)
{
    Q_UNUSED(descriptor);
    Q_UNUSED(data);
}

// ═══════════════════════════════════════════════════════════════
//  AT 指令发送（配置 JDY-x3）
// ═══════════════════════════════════════════════════════════════
bool BleManager::sendATCommand(const QByteArray &cmd)
{
    if (!m_connected || !m_serviceFound || !m_service) {
        emit statusMessage("⚠️ BLE 未就绪，无法发送 AT 指令");
        return false;
    }
    if (!m_rxChar.isValid()) {
        emit statusMessage("⚠️ RX 特征无效，无法发送 AT 指令");
        return false;
    }

    emit statusMessage(QString("📤 AT → %1 字节").arg(cmd.size()));
    m_service->writeCharacteristic(m_rxChar, cmd, QLowEnergyService::WriteWithoutResponse);
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  查询接口
// ═══════════════════════════════════════════════════════════════

QStringList BleManager::getDiscoveredDeviceNames() const
{
    return m_deviceNames;
}

QStringList BleManager::getDiscoveredDeviceAddresses() const
{
    return m_deviceAddresses;
}

QString BleManager::getConnectedDeviceName() const
{
    return m_connectedName;
}

QString BleManager::getConnectedDeviceAddress() const
{
    return m_connectedAddress;
}

int BleManager::discoveredCount() const
{
    return m_deviceNames.size();
}

// ═══════════════════════════════════════════════════════════════
//  内部辅助
// ═══════════════════════════════════════════════════════════════

void BleManager::clearController()
{
    m_connected = false;
    m_serviceFound = false;
    m_connectTimer->stop();

    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }
    if (m_controller) {
        if (m_controller->state() != QLowEnergyController::UnconnectedState) {
            m_controller->disconnectFromDevice();
        }
        m_controller->deleteLater();
        m_controller = nullptr;
    }
    m_txChar = QLowEnergyCharacteristic();
    m_rxChar = QLowEnergyCharacteristic();
}

void BleManager::resetDiscovery()
{
    m_deviceNames.clear();
    m_deviceAddresses.clear();
    m_deviceRssis.clear();
}
