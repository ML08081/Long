#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QObject>
#include <QTcpSocket>

class WiFiManager : public QObject
{
    Q_OBJECT
public:
    explicit WiFiManager(QObject *parent = nullptr);
    ~WiFiManager();

    void connectToHost(const QString &ip, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    bool sendData(const QByteArray &data);
    void pingHost(const QString &ip, quint16 port, int timeoutMs = 2000);

    QString getConnectedIp() const;
    quint16 getConnectedPort() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void dataReceived(const QByteArray &data);
    void pingResult(bool success, const QString &ip, quint16 port, int ms);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QString m_connectedIp;
    quint16 m_connectedPort;
};

#endif // WIFIMANAGER_H
