#include "ApiServer.hpp"

#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <memory>

using std::function;

namespace {

const char *MOBILE_HOST = "192.168.1.6";
constexpr quint16 MOBILE_PORT = 5000;
constexpr int MOBILE_TIMEOUT_MS = 10000;

struct MobileMessage {
    QByteArray contentType;
    QByteArray body;
};

// O MobileServer responde com uma linha terminada em '\n',
// opcionalmente prefixada com "(JSON)".
MobileMessage parseMobileLine(QByteArray line)
{
    MobileMessage msg;
    msg.contentType = "text/plain; charset=utf-8";

    if (line.startsWith("(JSON)")) {
        msg.contentType = "application/json";
        line.remove(0, 6);
    }

    msg.body = line;
    return msg;
}

// JSON de verdade quando possível; senão, string.
QJsonValue bodyToJson(const QByteArray &body, bool tryJson)
{
    if (tryJson) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &err);

        if (err.error == QJsonParseError::NoError)
            return doc.isObject() ? QJsonValue(doc.object())
                                  : QJsonValue(doc.array());
    }

    return QString::fromUtf8(body);
}

QJsonObject bodyToObject(const QByteArray &body)
{
    return QJsonDocument::fromJson(body).object();
}

void sendJson(QWebSocket *client, const QJsonObject &obj)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "WebSocket desconectado";
        return;
    }

    const QString text = QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));

    qInfo() << "WebSocket ->" << text;

    client->sendTextMessage(text);
}

} // namespace

ApiServer::ApiServer(qint32 port, QObject *parent)
    : QObject{parent},
      port{port},
      server{QStringLiteral("ApiServer"), QWebSocketServer::NonSecureMode, this}
{
    connect(&server, &QWebSocketServer::newConnection,
            this, &ApiServer::newConnection);
}

ApiServer::~ApiServer()
{
    quit();
}

void ApiServer::start()
{
    if (!server.listen(QHostAddress::Any, port)) {
        qWarning() << server.errorString();
        return;
    }

    qInfo() << "WebSocket server listening on port" << port << "...";
}

void ApiServer::quit()
{
    server.close();

    const auto watchers = watches.keys();
    for (QWebSocket *c : watchers)
        closeWatches(c);

    const auto copy = clients;
    for (QWebSocket *c : copy)
        c->close();
}

void ApiServer::newConnection()
{
    while (server.hasPendingConnections()) {
        QWebSocket *socket = server.nextPendingConnection();

        connect(socket, &QWebSocket::textMessageReceived,
                this, &ApiServer::textMessageReceived);
        connect(socket, &QWebSocket::disconnected,
                this, &ApiServer::disconnected);

        clients.append(socket);

        qInfo() << "Connected:" << socket->peerAddress().toString()
                << socket->peerPort();
    }
}

void ApiServer::disconnected()
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());

    if (!socket)
        return;

    qInfo() << "Disconnected:" << socket->peerAddress().toString();

    // Cliente saiu: encerra as conexões persistentes dele com o MobileServer.
    closeWatches(socket);

    clients.removeAll(socket);
    socket->deleteLater();
}

void ApiServer::closeWatches(QWebSocket *client)
{
    auto it = watches.find(client);

    if (it == watches.end())
        return;

    const auto sockets = it.value();
    watches.erase(it);

    for (const QPointer<QTcpSocket> &s : sockets) {
        if (!s)
            continue;

        s->disconnect(this);
        s->disconnectFromHost();
        s->deleteLater();
    }
}

// Protocolo: o cliente envia como texto o caminho da rota, ex.:
//   "/home"
//   "/api/<device>"
//   "/connect_device/<device>"
// (um prefixo opcional "GET " também é aceito)
void ApiServer::textMessageReceived(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket*>(sender());

    if (!client)
        return;

    qInfo() << "Request:" << message;

    QString path = message.trimmed();

    if (path.startsWith("GET ", Qt::CaseInsensitive))
        path = path.mid(4).trimmed();

    if (!path.startsWith('/')) {
        sendResponse(client, 400, "text/plain", "Bad Request");
        return;
    }

    const QString route  = path.section('/', 0, 1);   // "/api"
    const QString device = path.section('/', 2);      // "<device>"

    using Func = function<void()>;
    QMap<QString, Func> routes;

    QPointer<QWebSocket> ws = client;

    routes["/home"] = [this, ws]() {
        if (!ws) return;
        sendResponse(ws, 200, "text/html", "oi");
    };

    routes["/api"] = [this, ws, device]() {
        if (!ws) return;
        if (device.isEmpty() || device.contains('/')) {
            sendResponse(ws, 400, "text/plain", "Device inválido");
            return;
        }
        callMobileServer(ws, device.toUtf8());
    };

    routes["/connect_device"] = [this, ws, device]() {
        if (!ws) return;
        if (device.isEmpty() || device.contains('/')) {
            sendResponse(ws, 400, "text/plain", "Device inválido");
            return;
        }
        watchDevice(ws, device);
    };

    if (!routes.contains(route)) {
        sendResponse(client, 404, "text/plain", "Not Found");
        return;
    }

    routes[route]();
}

// Resposta a uma requisição do cliente:
//   { "type": "response", "status": 200, "contentType": "...", "body": <string ou JSON> }
void ApiServer::sendResponse(QWebSocket *client,
                             int status,
                             const QByteArray &contentType,
                             const QByteArray &body)
{
    QJsonObject obj;
    obj["type"] = "response";
    obj["status"] = status;
    obj["contentType"] = QString::fromUtf8(contentType);
    obj["body"] = bodyToJson(body, contentType.startsWith("application/json"));

    sendJson(client, obj);
}

// Evento espontâneo enviado pelo MobileServer (ex.: celular desconectou):
//   { "type": "event", "device": "<id>", "body": { "event": "device_disconnected", ... } }
void ApiServer::sendEvent(QWebSocket *client,
                          const QString &device,
                          const QByteArray &body)
{
    QJsonObject obj;
    obj["type"] = "event";
    obj["device"] = device;
    obj["body"] = bodyToJson(body, true);

    sendJson(client, obj);
}

// ---------------------------------------------------------------------------
// /api/<device>  ->  uma requisição ao MobileServer, uma resposta, fecha.
// ---------------------------------------------------------------------------
void ApiServer::callMobileServer(QWebSocket *client, const QByteArray &device)
{
    auto *socket = new QTcpSocket(this);

    QPointer<QWebSocket> wsClient = client;
    QPointer<QTcpSocket> mobile = socket;

    auto done = std::make_shared<bool>(false);
    auto buffer = std::make_shared<QByteArray>();

    auto finish = [this, wsClient, mobile, done](
                      int status, const QByteArray &type, const QByteArray &body) {
        if (*done)
            return;
        *done = true;

        if (wsClient)
            sendResponse(wsClient, status, type, body);

        if (mobile) {
            mobile->disconnect(this);
            mobile->disconnectFromHost();
            mobile->deleteLater();
        }
    };

    connect(socket, &QTcpSocket::connected, this, [mobile, device]() {
        if (!mobile)
            return;

        mobile->write("API_SERVER GET\n" + device + "\n");
        mobile->flush();
    });

    connect(socket, &QTcpSocket::readyRead, this, [mobile, buffer, finish]() {
        if (!mobile)
            return;

        buffer->append(mobile->readAll());

        qsizetype idx;
        while ((idx = buffer->indexOf('\n')) >= 0) {
            const QByteArray line = buffer->left(idx).trimmed();
            buffer->remove(0, idx + 1);

            if (line.isEmpty())
                continue;

            qInfo() << "ApiServer recebeu do MobileServer:" << line;

            const MobileMessage msg = parseMobileLine(line);
            finish(200, msg.contentType, msg.body);
            return;
        }
    });

    connect(socket, &QTcpSocket::errorOccurred, this,
            [finish](QAbstractSocket::SocketError) {
        finish(502, "text/plain; charset=utf-8",
               "Erro ao comunicar com o servidor Mobile");
    });

    connect(socket, &QTcpSocket::disconnected, this, [finish]() {
        finish(502, "text/plain; charset=utf-8",
               "Servidor Mobile encerrou a conexão");
    });

    QTimer::singleShot(MOBILE_TIMEOUT_MS, socket, [finish]() {
        finish(504, "text/plain; charset=utf-8",
               "Timeout ao aguardar o servidor Mobile");
    });

    socket->connectToHost(MOBILE_HOST, MOBILE_PORT);
}

// ---------------------------------------------------------------------------
// /connect_device/<device>  ->  conexão persistente.
//
// 1ª linha vinda do MobileServer = resposta ao CONNECT (vai como "response").
// Linhas seguintes = eventos (vão como "event"), por exemplo
// {"event":"device_disconnected", ...}. Ao receber esse evento, ou se a
// conexão com o MobileServer cair, a sessão é encerrada.
// ---------------------------------------------------------------------------
void ApiServer::watchDevice(QWebSocket *client, const QString &device)
{
    auto &mine = watches[client];

    // Se esse cliente já observa esse device, descarta a conexão antiga.
    if (QTcpSocket *old = mine.take(device)) {
        old->disconnect(this);
        old->disconnectFromHost();
        old->deleteLater();
    }

    auto *socket = new QTcpSocket(this);
    mine.insert(device, socket);

    QPointer<QWebSocket> ws = client;
    QPointer<QTcpSocket> mobile = socket;

    auto buffer   = std::make_shared<QByteArray>();
    auto answered = std::make_shared<bool>(false);  // CONNECT já foi respondido?
    auto closed   = std::make_shared<bool>(false);  // sessão encerrada?

    auto teardown = [this, ws, mobile, device, closed]() {
        *closed = true;

        if (mobile) {
            mobile->disconnect(this);
            mobile->disconnectFromHost();
            mobile->deleteLater();
        }

        if (ws) {
            auto it = watches.find(ws.data());
            if (it != watches.end() && it->value(device) == mobile)
                it->remove(device);
        }
    };

    auto lost = [this, ws, device, answered, closed, teardown]() {
        if (*closed)
            return;

        if (!*answered) {
            sendResponse(ws, 502, "text/plain; charset=utf-8",
                         "Erro ao comunicar com o servidor Mobile");
        } else {
            sendEvent(ws, device,
                      R"({"event":"connection_lost","connected":false,)"
                      R"("description":"Conexao com o servidor Mobile perdida"})");
        }

        teardown();
    };

    connect(socket, &QTcpSocket::connected, this, [mobile, device]() {
        if (!mobile)
            return;

        mobile->write("API_SERVER CONNECT\n" + device.toUtf8() + "\n");
        mobile->flush();
    });

    connect(socket, &QTcpSocket::readyRead, this,
            [this, ws, mobile, device, buffer, answered, closed, teardown]() {
        if (!mobile || *closed)
            return;

        buffer->append(mobile->readAll());

        qsizetype idx;
        while (!*closed && (idx = buffer->indexOf('\n')) >= 0) {
            const QByteArray line = buffer->left(idx).trimmed();
            buffer->remove(0, idx + 1);

            if (line.isEmpty())
                continue;

            qInfo() << "ApiServer (watch) recebeu do MobileServer:" << line;

            const MobileMessage msg = parseMobileLine(line);

            if (!*answered) {
                // Resposta ao CONNECT.
                *answered = true;
                sendResponse(ws, 200, msg.contentType, msg.body);

                // Device não encontrado: não há o que observar.
                if (!bodyToObject(msg.body).value("connected").toBool())
                    teardown();

                continue;
            }

            // Evento.
            sendEvent(ws, device, msg.body);

            if (bodyToObject(msg.body).value("event").toString()
                == "device_disconnected")
                teardown();
        }
    });

    connect(socket, &QTcpSocket::errorOccurred, this,
            [lost](QAbstractSocket::SocketError) { lost(); });

    connect(socket, &QTcpSocket::disconnected, this, [lost]() { lost(); });

    // Timeout só vale até o CONNECT ser respondido.
    QTimer::singleShot(MOBILE_TIMEOUT_MS, socket,
                       [this, ws, answered, closed, teardown]() {
        if (*closed || *answered)
            return;

        sendResponse(ws, 504, "text/plain; charset=utf-8",
                     "Timeout ao aguardar o servidor Mobile");
        teardown();
    });

    socket->connectToHost(MOBILE_HOST, MOBILE_PORT);
}