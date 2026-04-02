#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>
#include <functional>
#include <QJsonObject>

class HttpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit HttpServer(QObject *parent = nullptr);

    // 注册路由：method + path 映射到处理函数
    void route(const QString &method, const QString &path,
               std::function<QByteArray(const QJsonObject &)> handler);

    static QByteArray buildHttpResponse(int statusCode, const QString &statusText, const QByteArray &body);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QMap<QString, std::function<QByteArray(const QJsonObject &)>> m_routes;
};

#endif // HTTPSERVER_H
