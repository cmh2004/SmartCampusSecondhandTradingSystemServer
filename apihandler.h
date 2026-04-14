#ifndef APIHANDLER_H
#define APIHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include "databasemanager.h"
#include "websocketserver.h"

class ApiHandler : public QObject
{
    Q_OBJECT
public:
    explicit ApiHandler(DatabaseManager *db, WebSocketServer *ws, QObject *parent = nullptr);

    // HTTP请求处理入口
    QByteArray handleRequest(const QString &method, const QString &path, const QByteArray &body);

    // WebSocket消息处理
    void handleWebSocketMessage(int senderId, const QJsonObject &message);

    bool authenticate(const QJsonObject &request, int &userId, QString &role);
    bool authenticate(const QString &token, int &userId, QString &role);
    QJsonObject handleUploadImage(int userId, const QJsonObject &data);

private:
    // 辅助函数
    QJsonObject parseJson(const QByteArray &data);
    QByteArray buildJsonResponse(const QJsonObject &data);

    // ========== 认证模块 ==========
    QJsonObject handleLogin(const QJsonObject &data);
    QJsonObject handleRegister(const QJsonObject &data);
    QJsonObject handleLogout(int userId);
    QJsonObject handleRefreshToken(int userId);

    // ========== 用户模块 ==========
    QJsonObject handleGetUserProfile(int userId, const QJsonObject &data);
    QJsonObject handleUpdateUserProfile(int userId, const QJsonObject &data);
    QJsonObject handleUploadAvatar(int userId, const QJsonObject &data);
    QJsonObject handleGetMyReviews(int userId, const QJsonObject &data);
    QJsonObject handleGetMyGoods(int userId, const QJsonObject &data);
    QJsonObject handleGetBrowseHistory(int userId, const QJsonObject &data);
    QJsonObject handleAddBrowseHistory(int userId, const QJsonObject &data);
    QJsonObject handleChangePassword(int userId, const QJsonObject &data);

    // ========== 商品模块 ==========
    QJsonObject handlePublishGoods(int userId, const QJsonObject &data);
    QJsonObject handleSearchGoods(const QJsonObject &data);
    QJsonObject handleGetGoodsDetail(int userId,const QJsonObject &data);
    QJsonObject handleUpdateGoods(int userId, const QJsonObject &data);
    QJsonObject handleDeleteGoods(int userId, const QJsonObject &data);
    QJsonObject handleGetRecommendedGoods(const QJsonObject &data);
    QJsonObject handleUpdateGoodsStatus(int userId, const QJsonObject &data);


    // ========== 订单模块 ==========
    QJsonObject handleCreateOrder(int userId, const QJsonObject &data);
    QJsonObject handlePayOrder(int userId, const QJsonObject &data);
    QJsonObject handleConfirmOrder(int userId, const QJsonObject &data);
    QJsonObject handleCancelOrder(int userId, const QJsonObject &data);
    QJsonObject handleGetOrderList(int userId, const QJsonObject &data);
    QJsonObject handleGetOrderDetail(int userId, const QJsonObject &data);

    // ========== 聊天模块 ==========
    QJsonObject handleSendMessage(int userId, const QJsonObject &data);
    QJsonObject handleGetMessageHistory(int userId, const QJsonObject &data);
    QJsonObject handleGetChatList(int userId);
    QJsonObject handleMarkMessageRead(int userId, const QJsonObject &data);

    // ========== 评价模块 ==========
    QJsonObject handleSubmitReview(int userId, const QJsonObject &data);
    QJsonObject handleGetGoodsReviews(const QJsonObject &data);
    QJsonObject handleGetSellerReviews(const QJsonObject &data);

    // ========== 收藏模块 ==========
    QJsonObject handleAddFavorite(int userId, const QJsonObject &data);
    QJsonObject handleRemoveFavorite(int userId, const QJsonObject &data);
    QJsonObject handleGetFavorites(int userId, const QJsonObject &data);

    // ========== 信用模块 ==========
    QJsonObject handleGetCreditScore(int userId, const QJsonObject &data);
    QJsonObject handleGetCreditHistory(int userId, const QJsonObject &data);

    // ========== 举报模块 ==========
    QJsonObject handleSubmitReport(int userId, const QJsonObject &data);
    QJsonObject handleGetMyReports(int userId, const QJsonObject &data);

    // ========== 纠纷模块 ==========
    QJsonObject handleSubmitDispute(int userId, const QJsonObject &data);
    QJsonObject handleGetDisputeDetail(int userId, const QJsonObject &data);
    QJsonObject handleGetMyDisputes(int userId, const QJsonObject &data);
    QJsonObject handleGetDisputeByOrder(int userId, const QJsonObject &data);

    // ========== 管理员模块 ==========
    QJsonObject handleGetPendingGoods(int userId, const QJsonObject &data);
    QJsonObject handleReviewGoods(int userId, const QJsonObject &data);
    QJsonObject handleGetUserList(int userId, const QJsonObject &data);
    QJsonObject handleUpdateUserStatus(int userId, const QJsonObject &data);
    QJsonObject handleGetDisputeList(int userId, const QJsonObject &data);
    QJsonObject handleProcessDispute(int userId, const QJsonObject &data);
    QJsonObject handleGetStatistics(int userId, const QJsonObject &data);
    QJsonObject handleGetAllReports(int userId, const QJsonObject &data);
    QJsonObject handleProcessReport(int userId, const QJsonObject &data);
    QJsonObject handleUpdateCreditScore(int userId, const QJsonObject &data);
    QJsonObject handleGetGoodsForReview(int userId, const QJsonObject &data);

    // ========== AI模块 ==========
    QJsonObject handleEstimatePrice(int userId, const QJsonObject &data);
    QJsonObject handleAISearch(int userId, const QJsonObject &data);

    DatabaseManager *m_db;
    WebSocketServer *m_ws;

    // 简单的内存token管理（生产环境应使用JWT或Redis）
    QMap<QString, int> m_tokenToUserId;   // token -> userId
};

#endif // APIHANDLER_H
