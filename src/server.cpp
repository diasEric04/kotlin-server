#include "server.hpp"
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QIODeviceBase>
#include <QDir>

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

    // GET path http/1.1
    QList<QByteArray> parts = requestLine.split(' ');

    if (parts.size() < 3) {
        sendResponse(socket, "400 Bad Request", "text/plain", "Bad Request");
        return;
    }

    QByteArray method = parts[0];
    QByteArray path = parts[1];

    if (method != "GET") {
        sendResponse(socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }
 
    
    QMap<QString, fileInfo> paths;

    paths["/home"] = {
        "../pages/home.html",
        "text/html",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/soma"] = {
        "../pages/soma.html",
        "text/html",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/binario"] = {
        "../pages/binario.html",
        "text/html",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/styles/main.css"] = {
        "../pages/styles/main.css",
        "text/css",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/scripts/binario.js"] = {
        "../pages/scripts/binario.js",
        "text/javascript",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/scripts/soma.js"] = {
        "../pages/scripts/soma.js",
        "text/javascript",
        QIODevice::ReadOnly | QIODevice::Text
    };

    paths["/images/kill.jpg"] = {
        "../pages/images/kill.jpg",
        "image/jpeg",
        QIODevice::ReadOnly
    };

    paths["/images/ride.jpg"] = {
        "../pages/images/ride.jpg",
        "image/jpeg",
        QIODevice::ReadOnly
    };

    paths["/images/master.jpg"] = {
        "../pages/images/master.jpg",
        "image/jpeg",
        QIODevice::ReadOnly
    };

    paths["/images/justice.jpg"] = {
        "../pages/images/justice.jpg",
        "image/jpeg",
        QIODevice::ReadOnly
    };
    

    if (path == "/") {
        QString text = Server::navegateFiles("..", 1);
        sendResponse(socket, "200 OK", "text/html", text.toUtf8());
        return;
    }

    if (!paths.contains(path)) {
        sendResponse(socket, "404 Not Found", "text/html; charset=utf-8", "Not Found");
        return;
    }


    fileInfo fi = paths[path];

    QFile file {fi.arquivo};
    QByteArray text = "";
    if (file.open(fi.openFileFlags)) {
        text = file.readAll();
    }
    file.close();
    sendResponse(socket, "200 OK", fi.contentType, text);

    
}

QString Server::navegateFiles(const QString &filePath, int level) {
    QDir dir(filePath);

    QString text = "<ul>";
    QString tab = "\t";

    QStringList files = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& file : files) {
        QFileInfo info(dir.filePath(file));

        if (info.isFile()) {
            text += tab.repeated(level);
            text += "<li>";
            if (info.suffix() == "html") {
                text += "<a href=\"" + info.completeBaseName() + "\">";
                text += file;
                text += "</a>";
            } else {
                text += file;
            }
            text += "</li>";
        }
        else if (info.isDir()) {
            text += tab.repeated(level);
            text += "<li>" + file + "/";
            text += navegateFiles(dir.filePath(file),level + 1);
            text += "</li>";
        }
    }
    text += "</ul>";

    return text;
}

void Server::sendResponse(QTcpSocket *socket, const QByteArray &status, const QByteArray &contentType, const QByteArray &body)
{
    QByteArray response;

    response += "HTTP/1.1 " + status + "\r\n";
    response += "Content-Type:" + contentType +"\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    qDebug() << body;

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