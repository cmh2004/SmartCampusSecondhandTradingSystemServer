#include "httpconnection.h"
#include <QHostAddress>
#include <QDir>

HttpConnection::HttpConnection(qintptr socketDescriptor,
                               std::function<QByteArray(const QString &, const QString &,const QMap<QString, QString> &, const QByteArray &)> handler,
                               QObject *parent)
    : QObject(parent), m_handler(handler)
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(socketDescriptor)) {
        deleteLater();
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &HttpConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &HttpConnection::onDisconnected);
}

HttpConnection::~HttpConnection()
{
    m_socket->deleteLater();
}

void HttpConnection::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    int headerEnd = m_buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    QByteArray headerData = m_buffer.left(headerEnd);
    QByteArray bodyData = m_buffer.mid(headerEnd + 4);

    // 解析请求行
    QList<QByteArray> lines = headerData.split('\r\n');
    if (lines.isEmpty()) return;
    QList<QByteArray> requestLine = lines[0].split(' ');
    if (requestLine.size() < 3) return;
    QString method = QString::fromUtf8(requestLine[0]);
    QString path = QString::fromUtf8(requestLine[1]);

    // 解析请求头
    QMap<QString, QString> headers;
    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        int colonPos = line.indexOf(':');
        if (colonPos != -1) {
            QString key = QString::fromUtf8(line.left(colonPos)).trimmed();
            QString value = QString::fromUtf8(line.mid(colonPos + 1)).trimmed();
            headers[key] = value;
        }
    }

    // ========== 静态文件处理 ==========
    if (path.startsWith("/uploads/")) {
        // 安全处理：防止目录遍历攻击（../）
        QString safePath = QDir::cleanPath(path);
        // 实际文件路径：服务器当前目录 + 相对路径
        QString filePath = QDir::currentPath() + safePath;
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray fileData = file.readAll();
            QString contentType;
            if (safePath.endsWith(".png", Qt::CaseInsensitive))
                contentType = "image/png";
            else if (safePath.endsWith(".jpg", Qt::CaseInsensitive) ||
                     safePath.endsWith(".jpeg", Qt::CaseInsensitive))
                contentType = "image/jpeg";
            else
                contentType = "application/octet-stream";

            QByteArray response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: " + contentType.toUtf8() + "\r\n"
                                                           "Content-Length: " + QByteArray::number(fileData.size()) + "\r\n"
                                                                          "Connection: close\r\n"
                                                                          "\r\n";
            response += fileData;
            m_socket->write(response);
            m_socket->disconnectFromHost();
            return;
        } else {
            // 文件不存在
            QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "\r\n"
                                  "404 Not Found";
            m_socket->write(response);
            m_socket->disconnectFromHost();
            return;
        }
    }
    // ====================================

    // 继续根据 Content-Length 确认 body 完整，然后调用 handler
    int contentLength = headers.value("Content-Length").toInt();
    if (bodyData.size() < contentLength) return; // 等待更多数据

    // 调用 handler（业务路由）
    QByteArray response = m_handler(method, path, headers, bodyData);
    m_socket->write(response);
    m_socket->disconnectFromHost();
}

void HttpConnection::onDisconnected()
{
    deleteLater();
}
