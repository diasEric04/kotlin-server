#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>

struct fileInfo {
    QString arquivo;
    QByteArray contentType;
    QIODeviceBase::OpenMode openFileFlags;
};

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
    void sendResponse(QTcpSocket *socket, const QByteArray &status, const QByteArray &contentType, const QByteArray &body);
    static QString navegateFiles(const QString &filePath, int level);
};