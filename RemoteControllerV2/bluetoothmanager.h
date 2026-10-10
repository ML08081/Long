#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class BluetoothManager : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager();

    bool connectDevice(const QString& portName, qint32 baudRate);
    void disconnectDevice();
    bool isConnected() const;
    bool sendData(const QByteArray &data);
    QStringList listAvailableDevices();
    QString getConnectedPort() const;
    qint32 getConnectedBaudRate() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void dataReceived(const QByteArray &data);

private slots:
    void onReadyRead();

private:
    QSerialPort *m_serialPort;
    QString m_connectedPort;
    qint32 m_connectedBaudRate;
};

#endif // BLUETOOTHMANAGER_H
