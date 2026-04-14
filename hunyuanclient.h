#ifndef HUNYUANCLIENT_H
#define HUNYUANCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <functional>

class HunyuanClient : public QObject
{
    Q_OBJECT
public:
    static HunyuanClient* instance();

    // 初始化配置
    void initialize(const QString& apiKey, const QString& model = "hunyuan-vision");

    // 多模态分析（支持图片+文本）
    void analyzeImageAndText(const QString& imageBase64,
                             const QString& text,
                             std::function<void(bool, const QString&)> callback);
    void analyzeImagesAndText(const QJsonArray& imagesBase64,
                              const QString& text,
                              std::function<void(bool, const QString&)> callback);

    // 纯文本分析
    void analyzeText(const QString& text,
                     std::function<void(bool, const QString&)> callback);

private:
    explicit HunyuanClient(QObject* parent = nullptr);
    ~HunyuanClient();

    void sendRequest(const QJsonObject& requestBody,
                     std::function<void(bool, const QJsonObject&)> callback);

    QNetworkAccessManager* m_networkManager;
    QString m_apiKey;
    QString m_apiUrl = "https://api.hunyuan.cloud.tencent.com/v1/chat/completions";
    QString m_model;

    static HunyuanClient* m_instance;
};

#endif // HUNYUANCLIENT_H
