#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

class MobileServer : public QObject
{
    Q_OBJECT

public:
    explicit MobileServer(qint32 port, QObject *parent = nullptr);
    ~MobileServer();

    void start();
    void quit();

private slots:
    void newConnection();
    void readyRead();
    void disconnected();

private:
    void sendResponse(QTcpSocket *socket, const QByteArray &content);
    void notifyDeviceDisconnected(const QString &deviceId);

    qint32 port;
    QTcpServer server;

    QMap<QString, QTcpSocket*> kotlinSockets;
    QMap<QString, QTcpSocket*> pendingApiSockets;
    QSet<QString> connectedDevices;
    QMap<QTcpSocket*, QByteArray> buffers;
    QMap<QString, QList<QTcpSocket*>> watchers;
};