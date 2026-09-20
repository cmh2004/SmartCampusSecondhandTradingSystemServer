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
    bool addUser(const QJsonObject& user);
    bool updateUser(int userId, const QJsonObject& updates);
    bool updateUserCreditScore(int userId, int delta, const QString& reason); // 信用分变更并记录
    QJsonArray getUserList(const QString& role = "", const QString& status = "", const QString& keyword = "", int page = 1, int pageSize = 20);
    bool updateUserStatus(int userId, int status, const QString& reason = "");
    QJsonArray getCreditHistory(int userId, int page, int pageSize);
    bool updateLastLoginTime(int userId);
    QJsonArray getMyReviews(int userId, int page = 1, int pageSize = 20);
    bool addOrUpdateBrowseHistory(int userId, int goodsId);
    QJsonArray getBrowseHistory(int userId, int page, int pageSize);
    // 获取用户信用分最后更新时间
    QString getCreditScoreLastUpdateTime(int userId);
    QString getUserEmailByAccount(const QString& account);
    QString getMaskedEmail(const QString& email);

    // ==================== 商品相关 ====================
    bool addGoods(const QJsonObject& goods);
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
                           int pageSize = 20,const QString& school = "");
    QJsonArray getGoodsBySeller(int sellerId, int status = -1, int page = 1, int pageSize = 20);
    // 获取商品图片列表
    QJsonArray getGoodsImages(int goodsId);
    QJsonArray getRecommendedGoods(int limit);
    QJsonArray getPendingGoods(int page,int pageSize);
    int getLastInsertId();
    bool addGoodsImage(int goodsId, const QString& imageUrl, int sortOrder = 0);
    QJsonArray getGoodsForReview(const QString& keyword, int status, const QString& startDate, const QString& endDate, int page, int pageSize);
    // 获取用户交互过的商品ID列表（收藏+购买成功）
    QJsonArray getUserInteractedGoods(int userId);
    // 基于物品协同过滤获取推荐商品（按共同行为人数排序）
    QJsonArray getCollaborativeRecommendations(int userId, int limit, const QString& school = "");
    // 热度推荐（降级方案）
    QJsonArray getHotRecommendations(int limit);

    // ==================== 订单相关 ====================
    bool addOrder(const QJsonObject& order);
    QJsonObject getOrder(int orderId);
    QJsonObject getOrderBySn(const QString& orderSn);
    QJsonArray getOrdersByUser(int userId, int status, const QString &keyword = "", int page = 1, int pageSize = 20);
    QJsonArray getOrdersBySeller(int sellerId, int status = -1, int page = 1, int pageSize = 20);
    bool updateOrderStatus(int orderId, int status, const QString& reason = "");
    bool updateOrderPayment(int orderId, qint64 paymentTime); // 支付成功
    // 检查订单是否存在未关闭的纠纷（status != 3）
    bool hasActiveDispute(int orderId);
    // 检查订单是否存在已解决的纠纷（status == 2）
    bool hasResolvedDispute(int orderId);

    // ==================== 收藏相关 ====================
    bool addFavorite(int userId, int goodsId);
    bool removeFavorite(int userId, int goodsId);
    bool isFavorited(int userId, int goodsId);
    QJsonArray getFavorites(int userId, int page = 1, int pageSize = 20);

    // ==================== 评价相关 ====================
    bool addEvaluation(const QJsonObject& evaluation);
    QJsonArray getEvaluationsByUser(int userId, bool asSeller = true, int page = 1, int pageSize = 20);
    QJsonArray getEvaluationsByGoods(int goodsId, int page = 1, int pageSize = 20);
    double getAverageScore(int userId, bool asSeller); // 获取卖家平均分

    // ==================== 聊天相关 ====================
    bool addChatMessage(const QJsonObject& message);
    QJsonArray getChatMessages(const QString& sessionId, int limit = 50, int offset = 0);
    bool markChatAsRead(const QString& sessionId, int userId); // 将某个会话中对方发送的消息标记为已读
    int getUnreadCount(int userId);
    QJsonArray getChatList(int userId);

    // ==================== 举报相关 ====================
    bool addReport(const QJsonObject& report);
    QJsonArray getReports(const QString& status = "", int page = 1, int pageSize = 20);
    bool updateReportStatus(int reportId, int status, const QString& result = "", int adminId = 0);
    QJsonArray getMyReports(int userId, int page = 1, int pageSize = 20);
    QJsonObject getReportById(int reportId);

    // ==================== 纠纷相关 ====================
    bool addDispute(const QJsonObject& dispute);
    QJsonObject getDispute(int disputeId);
    QJsonArray getDisputes(int status = -1, int page = 1, int pageSize = 20);
    bool updateDispute(int disputeId, int status, const QString& result = "", int adminId = 0);
    QJsonArray getMyDisputes(int userId, int status = -1, int page = 1, int pageSize = 20);
    QJsonArray getDisputesByOrder(int orderId);

    // ==================== 系统消息相关 ====================
    bool addSystemMessage(int userId, int messageType, const QString& title,
                          const QString& content, int relatedId = 0);
    QJsonArray getSystemMessages(int userId, bool unreadOnly = false, int page = 1, int pageSize = 20);
    bool markSystemMessageRead(int messageId, int userId);
    int getUnreadSystemMessageCount(int userId);

    // ==================== AI估价记录 ====================
    bool addAIValuation(const QJsonObject& valuation);
    // 获取商品的 AI 估价记录
    QJsonObject getAIValuationByGoodsId(int goodsId);

    // 生成随机盐值
    static QString generateSalt(int length = 16);

    bool applyCreditChange(int userId, const QString& ruleCode, int relatedId = 0, int adminId = 0, int overrideChangeValue = 0);
    int getMonthlyCreditChangeCount(int userId, const QString& ruleCode);
    // 用户管理相关
    int getUserCount(const QString& role = "", const QString& status = "", const QString& keyword = "");
    // 待审核商品计数
    int getPendingGoodsCount();
    // 纠纷计数（按状态筛选）
    int getDisputeCount(const QString& status = "");
    // 举报计数（按状态筛选）
    int getReportCount(const QString& status = "");
    int getGoodsForReviewCount(const QString &keyword, int status,
                               const QString &startDate, const QString &endDate);
    int getGoodsCount(const QString& keyword = "",
                      int categoryId = 0,
                      double minPrice = 0,
                      double maxPrice = 0,
                      const QString& school = "");

    // 商品相关（已有 getGoodsCount，再增加按卖家计数的版本）
    int getGoodsCountBySeller(int sellerId, int status = -1);
    // 订单相关
    int getOrdersCount(int userId, int status, const QString& keyword = "");
    // 收藏相关
    int getFavoritesCount(int userId);
    // 评价相关（我的评价，买家给出的）
    int getMyReviewsCount(int userId);
    // 浏览历史相关
    int getBrowseHistoryCount(int userId);
    // 信用历史相关
    int getCreditHistoryCount(int userId);
    // 系统消息相关
    int getSystemMessagesCount(int userId, bool unreadOnly = false);

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    // 辅助函数：执行查询返回单个对象
    QJsonObject execSelectOne(const QString& sql, const QList<QVariant>& bindValues);
    QJsonArray execSelectMany(const QString& sql, const QList<QVariant>& bindValues);
    bool execInsert(const QString& sql, const QList<QVariant>& bindValues);
    bool execUpdateDelete(const QString& sql, const QList<QVariant>& bindValues);

    // 计算密码哈希：MD5(plainPassword + salt)
    static QString hashPassword(const QString& plainPassword, const QString& salt);

    QSqlDatabase m_db;
    QRecursiveMutex  m_mutex;
    static DatabaseManager* m_instance;
};

#endif // DATABASEMANAGER_H
