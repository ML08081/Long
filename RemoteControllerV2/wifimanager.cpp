#include <memory>
#include <QTimer>
#include "wifimanager.h"
#include <QHostAddress>
#include <QElapsedTimer>
#include <QTimer>

WiFiManager::WiFiManager(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    m_connectedPort = 0;

    connect(m_socket, &QTcpSocket::connected, this, &WiFiManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &WiFiManager::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &WiFiManager::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &WiFiManager::onError);
}

WiFiManager::~WiFiManager()
{
    disconnectFromHost();
    m_socket->deleteLater();
}

void WiFiManager::connectToHost(const QString &ip, quint16 port)
{
    if (m_socket->isOpen()) {
        m_socket->close();
    }

    m_connectedIp = ip;
    m_connectedPort = port;
    m_socket->connectToHost(ip, port);
}

void WiFiManager::disconnectFromHost()
{
    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
    m_connectedIp.clear();
    m_connectedPort = 0;
}

bool WiFiManager::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool WiFiManager::sendData(const QByteArray &data)
{
    if (!isConnected()) return false;

    qint64 written = m_socket->write(data);
    if (written == data.size()) {
        m_socket->flush();
        return true;
    }
    return false;
}

void WiFiManager::pingHost(const QString &ip, quint16 port, int timeoutMs)
{
    QTcpSocket *pingSocket = new QTcpSocket(this);
    auto timer=std::make_shared<QElapsedTimer>(); timer->start();
    auto finished=std::make_shared<bool>(false);
    auto finish=[this,pingSocket,ip,port,timer,finished](bool success) {
        if(*finished) return;
        *finished=true;
        emit pingResult(success,ip,port,static_cast<int>(timer->elapsed()));
        pingSocket->abort(); pingSocket->deleteLater();
    };
    connect(pingSocket,&QTcpSocket::connected,this,[finish](){finish(true);});
    connect(pingSocket,&QTcpSocket::errorOccurred,this,[finish](){finish(false);});
    QTimer::singleShot(timeoutMs,pingSocket,[finish](){finish(false);});
    pingSocket->connectToHost(ip,port);

}

QString WiFiManager::getConnectedIp() const
{
    return m_connectedIp;
}

quint16 WiFiManager::getConnectedPort() const
{
    return m_connectedPort;
}

void WiFiManager::onConnected()
{
    emit connected();
}

void WiFiManager::onDisconnected()
{
    m_connectedIp.clear();
    m_connectedPort = 0;
    emit disconnected();
}

void WiFiManager::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_socket->errorString());
}

void WiFiManager::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    emit dataReceived(data);
}
