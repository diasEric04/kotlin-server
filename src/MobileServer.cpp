#include "MobileServer.hpp"

#include <QHostAddress>
#include <QDebug>
#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>

MobileServer::MobileServer(qint32 port, QObject *parent)
    : QObject(parent), port(port)
{
    connect(&server, &QTcpServer::newConnection, this, &MobileServer::newConnection);
}

void MobileServer::start()
{
    if (!server.listen(QHostAddress::Any, port)) {
        qWarning() << "Erro ao iniciar MobileServer:"
                   << server.errorString();
        return;
    }

    qInfo() << "MobileServer ouvindo na porta" << port;
}

void MobileServer::quit()
{
    server.close();

    for (QTcpSocket *socket : buffers.keys()) {
        if (socket) socket->abort();
    }

    kotlinSockets.clear();
    pendingApiSockets.clear();
    connectedDevices.clear();
    watchers.clear();
    buffers.clear();
}

void MobileServer::newConnection()
{
    while (server.hasPendingConnections()) {
        QTcpSocket *socket = server.nextPendingConnection();

        buffers.insert(socket, QByteArray{});

        connect(socket, &QTcpSocket::disconnected, this, &MobileServer::disconnected);

        connect(socket, &QTcpSocket::readyRead, this, &MobileServer::readyRead);

        qInfo() << "Conexão recebida:" << socket;
    }
}

void MobileServer::readyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket) return;

    buffers[socket].append(socket->readAll());

    QByteArray &buffer = buffers[socket];

    while (true) {
        qsizetype fimComando = buffer.indexOf('\n');

        if (fimComando < 0) return;

        QByteArray comando = buffer.left(fimComando).trimmed();

        if (comando != "KOTLIN_APP CONNECT" &&
            comando != "API_SERVER CONNECT" &&
            comando != "API_SERVER GET" &&
            comando != "KOTLIN_APP POST") 
        {

            qWarning() << "Comando desconhecido:" << comando;

            buffer.remove(0, fimComando + 1);
            continue;
        }

        qsizetype fimConteudo = buffer.indexOf('\n', fimComando + 1);

        if (fimConteudo < 0) return;

        QByteArray conteudo = buffer.mid(
            fimComando + 1,
            fimConteudo - fimComando - 1
        ).trimmed();

        buffer.remove(0, fimConteudo + 1);

        QString deviceId = QString::fromUtf8(conteudo);

        if (comando == "KOTLIN_APP CONNECT") {
            if (deviceId.isEmpty()) {
                qWarning() << "Device ID vazio";
                continue;
            }
            QTcpSocket *socketAnterior = kotlinSockets.value(deviceId, nullptr);

            if (socketAnterior && socketAnterior != socket) notifyDeviceDisconnected(deviceId);

            kotlinSockets.insert(deviceId, socket);

            qInfo() << "Dispositivo registrado:" << deviceId << "Socket:" << socket;

            continue;
        }
        if (comando == "API_SERVER GET") {

            if (!connectedDevices.contains(deviceId)) {
                qWarning() << "API negada: dispositivo não autorizado:" << deviceId;

                sendResponse(socket, "(JSON){\"connected\": false, \"description\": \"Execute /connect_device primeiro\"}\n");

                continue;
            }

            auto it = kotlinSockets.constFind(deviceId);

            if (it == kotlinSockets.cend() ||
                !it.value() ||
                it.value()->state() != QAbstractSocket::ConnectedState) 
            {

                connectedDevices.remove(deviceId);

                qWarning() << "Dispositivo não conectado:" << deviceId;

                sendResponse(socket, "(JSON){\"connected\": false, \"description\": \"Device não conectado\"}\n");

                continue;
            }

            // Não permite sobrescrever uma requisição pendente.
            if (pendingApiSockets.contains(deviceId)) {
                qWarning() << "Já existe um GET pendente para:" << deviceId;

                sendResponse(socket, "(JSON){\"connected\": false, \"description\": \"Requisicao ja pendente para este dispositivo\"}\n");

                continue;
            }

            QTcpSocket *kotlinApp = it.value();

            pendingApiSockets.insert(deviceId, socket);

            qInfo() << "GET autorizado para:" << deviceId;

            sendResponse(kotlinApp,"API_SERVER GET\n");

            continue;
        }

        if (comando == "KOTLIN_APP POST") {

            QString deviceIdKotlin;

            for (auto it = kotlinSockets.cbegin();
                 it != kotlinSockets.cend(); ++it) {

                if (it.value() == socket) {
                    deviceIdKotlin = it.key();
                    break;
                }
            }

            if (deviceIdKotlin.isEmpty()) {
                qWarning() << "Socket Kotlin sem dispositivo registrado";
                continue;
            }

            auto apiIt = pendingApiSockets.find(deviceIdKotlin);

            if (apiIt == pendingApiSockets.end()) {
                qWarning() << "Nenhuma requisição pendente para:" << deviceIdKotlin;
                continue;
            }

            QTcpSocket *apiSocket = apiIt.value();

            pendingApiSockets.erase(apiIt);

            if (!apiSocket || apiSocket->state() != QAbstractSocket::ConnectedState) {

                qWarning() << "Socket da API desconectado";
                continue;
            }

            qInfo() << "Resposta Kotlin recebida para:" << deviceIdKotlin;

            qInfo().noquote() << conteudo;

            sendResponse(apiSocket,"(JSON)" + conteudo + "\n");

            continue;
        }

        if (comando == "API_SERVER CONNECT") {

            auto it = kotlinSockets.constFind(deviceId);

            if (it == kotlinSockets.cend() ||
                !it.value() ||
                it.value()->state() != QAbstractSocket::ConnectedState)
            {
                connectedDevices.remove(deviceId);

                qWarning() << "Dispositivo não conectado:"<< deviceId;

                sendResponse(socket,
                    "(JSON){\"connected\": false, \"description\": \"Device nao conectado\"}\n"
                );

                continue;
            }

            QTcpSocket *kotlinApp = it.value();

            connectedDevices.insert(deviceId);

            QList<QTcpSocket*> &lista = watchers[deviceId];

            if (!lista.contains(socket))
                lista.append(socket);

            qInfo() << "Dispositivo autorizado:"<< deviceId << "Observadores:" << lista.size();

            sendResponse(
                socket,
                "(JSON){\"connected\": true, \"description\": \"Device encontrado\"}\n"
            );

            sendResponse(
                kotlinApp,
                "API_SERVER CONNECT\n" +
                    deviceId.toUtf8() + "\n"
            );

            continue;
        }
    }
}

void MobileServer::sendResponse(
    QTcpSocket *socket,
    const QByteArray &content)
{
    if (!socket ||
        socket->state() != QAbstractSocket::ConnectedState) {

        qWarning() << "Tentativa de envio para socket desconectado";
        return;
    }

    qInfo().noquote() << "Enviando:" << content.trimmed();

    socket->write(content);
    socket->flush();
}

void MobileServer::notifyDeviceDisconnected(const QString &deviceId)
{
    connectedDevices.remove(deviceId);

    const QJsonObject obj {
        {"event", "device_disconnected"},
        {"connected", false},
        {"device", deviceId},
        {"description", "Device desconectado"}
    };

    const QByteArray evento = "(JSON)" + QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";

    auto pendingIt = pendingApiSockets.find(deviceId);

    if (pendingIt != pendingApiSockets.end()) {
        QTcpSocket *apiSocket = pendingIt.value();

        pendingApiSockets.erase(pendingIt);

        sendResponse(apiSocket, evento);
    }

    const QList<QTcpSocket*> lista = watchers.take(deviceId);

    for (QTcpSocket *watcher : lista)
        sendResponse(watcher, evento);

    qInfo() << "Autorização removida:" << deviceId
            << "Observadores avisados:" << lista.size();
}

void MobileServer::disconnected()
{
    QTcpSocket *socket =
        qobject_cast<QTcpSocket*>(sender());

    if (!socket) return;

    QStringList devicesDoSocket;

    for (auto it = kotlinSockets.cbegin(); it != kotlinSockets.cend(); ++it) {
        if (it.value() == socket) devicesDoSocket.append(it.key());
    }

    for (const QString &deviceId : devicesDoSocket) {
        kotlinSockets.remove(deviceId);
        notifyDeviceDisconnected(deviceId);
    }

    for (auto it = pendingApiSockets.begin(); it != pendingApiSockets.end();) {

        if (it.value() == socket) it = pendingApiSockets.erase(it);
        else ++it;
    }

    for (auto it = watchers.begin(); it != watchers.end();) {

        if (it.value().removeAll(socket) > 0 && it.value().isEmpty()) {
            qInfo() << "Sem observadores, autorização removida:" << it.key();

            connectedDevices.remove(it.key());
            it = watchers.erase(it);
        } else ++it;

    }

    buffers.remove(socket);

    qInfo() << "Desconectado:" << socket;

    socket->deleteLater();
}

MobileServer::~MobileServer()
{
    server.close();

    for (QTcpSocket *socket : buffers.keys()) {
        if (socket) {
            socket->disconnect(this);
            socket->abort();
            socket->deleteLater();
        }
    }

    kotlinSockets.clear();
    pendingApiSockets.clear();
    connectedDevices.clear();
    watchers.clear();
    buffers.clear();
}