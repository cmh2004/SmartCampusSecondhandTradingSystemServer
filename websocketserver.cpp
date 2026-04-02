#include "websocketserver.h"
#include <QJsonDocument>
#include <QJsonParseError>

WebSocketServer::WebSocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QWebSocketServer("CampusSecondhandServer", QWebSocketServer::NonSecureMode, this))
{
    if (m_server->listen(QHostAddress::Any, port)) {
        connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
        qInfo() << "WebSocket server listening on port" << port;
    } else {
        qCritical() << "WebSocket server failed to start on port" << port;
    }

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &WebSocketServer::sendHeartbeat);
    m_heartbeatTimer.start(30000); // 30秒心跳
}

void WebSocketServer::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::onSocketDisconnected);
    // 此时还不知道用户ID，等待第一条消息认证
}

void WebSocketServer::onTextMessageReceived(const QString &message)
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Invalid WebSocket message format";
        return;
    }

    QJsonObject obj = doc.object();
    int type = obj.value("type").toInt();

    // 如果是认证消息（type=0），则进行用户绑定
    if (type == 0) {
        int userId = obj.value("userId").toInt();
        QString token = obj.value("token").toString();
        // 这里可以验证token，简化处理
        if (!token.isEmpty()) {
            registerUser(userId, socket);
            qInfo() << "User" << userId << "connected via WebSocket";
            return;
        }
    }

    // 其他消息转发给业务处理器
    int userId = m_socketUsers.value(socket, -1);
    if (userId != -1) {
        emit messageReceived(userId, obj);
    }
}

void WebSocketServer::onSocketDisconnected()
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;
    int userId = m_socketUsers.take(socket);
    m_userSockets.remove(userId);
    socket->deleteLater();
    if (userId != -1) {
        emit userDisconnected(userId);
        qInfo() << "User" << userId << "disconnected";
    }
}

void WebSocketServer::registerUser(int userId, QWebSocket *socket)
{
    // 如果该用户已有连接，先断开旧的
    if (m_userSockets.contains(userId)) {
        QWebSocket *oldSocket = m_userSockets[userId];
        m_socketUsers.remove(oldSocket);
        oldSocket->close();
        oldSocket->deleteLater();
    }
    m_userSockets[userId] = socket;
    m_socketUsers[socket] = userId;
    emit userConnected(userId);
}

void WebSocketServer::unregisterUser(int userId)
{
    if (m_userSockets.contains(userId)) {
        QWebSocket *socket = m_userSockets.take(userId);
        m_socketUsers.remove(socket);
        socket->close();
        socket->deleteLater();
    }
}

void WebSocketServer::sendToUser(int userId, const QJsonObject &message)
{
    if (!m_userSockets.contains(userId)) return;
    QWebSocket *socket = m_userSockets[userId];
    QJsonDocument doc(message);
    socket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocketServer::sendToUsers(const QList<int> &userIds, const QJsonObject &message)
{
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    for (int userId : userIds) {
        if (m_userSockets.contains(userId)) {
            m_userSockets[userId]->sendTextMessage(data);
        }
    }
}

void WebSocketServer::broadcast(const QJsonObject &message, const QStringList &excludeUsers)
{
    Q_UNUSED(excludeUsers);
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    for (auto socket : m_userSockets) {
        socket->sendTextMessage(data);
    }
}

void WebSocketServer::sendHeartbeat()
{
    QJsonObject heartbeat;
    heartbeat["type"] = 99; // 心跳类型
    heartbeat["timestamp"] = QDateTime::currentSecsSinceEpoch();
    QJsonDocument doc(heartbeat);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    for (auto socket : m_userSockets) {
        socket->sendTextMessage(data);
    }
}
