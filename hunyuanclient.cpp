#include "hunyuanclient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

HunyuanClient* HunyuanClient::m_instance = nullptr;

HunyuanClient::HunyuanClient(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

HunyuanClient::~HunyuanClient() {}

HunyuanClient* HunyuanClient::instance()
{
    if (!m_instance) {
        m_instance = new HunyuanClient();
    }
    return m_instance;
}

void HunyuanClient::initialize(const QString& apiKey, const QString& model)
{
    m_apiKey = apiKey;
    m_model = model;
    qInfo() << "HunyuanClient initialized with model:" << model;
}

void HunyuanClient::analyzeImageAndText(const QString& imageBase64,
                                        const QString& text,
                                        std::function<void(bool, const QString&)> callback)
{
    QJsonObject requestBody;
    requestBody["model"] = m_model;  // 确保 m_model 是 "hunyuan-turbo" 或 "hunyuan-vision"
    requestBody["stream"] = false;

    QJsonArray messages;
    QJsonObject userMessage;
    userMessage["role"] = "user";

    if (imageBase64.isEmpty()) {
        // 纯文本模式：content 直接是字符串
        userMessage["content"] = text;
    } else {
        // 多模态模式：content 是数组，包含 text 和 image_url
        QJsonArray contents;
        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = text;
        contents.append(textContent);

        QJsonObject imageContent;
        imageContent["type"] = "image_url";
        QJsonObject imageUrl;
        // 注意：base64 需要带 data:image/...;base64, 前缀
        QString mimeType = "image/jpeg"; // 可根据实际情况调整
        imageUrl["url"] = QString("data:%1;base64,%2").arg(mimeType).arg(imageBase64);
        imageContent["image_url"] = imageUrl;
        contents.append(imageContent);

        userMessage["content"] = contents;
    }

    messages.append(userMessage);
    requestBody["messages"] = messages;

    sendRequest(requestBody, [callback](bool success, const QJsonObject& response) {
        if (success) {
            QJsonArray choices = response.value("choices").toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QJsonObject message = choice.value("message").toObject();
                QString content = message.value("content").toString();
                callback(true, content);
                return;
            }
        }
        callback(false, QString());
    });
}

void HunyuanClient::analyzeText(const QString& text,
                                std::function<void(bool, const QString&)> callback)
{
    analyzeImageAndText("", text, callback);
}

void HunyuanClient::sendRequest(const QJsonObject& requestBody,
                                std::function<void(bool, const QJsonObject&)> callback)
{
    QNetworkRequest request;
    request.setUrl(QUrl(m_apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonDocument doc(requestBody);
    QByteArray data = doc.toJson();

    QNetworkReply* reply = m_networkManager->post(request, data);

    // 使用事件循环同步等待（与你的服务端风格一致）
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000); // 30秒超时

    if (reply->isFinished()) {
        loop.quit();
    } else {
        loop.exec(); // 等待直到 finished 或 timeout
    }

    QJsonObject response;
    bool success = false;

    if (timer.isActive()) {
        timer.stop();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "HTTP status code:" << statusCode;

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            qDebug() << "Response body:" << responseData;
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                response = doc.object();
                success = true;
            } else {
                response["error"] = "JSON parse error: " + parseError.errorString();
                qWarning() << "JSON parse error:" << parseError.errorString();
                qWarning() << "Raw response:" << responseData;
            }
        } else {
            response["error"] = reply->errorString();
            qWarning() << "Network error:" << reply->errorString();
        }
    } else {
        response["error"] = "Request timeout";
        qWarning() << "Request timeout";
        reply->abort();
    }

    reply->deleteLater();
    callback(success, response);
}
