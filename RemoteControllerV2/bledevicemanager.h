#ifndef BLEDEVICEMANAGER_H
#define BLEDEVICEMANAGER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QTimer>
#include <QBluetoothAddress>
#include <QBluetoothUuid>

class BleManager : public QObject
{
    Q_OBJECT
public:
    explicit BleManager(QObject *parent = nullptr);
    ~BleManager();

    void startScan(int timeoutMs = 15000);
    void stopScan();
    bool isScanning() const;

    void connectToDevice(const QString &address);
    void disconnectFromDevice();
    bool isConnected() const;

    bool sendData(const QByteArray &data);
    bool sendATCommand(const QByteArray &cmd);

    QStringList getDiscoveredDeviceNames() const;
    QStringList getDiscoveredDeviceAddresses() const;
    QString getConnectedDeviceName() const;
    QString getConnectedDeviceAddress() const;
    int discoveredCount() const;

    static QBluetoothUuid nusServiceUuid();
    static QBluetoothUuid nusTxCharUuid();
    static QBluetoothUuid nusRxCharUuid();

signals:
    void scanStarted();
    void scanFinished();
    void scanError(const QString &error);
    void deviceDiscovered(const QString &name, const QString &address, int rssi);
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void dataReceived(const QByteArray &data);
    void statusMessage(const QString &msg);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onScanTimeout();
    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(QLowEnergyController::Error error);
    void onServiceDiscovered(const QBluetoothUuid &uuid);
    void onServiceScanDone();
    void onServiceStateChanged(QLowEnergyService::ServiceState newState);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                  const QByteArray &newValue);
    void onDescriptorWritten(const QLowEnergyDescriptor &descriptor,
                             const QByteArray &data);
    void tryService(int index);

private:
    void clearController();
    void resetDiscovery();

    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent = nullptr;
    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_service = nullptr;
    QLowEnergyCharacteristic m_txChar;
    QLowEnergyCharacteristic m_rxChar;

    QTimer *m_scanTimer;
    QTimer *m_keepaliveTimer;
    QTimer *m_reconnectTimer;
    QTimer *m_connectTimer;

    bool m_scanning = false;
    bool m_connected = false;
    bool m_serviceFound = false;
    int m_serviceIndex = 0;

    QStringList m_deviceNames;
    QStringList m_deviceAddresses;
    QList<int> m_deviceRssis;

    QString m_connectedAddress;
    QString m_connectedName;
    QString m_pendingAddress;
    int m_reconnectAttempts = 0;

    QList<QBluetoothUuid> m_discoveredServices;
};

#endif // BLEDEVICEMANAGER_H
