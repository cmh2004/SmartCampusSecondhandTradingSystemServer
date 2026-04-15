#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include "httpserver.h"
#include "websocketserver.h"
#include "apihandler.h"
#include "databasemanager.h"
#include "hunyuanclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 初始化数据库
    DatabaseManager *db = DatabaseManager::instance();
    if (!db->initialize("localhost", "smartcampussecondhandtradingsystem", "root", "Cmh20041104Cmh", 3306)) {
        qCritical() << "Database initialization failed";
        return 1;
    }

    // 读取配置文件
    QString configPath = QDir::currentPath() + "/config.json";
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open config.json";
    } else {
        QByteArray configData = configFile.readAll();
        QJsonDocument configDoc = QJsonDocument::fromJson(configData);
        if (!configDoc.isNull() && configDoc.isObject()) {
            QJsonObject config = configDoc.object();

            // 读取混元 API Key
            QString hunyuanApiKey = config.value("hunyuan_api_key").toString();
            if (!hunyuanApiKey.isEmpty()) {
                QString hunyuanModel = config.value("hunyuan_model").toString("hunyuan-vision");
                HunyuanClient::instance()->initialize(hunyuanApiKey, hunyuanModel);
                qInfo() << "Hunyuan AI initialized from config file";
            } else {
                qWarning() << "No hunyuan_api_key in config.json";
            }
        } else {
            qWarning() << "Invalid config.json format";
        }
    }

    // 创建WebSocket服务器（端口8081）
    WebSocketServer wsServer(8081);

    // 创建API处理器
    ApiHandler apiHandler(db, &wsServer);

    // 创建HTTP服务器（端口8080）
    HttpServer httpServer;

    // 注册所有路由（与客户端对应的API）
    // 图片上传（Base64方式）
    httpServer.route("POST", "/api/upload/image", [&](const QJsonObject &data) {
        int userId = -1;
        QString role;
        apiHandler.authenticate(data.value("token").toString(), userId, role);
        QJsonObject response = apiHandler.handleUploadImage(userId, data);
        // 将 QJsonObject 转换为 QByteArray
        return QJsonDocument(response).toJson(QJsonDocument::Compact);
    });

    // 用户认证
    httpServer.route("POST", "/api/auth/login", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/login", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/auth/register", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/register", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/auth/logout", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/logout", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/auth/refresh", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/refresh", QJsonDocument(data).toJson());
    });

    // 用户资料
    httpServer.route("GET", "/api/user/profile", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/user/profile", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/user/profile", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/user/profile", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/user/avatar", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/user/avatar", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/user/goods", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/user/goods", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/user/reviews", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/user/reviews", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/user/history", [&](const QJsonObject &data) {
        QByteArray body = QJsonDocument(data).toJson();
        return apiHandler.handleRequest("POST", "/api/user/history", body);
    });
    httpServer.route("POST", "/api/user/history/add", [&](const QJsonObject &data) {
        QByteArray body = QJsonDocument(data).toJson();
        return apiHandler.handleRequest("POST", "/api/user/history/add", body);
    });
    httpServer.route("POST", "/api/user/change_password", [&](const QJsonObject &data) {
        QByteArray body = QJsonDocument(data).toJson();
        return apiHandler.handleRequest("POST", "/api/user/change_password", body);
    });

    // 商品
    httpServer.route("POST", "/api/goods/publish", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/publish", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/goods/search", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/search", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/goods/detail", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/detail", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/goods/update", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/update", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/goods/delete", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/delete", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/goods/recommended", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/goods/recommended", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/goods/update_status", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/goods/update_status", QJsonDocument(data).toJson());
    });

    // 订单
    httpServer.route("POST", "/api/order/create", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/create", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/order/pay", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/pay", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/order/confirm", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/confirm", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/order/cancel", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/cancel", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/order/list", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/list", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/order/detail", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/order/detail", QJsonDocument(data).toJson());
    });

    // 消息
    httpServer.route("POST", "/api/message/send", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/message/send", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/message/history", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/message/history", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/message/chatlist", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/message/chatlist", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/message/mark_read", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/message/mark_read", QJsonDocument(data).toJson());
    });

    // 评价
    httpServer.route("POST", "/api/review/submit", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/review/submit", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/review/goods", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/review/goods", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/review/seller", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/review/seller", QJsonDocument(data).toJson());
    });

    // 收藏
    httpServer.route("POST", "/api/favorite/add", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/favorite/add", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/favorite/remove", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/favorite/remove", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/favorite/list", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/favorite/list", QJsonDocument(data).toJson());
    });

    // 信用
    httpServer.route("POST", "/api/credit/score", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/credit/score", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/credit/history", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/credit/history", QJsonDocument(data).toJson());
    });

    // 举报
    httpServer.route("POST", "/api/report/submit", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/report/submit", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/report/mylist", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/report/mylist", QJsonDocument(data).toJson());
    });

    // 纠纷
    httpServer.route("POST", "/api/dispute/submit", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/dispute/submit", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/dispute/detail", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/dispute/detail", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/dispute/mylist", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/dispute/mylist", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/dispute/by_order", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/dispute/by_order", QJsonDocument(data).toJson());
    });

    // 管理员
    httpServer.route("POST", "/api/admin/pending_goods", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/pending_goods", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/review_goods", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/review_goods", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/user_list", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/user_list", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/update_user_status", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/update_user_status", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/dispute_list", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/dispute_list", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/process_dispute", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/process_dispute", QJsonDocument(data).toJson());
    });
    httpServer.route("GET", "/api/admin/statistics", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("GET", "/api/admin/statistics", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/reports", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/reports", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/process_report", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/process_report", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/update_credit", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/update_credit", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/admin/goods_review_list", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/admin/goods_review_list", QJsonDocument(data).toJson());
    });

    // AI估价
    httpServer.route("POST", "/api/ai/estimate", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/ai/estimate", QJsonDocument(data).toJson());
    });
    // AI 智能检索
    httpServer.route("POST", "/api/ai/search", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/ai/search", QJsonDocument(data).toJson());
    });

    // 启动HTTP服务器
    if (!httpServer.listen(QHostAddress::Any, 8080)) {
        qCritical() << "HTTP server failed to start";
        return 1;
    }
    qInfo() << "HTTP server started on port 8080";
    qInfo() << "WebSocket server started on port 8081";

    return a.exec();
}
