#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include "httpserver.h"
#include "logger.h"
#include "websocketserver.h"
#include "apihandler.h"
#include "databasemanager.h"
#include "hunyuanclient.h"

void checkTimeoutOrders() {
    QSqlQuery query(DatabaseManager::instance()->getDb());
    // 查找超过24小时未支付的订单（status=0）且未取消
    query.exec("SELECT id, buyer_id FROM `order` WHERE status = 0 AND create_time < DATE_SUB(NOW(), INTERVAL 24 HOUR)");
    while (query.next()) {
        int orderId = query.value(0).toInt();
        int buyerId = query.value(1).toInt();
        // 应用扣分规则（超过月限次数会自动忽略）
        if (DatabaseManager::instance()->applyCreditChange(buyerId, "ORDER_TIMEOUT", orderId)) {
            // 取消订单
            DatabaseManager::instance()->updateOrderStatus(orderId, 3, "系统自动取消：超时未支付");
            qInfo() << "Order" << orderId << "cancelled due to timeout, buyer" << buyerId << "credit deducted";
        }
    }
}

void awardNoViolationBonus() {
    QSqlQuery query(DatabaseManager::instance()->getDb());
    // 选择过去30天内没有扣分记录且信用分未满100的用户
    query.exec(R"(
        SELECT DISTINCT u.id
        FROM `user` u
        WHERE u.id NOT IN (
            SELECT DISTINCT user_id FROM credit_record
            WHERE change_value < 0 AND create_time > DATE_SUB(NOW(), INTERVAL 30 DAY)
        )
        AND u.credit_score < 100
    )");
    while (query.next()) {
        int userId = query.value(0).toInt();
        if (DatabaseManager::instance()->applyCreditChange(userId, "NO_VIOLATION_30D")) {
            qInfo() << "User" << userId << "received no-violation bonus";
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 初始化日志：保存到 logs/ 目录，只记录 INFO 及以上级别，同时输出到控制台
    Logger::instance()->initialize("logs", Logger::Info, true, 10, 5);
    qInfo() << "Application started";

    // 读取配置文件
    QString configPath = QDir::currentPath() + "/config.json";
    QFile configFile(configPath);
    QJsonObject config;
    if (!configFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open config.json, please create it from config.example.json";
        return 1;
    } else {
        QByteArray configData = configFile.readAll();
        QJsonDocument configDoc = QJsonDocument::fromJson(configData);
        if (!configDoc.isNull() && configDoc.isObject()) {
            config = configDoc.object();
        } else {
            qCritical() << "Invalid config.json format";
            return 1;
        }
    }

    // 初始化数据库（配置从 config.json 读取）
    QString dbHost = config.value("db_host").toString("localhost");
    QString dbName = config.value("db_name").toString("smartcampussecondhandtradingsystem");
    QString dbUser = config.value("db_user").toString("root");
    QString dbPassword = config.value("db_password").toString();
    int dbPort = config.value("db_port").toInt(3306);

    if (dbPassword.isEmpty()) {
        qCritical() << "db_password is empty in config.json";
        return 1;
    }

    DatabaseManager *db = DatabaseManager::instance();
    if (!db->initialize(dbHost, dbName, dbUser, dbPassword, dbPort)) {
        qCritical() << "Database initialization failed";
        return 1;
    }
    qInfo() << "Database initialized from config file:" << dbHost << dbName;

    // 读取混元 API Key
    QString hunyuanApiKey = config.value("hunyuan_api_key").toString();
    if (!hunyuanApiKey.isEmpty()) {
        QString hunyuanModel = config.value("hunyuan_model").toString("hunyuan-vision");
        HunyuanClient::instance()->initialize(hunyuanApiKey, hunyuanModel);
        qInfo() << "Hunyuan AI initialized from config file";
    } else {
        qWarning() << "No hunyuan_api_key in config.json";
    }

    // 启动订单超时检查（每小时一次）
    QTimer *timeoutTimer = new QTimer();
    QObject::connect(timeoutTimer, &QTimer::timeout, checkTimeoutOrders);
    timeoutTimer->start(3600 * 1000); // 3600秒 = 1小时
    // 立即执行一次
    checkTimeoutOrders();

    // 启动月度无违规奖励（每月1号凌晨执行）
    QDateTime now = QDateTime::currentDateTime();
    QDate nextMonthFirst = QDate(now.date().year(), now.date().month(), 1).addMonths(1);
    QDateTime executeTime(nextMonthFirst, QTime(0, 0, 0));
    int msecsToExecute = now.msecsTo(executeTime);
    if (msecsToExecute < 0) msecsToExecute = 0;

    QTimer *monthlyTimer = new QTimer();
    monthlyTimer->setSingleShot(true);
    QObject::connect(monthlyTimer, &QTimer::timeout, []() {
        awardNoViolationBonus();
        // 之后每月执行（用另一个定时器，间隔30天，简单处理）
        QTimer *repeatTimer = new QTimer();
        repeatTimer->start(30 * 24 * 3600 * 1000);
        QObject::connect(repeatTimer, &QTimer::timeout, awardNoViolationBonus);
    });
    monthlyTimer->start(msecsToExecute);

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
    httpServer.route("POST", "/api/auth/send_reset_code", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/send_reset_code", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/auth/reset_password", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/reset_password", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/auth/check_account_email", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/auth/check_account_email", QJsonDocument(data).toJson());
    });

    // 用户资料
    httpServer.route("POST", "/api/user/get_profile", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/user/get_profile", QJsonDocument(data).toJson());
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
    // 个性化推荐（协同过滤）
    httpServer.route("POST", "/api/recommend/collaborative", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/recommend/collaborative", QJsonDocument(data).toJson());
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
    httpServer.route("POST", "/api/order/detail", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/order/detail", QJsonDocument(data).toJson());
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
    httpServer.route("POST", "/api/review/seller", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/review/seller", QJsonDocument(data).toJson());
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

    //系统消息
    httpServer.route("POST", "/api/system/messages", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/system/messages", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/system/messages/read", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/system/messages/read", QJsonDocument(data).toJson());
    });
    httpServer.route("POST", "/api/system/messages/unread_count", [&](const QJsonObject &data) {
        return apiHandler.handleRequest("POST", "/api/system/messages/unread_count", QJsonDocument(data).toJson());
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