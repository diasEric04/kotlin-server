#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject* parent = nullptr);
    ~Server();

public slots:
    void start ();
    void quit ();
    void newConnection ();
    void disconnected ();
    void readyRead ();

private:
    QTcpServer server;
    void sendResponse(QTcpSocket *socket, const QByteArray &status, const QByteArray &body);
};