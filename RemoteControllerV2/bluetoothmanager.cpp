#include "bluetoothmanager.h"
#include <QThread>

BluetoothManager::BluetoothManager(QObject *parent) : QObject(parent)
{
    m_serialPort = new QSerialPort(this);
    m_connectedBaudRate = 0;

    connect(m_serialPort, &QSerialPort::readyRead, this, &BluetoothManager::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::ResourceError) {
            disconnectDevice();
        } else if (error != QSerialPort::NoError) {
            emit errorOccurred(m_serialPort->errorString());
        }
    });
}

BluetoothManager::~BluetoothManager()
{
    disconnectDevice();
    m_serialPort->deleteLater();
}

bool BluetoothManager::connectDevice(const QString& portName, qint32 baudRate)
{
    if (m_serialPort->isOpen()) {
        disconnectDevice();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    const int maxRetries = 3;
    for (int attempt = 0; attempt <= maxRetries; attempt++)
    {
        if (m_serialPort->open(QIODevice::ReadWrite)) {
            m_connectedPort = portName;
            m_connectedBaudRate = baudRate;
            emit connected();
            return true;
        }

        QString errorDetail = QString("打开串口失败 (尝试 %1/%2): %3 [错误码 %4]")
            .arg(attempt + 1).arg(maxRetries + 1)
            .arg(m_serialPort->errorString())
            .arg(m_serialPort->error());

        if (attempt < maxRetries) {
            emit errorOccurred(errorDetail + "，正在重试...");
            QThread::msleep(1500);
        } else {
            emit errorOccurred(errorDetail);
        }
    }

    return false;
}

void BluetoothManager::disconnectDevice()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        m_connectedPort.clear();
        m_connectedBaudRate = 0;
        emit disconnected();
    }
}

bool BluetoothManager::isConnected() const
{
    return m_serialPort->isOpen();
}

bool BluetoothManager::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        return false;
    }

    qint64 bytesWritten = m_serialPort->write(data);
    m_serialPort->flush();
    return bytesWritten == data.size();
}

QStringList BluetoothManager::listAvailableDevices()
{
    QStringList devices;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString deviceName = info.portName();
        QString description = info.description();
        if (!description.isEmpty()) {
            deviceName += " (" + description + ")";
        }
        devices << deviceName;
    }
    return devices;
}

QString BluetoothManager::getConnectedPort() const
{
    return m_connectedPort;
}

qint32 BluetoothManager::getConnectedBaudRate() const
{
    return m_connectedBaudRate;
}

void BluetoothManager::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    emit dataReceived(data);
}
