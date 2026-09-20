#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <QTcpSocket>
#include <functional>

class HttpConnection : public QObject
{
    Q_OBJECT
public:
    explicit HttpConnection(qintptr socketDescriptor,
                            std::function<QByteArray(const QString &method, const QString &path,
                                                     const QMap<QString, QString> &headers, const QByteArray &body)> handler,
                            QObject *parent = nullptr);
    ~HttpConnection();

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    std::function<QByteArray(const QString &method, const QString &path,
                             const QMap<QString, QString> &headers, const QByteArray &body)> m_handler;
    QByteArray m_buffer;
};

#endif // HTTPCONNECTION_H
