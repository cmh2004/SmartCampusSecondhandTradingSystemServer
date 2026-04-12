#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QDateTime>
#include <QCryptographicHash>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager* instance();

    // 初始化MySQL连接
    bool initialize(const QString& host, const QString& database,
                    const QString& username, const QString& password,
                    int port = 3306);
    QSqlDatabase getDb();
    int getCategoryIdByName(const QString &name);

    // ==================== 用户相关 ====================
    // 登录验证（根据账号和明文密码返回用户信息，若失败返回空对象）
    QJsonObject login(const QString& account, const QString& plainPassword);
    QJsonObject getUserById(int userId);
    QJsonObject getUserByAccount(const QString& account);
    bool addUser(const QJsonObject& user);  // 需包含 account, password, nickname, email, phone, role 等
    bool updateUser(int userId, const QJsonObject& updates);
    bool updateUserCreditScore(int userId, int delta, const QString& reason); // 信用分变更并记录
    QJsonArray getUserList(const QString& role = "", const QString& status = "", int page = 1, int pageSize = 20);
    bool updateUserStatus(int userId, int status, const QString& reason = "");
    QJsonArray getCreditHistory(int userId, int page, int pageSize);
    bool updateLastLoginTime(int userId);

    // ==================== 商品相关 ====================
    bool addGoods(const QJsonObject& goods); // 需包含 seller_id, category_id, name, price, description 等
    QJsonObject getGoods(int goodsId);
    bool updateGoods(int goodsId, const QJsonObject& updates);
    bool updateGoodsStatus(int goodsId, int status);
    bool deleteGoods(int goodsId);
    QJsonArray searchGoods(const QString& keyword = "",
                           int categoryId = 0,
                           double minPrice = 0,
                           double maxPrice = 0,
                           const QString& sortBy = "publish_time",
                           int page = 1,
                           int pageSize = 20);
    QJsonArray getGoodsBySeller(int sellerId, int status = -1, int page = 1, int pageSize = 20);
    // 获取商品图片列表
    QJsonArray getGoodsImages(int goodsId);
    QJsonArray getRecommendedGoods(int limit);
    QJsonArray getPendingGoods(int page,int pageSize);
    int getLastInsertId();
    bool addGoodsImage(int goodsId, const QString& imageUrl, int sortOrder = 0);

    // ==================== 订单相关 ====================
    bool addOrder(const QJsonObject& order); // 需包含 buyer_id, seller_id, goods_id, deal_price, goods_title, goods_image 等
    QJsonObject getOrder(int orderId);
    QJsonObject getOrderBySn(const QString& orderSn);
    QJsonArray getOrdersByUser(int userId, int status, const QString &keyword = "", int page = 1, int pageSize = 20);
    QJsonArray getOrdersBySeller(int sellerId, int status = -1, int page = 1, int pageSize = 20);
    bool updateOrderStatus(int orderId, int status, const QString& reason = "");
    bool updateOrderPayment(int orderId, qint64 paymentTime); // 支付成功

    // ==================== 收藏相关 ====================
    bool addFavorite(int userId, int goodsId);
    bool removeFavorite(int userId, int goodsId);
    bool isFavorited(int userId, int goodsId);
    QJsonArray getFavorites(int userId, int page = 1, int pageSize = 20);

    // ==================== 评价相关 ====================
    bool addEvaluation(const QJsonObject& evaluation); // 需包含 order_id, buyer_id, seller_id, score, content 等
    QJsonArray getEvaluationsByUser(int userId, bool asSeller = true, int page = 1, int pageSize = 20);
    QJsonArray getEvaluationsByGoods(int goodsId, int page = 1, int pageSize = 20);
    double getAverageScore(int userId, bool asSeller); // 获取卖家平均分

    // ==================== 聊天相关 ====================
    bool addChatMessage(const QJsonObject& message); // 需包含 session_id, buyer_id, seller_id, goods_id, content 等
    QJsonArray getChatMessages(const QString& sessionId, int limit = 50, int offset = 0);
    bool markChatAsRead(const QString& sessionId, int userId); // 将某个会话中对方发送的消息标记为已读
    int getUnreadCount(int userId);
    QJsonArray getChatList(int userId);

    // ==================== 举报相关 ====================
    bool addReport(const QJsonObject& report); // 需包含 reporter_id, reported_type, reported_id, reason_type, description 等
    QJsonArray getReports(const QString& status = "", int page = 1, int pageSize = 20);
    bool updateReportStatus(int reportId, int status, const QString& result = "", int adminId = 0);
    QJsonArray getMyReports(int userId, int page = 1, int pageSize = 20);

    // ==================== 纠纷相关 ====================
    bool addDispute(const QJsonObject& dispute); // 需包含 order_id, complainant_id, defendant_id, type, description 等
    QJsonObject getDispute(int disputeId);
    QJsonArray getDisputes(int status = -1, int page = 1, int pageSize = 20);
    bool updateDispute(int disputeId, int status, const QString& result = "", int adminId = 0);
    QJsonArray getMyDisputes(int userId, int status = -1, int page = 1, int pageSize = 20);
    QJsonArray getDisputesByOrder(int orderId);

    // ==================== 系统消息相关 ====================
    bool addSystemMessage(int userId, int messageType, const QString& title,
                          const QString& content, int relatedId = 0);
    QJsonArray getSystemMessages(int userId, bool unreadOnly = false, int page = 1, int pageSize = 20);
    bool markSystemMessageRead(int messageId);
    int getUnreadSystemMessageCount(int userId);

    // ==================== AI估价记录 ====================
    bool addAIValuation(const QJsonObject& valuation); // 需包含 goods_id, user_id, description, image_url, min_price, max_price, confidence

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    // 辅助函数：执行查询返回单个对象
    QJsonObject execSelectOne(const QString& sql, const QList<QVariant>& bindValues);
    QJsonArray execSelectMany(const QString& sql, const QList<QVariant>& bindValues);
    bool execInsert(const QString& sql, const QList<QVariant>& bindValues);
    bool execUpdateDelete(const QString& sql, const QList<QVariant>& bindValues);

    // 生成随机盐值
    static QString generateSalt(int length = 16);
    // 计算密码哈希：MD5(plainPassword + salt)
    static QString hashPassword(const QString& plainPassword, const QString& salt);

    QSqlDatabase m_db;
    QRecursiveMutex  m_mutex;
    static DatabaseManager* m_instance;
};

#endif // DATABASEMANAGER_H
