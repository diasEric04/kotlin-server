#include "server.hpp"

Server::Server(QObject *parent) : QObject{parent}
{
    connect(&server, &QTcpServer::newConnection,
            this, &Server::newConnection);
}

void Server::start()
{
    if (!server.listen(QHostAddress::Any, 2020)) {
        qWarning() << server.errorString();
        return;
    }

    qInfo() << "HTTP server listening on port 2020...";
}

void Server::quit()
{
    server.close();
}

void Server::newConnection()
{
    QTcpSocket *socket = server.nextPendingConnection();

    connect(socket, &QTcpSocket::disconnected,
            this, &Server::disconnected);

    connect(socket, &QTcpSocket::readyRead,
            this, &Server::readyRead);

    qInfo() << "Connected:" << socket;
}

void Server::readyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket)
        return;

    QByteArray request = socket->readAll();

    qInfo() << "Request:";
    qInfo().noquote() << request;

    QList<QByteArray> lines = request.split('\n');

    if (lines.isEmpty())
        return;

    QByteArray requestLine = lines[0].trimmed();

    QList<QByteArray> parts = requestLine.split(' ');

    if (parts.size() < 3) {
        sendResponse(socket, "400 Bad Request", "Bad Request");
        return;
    }

    QByteArray method = parts[0];
    QByteArray path = parts[1];

    qInfo() << "Method:" << method;
    qInfo() << "Path:" << path;

    if (method != "GET") {
        sendResponse(socket, "405 Method Not Allowed", "Method Not Allowed");
        return;
    }

    if (path == "/") {
        sendResponse(socket, "200 OK", "Hello from my HTTP server!");
    }
    else if (path == "/teste") {
        sendResponse(socket, "200 OK", "Rota /teste funcionando!");
    }
    else {
        sendResponse(socket, "404 Not Found", "Not Found");
    }
}

void Server::sendResponse(QTcpSocket *socket, const QByteArray &status, const QByteArray &body)
{
    QByteArray response;

    response += "HTTP/1.1 " + status + "\r\n";
    response += "Content-Type: text/plain; charset=utf-8\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;

    socket->write(response);
    socket->disconnectFromHost();
}

void Server::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());

    if (socket) {
        qInfo() << "Disconnected:" << socket;
        socket->deleteLater();
    }
}

Server::~Server() {
    server.disconnect();
}