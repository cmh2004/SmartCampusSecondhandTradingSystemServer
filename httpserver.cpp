#include "httpserver.h"
#include "httpconnection.h"
#include<QJsonParseError>

HttpServer::HttpServer(QObject *parent) : QTcpServer(parent) {}

void HttpServer::route(const QString &method, const QString &path,
                       std::function<QByteArray(const QJsonObject &)> handler)
{
    m_routes.insert(method + ":" + path, handler);
}

void HttpServer::incomingConnection(qintptr socketDescriptor)
{
    HttpConnection *conn = new HttpConnection(socketDescriptor,
                                              [this](const QString &method, const QString &path,
                                                     const QMap<QString, QString> &headers, const QByteArray &body) -> QByteArray {
                                                  QJsonObject requestObj;
                                                  if (!body.isEmpty()) {
                                                      QJsonParseError error;
                                                      QJsonDocument doc = QJsonDocument::fromJson(body, &error);
                                                      if (error.error == QJsonParseError::NoError && doc.isObject()) {
                                                          requestObj = doc.object();
                                                      }
                                                  }
                                                  // 从 header 中提取 Authorization 并存入 requestObj
                                                  if (headers.contains("Authorization")) {
                                                      QString auth = headers.value("Authorization");
                                                      if (auth.startsWith("Bearer ", Qt::CaseInsensitive)) {
                                                          QString token = auth.mid(7);
                                                          requestObj["token"] = token;  // 放入请求对象中，供 ApiHandler 使用
                                                      }
                                                  }

                                                  QString key = method + ":" + path;
                                                  if (m_routes.contains(key)) {
                                                      QByteArray responseBody = m_routes.value(key)(requestObj);
                                                      return buildHttpResponse(200, "OK", responseBody);
                                                  } else {
                                                      QJsonObject errorObj;
                                                      errorObj["error"] = "Not Found";
                                                      errorObj["success"] = false;
                                                      QByteArray body = QJsonDocument(errorObj).toJson();
                                                      return buildHttpResponse(404, "Not Found", body);
                                                  }
                                              }, this);
}

QByteArray HttpServer::buildHttpResponse(int statusCode, const QString &statusText, const QByteArray &body)
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText.toUtf8() + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    return response;
}
