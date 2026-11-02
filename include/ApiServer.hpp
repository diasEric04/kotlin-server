#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

class ApiServer : public QObject {
    Q_OBJECT
public:
    explicit ApiServer(qint32 port, QObject* parent = nullptr);
    ~ApiServer();

public slots:
    void start();
    void quit();
    void newConnection();
    void disconnected();
    void textMessageReceived(const QString &message);

private:
    qint32 port;              
    QWebSocketServer server;
    QList<QWebSocket*> clients;

    QMap<QWebSocket*, QMap<QString, QPointer<QTcpSocket>>> watches;

    void sendResponse(QWebSocket *client,int status,const QByteArray &contentType,const QByteArray &body);
    void sendEvent(QWebSocket *client, const QString &device, const QByteArray &body);
    void callMobileServer(QWebSocket *client, const QByteArray &device);
    void watchDevice(QWebSocket *client, const QString &device);
    void closeWatches(QWebSocket *client);
};