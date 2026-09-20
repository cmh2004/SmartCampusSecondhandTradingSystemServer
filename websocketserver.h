#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <QWebSocketServer>
#include <QWebSocket>
#include <QMap>
#include <QJsonObject>
#include <QTimer>

class WebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketServer(quint16 port, QObject *parent = nullptr);

    void broadcast(const QJsonObject &message, const QStringList &excludeUsers = {});
    void sendToUser(int userId, const QJsonObject &message);
    void sendToUsers(const QList<int> &userIds, const QJsonObject &message);

    // 用户连接管理
    void registerUser(int userId, QWebSocket *socket);
    void unregisterUser(int userId);

signals:
    void messageReceived(int senderId, const QJsonObject &message);
    void userConnected(int userId);
    void userDisconnected(int userId);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString &message);
    void onSocketDisconnected();
    void sendHeartbeat();

private:
    QWebSocketServer *m_server;
    QMap<int, QWebSocket*> m_userSockets;   // userId -> socket
    QMap<QWebSocket*, int> m_socketUsers;   // socket -> userId
    QTimer m_heartbeatTimer;
};

#endif // WEBSOCKETSERVER_H
