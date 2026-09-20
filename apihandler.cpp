#include "apihandler.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QRandomGenerator>
#include "hunyuanclient.h"

ApiHandler::ApiHandler(DatabaseManager *db, WebSocketServer *ws, QObject *parent)
    : QObject(parent), m_db(db), m_ws(ws)
{
    // 连接WebSocket信号
    connect(m_ws, &WebSocketServer::messageReceived, this, &ApiHandler::handleWebSocketMessage);
}

QByteArray ApiHandler::handleRequest(const QString &method, const QString &path, const QByteArray &body)
{
    QJsonObject request = parseJson(body);
    QJsonObject response;

    // 提取token（从请求体或头部）
    QString token = request.value("token").toString();

    // 认证（除了登录、注册等开放接口）
    bool needAuth = true;
    if ((method == "POST" && path == "/api/auth/login") ||
        (method == "POST" && path == "/api/auth/register" ||
        (method == "POST" && path == "/api/auth/send_reset_code") ||
        (method == "POST" && path == "/api/auth/reset_password") ||
        (method == "POST" && path == "/api/auth/check_account_email"))) {
        needAuth = false;
    }

    int userId = -1;
    QString role;
    if (needAuth) {
        if (!authenticate(token, userId, role)) {
            response["success"] = false;
            response["error"] = "Unauthorized";
            response["code"] = 401;
            return buildJsonResponse(response);
        }
    }

    // 路由分发
    QString routeKey = method + ":" + path;
    if (routeKey == "POST:/api/auth/login") {
        response = handleLogin(request);
    } else if (routeKey == "POST:/api/auth/register") {
        response = handleRegister(request);
    } else if (routeKey == "POST:/api/auth/logout") {
        response = handleLogout(userId);
    } else if (routeKey == "POST:/api/auth/refresh") {
        response = handleRefreshToken(userId);
    } else if (routeKey == "POST:/api/auth/send_reset_code") {
        response = handleSendResetCode(request);
    } else if (routeKey == "POST:/api/auth/reset_password") {
        response = handleResetPassword(request);
    } else if (routeKey == "POST:/api/auth/check_account_email") {
        response = handleCheckAccountEmail(request);
    } else if (routeKey == "POST:/api/user/get_profile") {
        response = handleGetUserProfile(userId, request);
    } else if (routeKey == "POST:/api/user/profile") {
        response = handleUpdateUserProfile(userId, request);
    } else if (routeKey == "POST:/api/user/avatar") {
        response = handleUploadAvatar(userId, request);
    } else if (routeKey == "POST:/api/user/goods") {
        response = handleGetMyGoods(userId, request);
    } else if (routeKey == "POST:/api/user/reviews") {
        response = handleGetMyReviews(userId, request);
    } else if (routeKey == "POST:/api/user/history") {
        response = handleGetBrowseHistory(userId, request);
    } else if (routeKey == "POST:/api/user/history/add") {
        response = handleAddBrowseHistory(userId, request);
    } else if (routeKey == "POST:/api/user/change_password") {
        response = handleChangePassword(userId, request);
    } else if (routeKey == "POST:/api/goods/publish") {
        response = handlePublishGoods(userId, request);
    } else if (routeKey == "POST:/api/goods/search") {
        response = handleSearchGoods(request);
    } else if (routeKey == "POST:/api/goods/detail") {
        response = handleGetGoodsDetail(userId,request);
    } else if (routeKey == "POST:/api/goods/update") {
        response = handleUpdateGoods(userId, request);
    } else if (routeKey == "POST:/api/goods/delete") {
        response = handleDeleteGoods(userId, request);
    } else if (routeKey == "GET:/api/goods/recommended") {
        response = handleGetRecommendedGoods(request);
    } else if (routeKey == "POST:/api/goods/update_status") {
        response = handleUpdateGoodsStatus(userId,request);
    } else if (routeKey == "POST:/api/recommend/collaborative") {
        response = handleGetCollaborativeRecommendations(userId, request);
    } else if (routeKey == "POST:/api/order/create") {
        response = handleCreateOrder(userId, request);
    } else if (routeKey == "POST:/api/order/pay") {
        response = handlePayOrder(userId, request);
    } else if (routeKey == "POST:/api/order/confirm") {
        response = handleConfirmOrder(userId, request);
    } else if (routeKey == "POST:/api/order/cancel") {
        response = handleCancelOrder(userId, request);
    } else if (routeKey == "POST:/api/order/list") {
        response = handleGetOrderList(userId, request);
    } else if (routeKey == "POST:/api/order/detail") {
        response = handleGetOrderDetail(userId, request);
    } else if (routeKey == "POST:/api/message/send") {
        response = handleSendMessage(userId, request);
    } else if (routeKey == "POST:/api/message/history") {
        response = handleGetMessageHistory(userId, request);
    } else if (routeKey == "GET:/api/message/chatlist") {
        response = handleGetChatList(userId);
    } else if (routeKey == "POST:/api/review/submit") {
        response = handleSubmitReview(userId, request);
    } else if (routeKey == "GET:/api/review/goods") {
        response = handleGetGoodsReviews(request);
    } else if (routeKey == "POST:/api/review/seller") {
        response = handleGetSellerReviews(request);
    } else if (routeKey == "POST:/api/favorite/add") {
        response = handleAddFavorite(userId, request);
    } else if (routeKey == "POST:/api/favorite/remove") {
        response = handleRemoveFavorite(userId, request);
    } else if (routeKey == "POST:/api/favorite/list") {
        response = handleGetFavorites(userId, request);
    } else if (routeKey == "POST:/api/credit/score") {
        response = handleGetCreditScore(userId, request);
    } else if (routeKey == "POST:/api/credit/history") {
        response = handleGetCreditHistory(userId, request);
    } else if (routeKey == "POST:/api/report/submit") {
        response = handleSubmitReport(userId, request);
    } else if (routeKey == "GET:/api/report/mylist") {
        response = handleGetMyReports(userId, request);
    } else if (routeKey == "POST:/api/dispute/submit") {
        response = handleSubmitDispute(userId, request);
    } else if (routeKey == "POST:/api/dispute/detail") {
        response = handleGetDisputeDetail(userId, request);
    } else if (routeKey == "GET:/api/dispute/mylist") {
        response = handleGetMyDisputes(userId, request);
    } else if (routeKey == "POST:/api/dispute/by_order") {
        response = handleGetDisputeByOrder(userId, request);
    } else if (routeKey == "POST:/api/admin/pending_goods") {
        response = handleGetPendingGoods(userId, request);
    } else if (routeKey == "POST:/api/admin/review_goods") {
        response = handleReviewGoods(userId, request);
    } else if (routeKey == "POST:/api/admin/user_list") {
        response = handleGetUserList(userId, request);
    } else if (routeKey == "POST:/api/admin/update_user_status") {
        response = handleUpdateUserStatus(userId, request);
    } else if (routeKey == "POST:/api/admin/dispute_list") {
        response = handleGetDisputeList(userId, request);
    } else if (routeKey == "POST:/api/admin/process_dispute") {
        response = handleProcessDispute(userId, request);
    } else if (routeKey == "GET:/api/admin/statistics") {
        response = handleGetStatistics(userId, request);
    } else if (routeKey == "POST:/api/admin/reports") {
        response = handleGetAllReports(userId, request);
    } else if (routeKey == "POST:/api/admin/process_report") {
        response = handleProcessReport(userId, request);
    } else if (routeKey == "POST:/api/admin/update_credit") {
        response = handleUpdateCreditScore(userId, request);
    } else if (routeKey == "POST:/api/admin/goods_review_list") {
        response = handleGetGoodsForReview(userId, request);
    } else if (routeKey == "POST:/api/ai/estimate") {
        response = handleEstimatePrice(userId, request);
    } else if (routeKey == "POST:/api/ai/search") {
        response = handleAISearch(userId, request);
    } else if (routeKey == "POST:/api/system/messages") {
        response = handleGetSystemMessages(userId, request);
    } else if (routeKey == "POST:/api/system/messages/read") {
        response = handleMarkSystemMessageRead(userId, request);
    } else if (routeKey == "POST:/api/system/messages/unread_count") {
        response = handleGetUnreadSystemMessageCount(userId, request);
    } else {
        response["success"] = false;
        response["error"] = "Not Found";
    }

    return buildJsonResponse(response);
}

// 解析JSON
QJsonObject ApiHandler::parseJson(const QByteArray &data)
{
    if (data.isEmpty()) return QJsonObject();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

// 构建JSON响应
QByteArray ApiHandler::buildJsonResponse(const QJsonObject &data)
{
    QJsonDocument doc(data);
    return doc.toJson(QJsonDocument::Compact);
}

// 认证（简化版）
bool ApiHandler::authenticate(const QString &token, int &userId, QString &role)
{
    if (token.isEmpty()) return false;
    if (m_tokenToUserId.contains(token)) {
        userId = m_tokenToUserId[token];
        QJsonObject user = m_db->getUserById(userId);
        if (!user.isEmpty()) {
            role = (user.value("role").toInt() == 1) ? "admin" : "user";
            return true;
        }
    }
    return false;
}

// 登录处理
QJsonObject ApiHandler::handleLogin(const QJsonObject &data)
{
    QString account = data.value("username").toString();
    QString password = data.value("password").toString(); // 客户端已MD5加密
    QString role = data.value("role").toString();

    QJsonObject user = m_db->getUserByAccount(account);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "账号不存在"}};
    }

    // 验证密码（需加盐）
    QString salt = user.value("salt").toString();
    QString passwordHash = QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Md5).toHex();
    if (passwordHash != user.value("password").toString()) {
        return {{"success", false}, {"error", "密码错误"}};
    }

    // 验证密码通过后，检查用户状态
    int status = user.value("status").toString().toInt();
    if (status != 1) {
        return {{"success", false}, {"error", "账号已被封禁，请联系管理员"}};
    }

    // 检查角色是否匹配
    int requestedRole = (role == "admin") ? 1 : 0;
    int actualRole = user.value("role").toString().toInt();
    if (requestedRole != actualRole) {
        return {{"success", false}, {"error", "账号角色不匹配，请选择正确的身份登录"}};
    }

    // 生成token（简单方案：user_id + 随机数）
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int userId = user.value("id").toInt();
    m_tokenToUserId[token] = userId;
    m_db->updateLastLoginTime(userId);

    QJsonObject dataObj;
    dataObj["token"] = token;
    dataObj["user_id"] = userId;
    dataObj["username"] = user.value("account").toString();
    dataObj["nickname"] = user.value("nickname").toString();
    dataObj["avatar"] = user.value("avatar_url").toString();
    dataObj["role"] = user.value("role").toString().toInt() == 1 ? "admin" : "user";
    dataObj["credit_score"] = user.value("credit_score").toInt();
    dataObj["ws_url"] = "ws://127.0.0.1:8081";

    return {{"success", true}, {"data", dataObj}};
}

// 注册处理
QJsonObject ApiHandler::handleRegister(const QJsonObject &data)
{
    QString account = data.value("username").toString();
    QString password = data.value("password").toString(); // 客户端已MD5加密
    QString nickname = data.value("nickname").toString();
    QString email = data.value("email").toString();
    QString phone = data.value("phone").toString();
    int role = data.value("role").toInt(0); // 默认 0，可选 1
    QString school = data.value("school").toString();

    // 检查账号是否已存在
    if (!m_db->getUserByAccount(account).isEmpty()) {
        return {{"success", false}, {"error", "账号已存在"}};
    }

    // 生成盐值
    QString salt = QUuid::createUuid().toString(QUuid::WithoutBraces).left(16);
    QString passwordHash = QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Md5).toHex();

    QJsonObject newUser;
    newUser["account"] = account;
    newUser["password"] = passwordHash;
    newUser["salt"] = salt;
    newUser["nickname"] = nickname;
    newUser["email"] = email;
    newUser["phone"] = phone;
    newUser["role"] = role;
    newUser["school"] = school;

    if (m_db->addUser(newUser)) {
        return {{"success", true}, {"message", "注册成功"}};
    } else {
        return {{"success", false}, {"error", "注册失败，请稍后重试"}};
    }
}

QJsonObject ApiHandler::handleLogout(int userId)
{
    // 移除该用户的所有token（简单实现）
    for (auto it = m_tokenToUserId.begin(); it != m_tokenToUserId.end(); ) {
        if (it.value() == userId) {
            it = m_tokenToUserId.erase(it);
        } else {
            ++it;
        }
    }
    return {{"success", true}, {"message", "登出成功"}};
}

// 获取用户资料
QJsonObject ApiHandler::handleGetUserProfile(int userId, const QJsonObject &data)
{
    QString targetUserId = data.value("user_id").toString();
    int id = targetUserId.isEmpty() ? userId : targetUserId.toInt();
    QJsonObject user = m_db->getUserById(id);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }
    // 移除敏感信息
    user.remove("password");
    user.remove("salt");
    return {{"success", true}, {"data", user}};
}

// 发布商品
QJsonObject ApiHandler::handlePublishGoods(int userId, const QJsonObject &data)
{
    QJsonObject user = m_db->getUserById(userId);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }
    int creditScore = user.value("credit_score").toInt();
    if (creditScore < 60) {
        return {{"success", false}, {"error", "您的信用分低于60分，暂时无法发布商品。请保持良好的交易行为，提高信用分后再试。"}};
    }

    QString name = data.value("name").toString();
    QString description = data.value("description").toString();
    QString school = getUserSchool(userId);

    QJsonObject audit = checkGoodsContent(name, description);
    if (audit.value("is_violation").toBool()) {
        QString reason = audit.value("reason").toString();
        qWarning() << "Goods content violation detected, reason:" << reason;
        return {{"success", false}, {"error", QString("发布失败：商品内容违规。原因：%1").arg(reason)}};
    }

    QJsonObject goods;
    goods["seller_id"] = userId;
    int categoryId = m_db->getCategoryIdByName(data.value("category").toString());
    goods["category_id"] = categoryId;
    goods["name"] = data.value("name").toString();
    goods["price"] = data.value("price").toDouble();
    goods["description"] = data.value("description").toString();
    goods["status"] = 0; // 待审核
    goods["school"] = school;

    if (m_db->addGoods(goods)) {
        // 获取刚插入的商品ID
        int goodsId = m_db->getLastInsertId();
        // 处理图片
        QJsonArray images = data.value("images").toArray();
        for (int i = 0; i < images.size(); ++i) {
            QString imageUrl = images[i].toString();
            m_db->addGoodsImage(goodsId, imageUrl, i);  // 需要实现此方法
        }
        return {{"success", true}, {"message", "发布成功，等待审核"}};
    } else {
        return {{"success", false}, {"error", "发布失败"}};
    }
}

// 搜索商品
QJsonObject ApiHandler::handleSearchGoods(const QJsonObject &data)
{
    QString keyword = data.value("keyword").toString();
    int categoryId = data.value("category_id").toInt();
    if (categoryId == 0 && data.contains("category")) {
        QString categoryName = data.value("category").toString();
        categoryId = m_db->getCategoryIdByName(categoryName);
    }
    double minPrice = data.value("min_price").toDouble();
    double maxPrice = data.value("max_price").toDouble();
    QString sortBy = data.value("sort_by").toString("publish_time");
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    bool schoolOnly = data.value("school_only").toBool(false);   // 默认 false
    QString schoolFilter;
    if (schoolOnly) {
        // 从请求中获取 token（已在认证层存入 request 对象或需要从 data 中取）
        QString token = data.value("token").toString();
        int userId = -1;
        QString role;
        if (authenticate(token, userId, role)) {
            schoolFilter = getUserSchool(userId);
        }
        // 如果未登录或未填学校，则 schoolFilter 为空，不筛选
    }

    QJsonArray goodsList = m_db->searchGoods(keyword, categoryId, minPrice, maxPrice, sortBy, page, pageSize, schoolFilter);
    // 获取总数（使用相同的筛选条件）
    int total = m_db->getGoodsCount(keyword, categoryId, minPrice, maxPrice, schoolFilter);

    QJsonObject result;
    result["goods_list"] = goodsList;
    result["total"] = total;
    return {{"success", true}, {"data", result}};
}

// 获取商品详情
QJsonObject ApiHandler::handleGetGoodsDetail(int userId,const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};

    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) {
        return {{"success", false}, {"error", "商品不存在"}};
    }

    // 查询当前用户是否收藏了该商品
    bool isFavorited = m_db->isFavorited(userId, goodsId);
    goods["is_favorited"] = isFavorited;

    // 获取商品图片
    QJsonArray images = m_db->getGoodsImages(goodsId);
    goods["images"] = images;

    // 获取卖家信息
    int sellerId = goods.value("seller_id").toInt();
    QJsonObject seller = m_db->getUserById(sellerId);
    goods["seller_name"] = seller.value("nickname").toString();
    goods["seller_avatar"] = seller.value("avatar_url").toString();
    goods["seller_credit"] = seller.value("credit_score").toInt();
    goods["seller_phone"] = seller.value("phone").toString();

    // 获取该商品最新的 AI 估价记录
    QJsonObject aiRecord = m_db->getAIValuationByGoodsId(goodsId);
    if (!aiRecord.isEmpty()) {
        goods["ai_min_price"] = aiRecord.value("min_price");
        goods["ai_max_price"] = aiRecord.value("max_price");
        goods["ai_confidence"] = aiRecord.value("confidence");
        goods["ai_reason"] = aiRecord.value("reason");
        goods["ai_condition"] = aiRecord.value("condition_assessment");
        goods["ai_risk_level"] = aiRecord.value("risk_level");
        qDebug() << "Found AI valuation for goodsId:" << goodsId;
    } else {
        qDebug() << "No AI valuation for goodsId:" << goodsId;
    }

    return {{"success", true}, {"data", goods}};
}

// 创建订单
QJsonObject ApiHandler::handleCreateOrder(int userId, const QJsonObject &data)
{
    QJsonObject buyer = m_db->getUserById(userId);
    if (buyer.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }
    int creditScore = buyer.value("credit_score").toInt();
    if (creditScore < 60) {
        return {{"success", false}, {"error", "您的信用分低于60分，暂时无法购买商品。请保持良好的交易行为，提高信用分后再试。"}};
    }

    int goodsId = data.value("goods_id").toInt();
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};
    if (goods.value("status").toString().toInt() != 1) { // 1=已上架
        return {{"success", false}, {"error", "商品不可购买"}};
    }

    int sellerId = goods.value("seller_id").toInt();
    if (userId == sellerId) {
        return {{"success", false}, {"error", "不能购买自己发布的商品"}};
    }

    QJsonObject order;
    order["buyer_id"] = userId;
    order["seller_id"] = goods.value("seller_id").toInt();
    order["goods_id"] = goodsId;
    order["goods_title"] = goods.value("name").toString();
    order["goods_image"] = ""; // 需要从图片表中获取主图
    order["deal_price"] = goods.value("price").toDouble();
    order["status"] = 0; // 待支付

    if (m_db->addOrder(order)) {
        // 更新商品状态为交易中（2）
        m_db->updateGoodsStatus(goodsId, 2);
        return {{"success", true}, {"message", "订单创建成功"}};
    } else {
        return {{"success", false}, {"error", "订单创建失败"}};
    }
}

// 支付订单
QJsonObject ApiHandler::handlePayOrder(int userId, const QJsonObject &data)
{
    int orderId = data.value("order_id").toInt();
    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};
    if (order.value("buyer_id").toInt() != userId) {
        return {{"success", false}, {"error", "无权操作此订单"}};
    }
    if (order.value("status").toInt() != 0) {
        return {{"success", false}, {"error", "订单状态不正确"}};
    }

    // 模拟支付成功
    if (m_db->updateOrderStatus(orderId, 1)) { // 1=已支付
        return {{"success", true}, {"message", "支付成功"}};
    } else {
        return {{"success", false}, {"error", "支付失败"}};
    }
}

// 发送消息（HTTP方式）
QJsonObject ApiHandler::handleSendMessage(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    QString content = data.value("content").toString();
    int receiverId = data.value("receiver_id").toString().toInt();
    qDebug()<<data;

    // 1. 获取商品信息
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) {
        return {{"success", false}, {"error", "商品不存在"}};
    }
    int sellerId = goods.value("seller_id").toInt();

    // 2. 确定买家ID和卖家ID
    int buyerId, seller;
    if (userId == sellerId) {
        // 卖家给买家发消息，需要知道买家ID
        if (receiverId <= 0) {
            return {{"success", false}, {"error", "请指定接收者"}};
        }
        buyerId = receiverId;
        sellerId = userId;
    } else {
        // 买家给卖家发消息
        buyerId = userId;
        sellerId = sellerId;
    }

    // 3. 生成统一的 session_id（小ID_大ID_商品ID）
    int id1 = qMin(buyerId, sellerId);
    int id2 = qMax(buyerId, sellerId);
    QString sessionId = QString("%1_%2_%3").arg(id1).arg(id2).arg(goodsId);

    // 4. 构造消息对象
    QJsonObject message;
    message["session_id"] = sessionId;
    message["buyer_id"] = buyerId;
    message["seller_id"] = sellerId;
    message["goods_id"] = goodsId;
    message["sender_id"] = userId;
    message["message_type"] = 1;
    message["content"] = content;

    // 5. 保存到数据库
    if (!m_db->addChatMessage(message)) {
        return {{"success", false}, {"error", "发送失败"}};
    }

    // 6. 实时推送给接收者
    receiverId = (userId == buyerId) ? sellerId : buyerId;
    QJsonObject wsMsg;
    wsMsg["type"] = 1;
    wsMsg["data"] = message;
    m_ws->sendToUser(receiverId, wsMsg);

    return {{"success", true}, {"message", "发送成功"}};
}

// 处理WebSocket消息
void ApiHandler::handleWebSocketMessage(int senderId, const QJsonObject &message)
{
    int type = message.value("type").toInt();
    QJsonObject data = message.value("data").toObject();

    switch (type) {
    case 1: // 聊天消息
    {
        int goodsId = data.value("goods_id").toInt();
        QString content = data.value("content").toString();

        // 获取商品信息，确定卖家ID
        QJsonObject goods = m_db->getGoods(goodsId);
        if (goods.isEmpty()) return;

        int sellerId = goods.value("seller_id").toInt();
        int buyerId;
        if (senderId == sellerId) {
            // 卖家发送，需要从 data 中获取接收者（买家ID）
            buyerId = data.value("receiver_id").toInt();
            if (buyerId <= 0) return;
        } else {
            buyerId = senderId;
            sellerId = sellerId;
        }

        // 统一 session_id
        int id1 = qMin(buyerId, sellerId);
        int id2 = qMax(buyerId, sellerId);
        QString sessionId = QString("%1_%2_%3").arg(id1).arg(id2).arg(goodsId);

        QJsonObject chatMsg;
        chatMsg["session_id"] = sessionId;
        chatMsg["buyer_id"] = buyerId;
        chatMsg["seller_id"] = sellerId;
        chatMsg["goods_id"] = goodsId;
        chatMsg["sender_id"] = senderId;
        chatMsg["message_type"] = 1;
        chatMsg["content"] = content;

        m_db->addChatMessage(chatMsg);

        // 转发给接收者
        int receiverId = (senderId == buyerId) ? sellerId : buyerId;
        m_ws->sendToUser(receiverId, message);
        break;
    }
    case 99: // 心跳
        // 忽略
        break;
    default:
        qWarning() << "Unknown WebSocket message type:" << type;
    }
}

QJsonObject ApiHandler::handleUpdateUserProfile(int userId, const QJsonObject &data)
{
    QJsonObject updates;
    // 可更新的字段：nickname, phone, email, avatar_url
    if (data.contains("nickname")) updates["nickname"] = data["nickname"].toString();
    if (data.contains("phone")) updates["phone"] = data["phone"].toString();
    if (data.contains("email")) updates["email"] = data["email"].toString();
    if (data.contains("avatar_url")) updates["avatar_url"] = data["avatar_url"].toString();
    if (data.contains("school")) updates["school"] = data["school"].toString();

    if (updates.isEmpty()) {
        return {{"success", false}, {"error", "没有可更新的字段"}};
    }

    if (m_db->updateUser(userId, updates)) {
        return {{"success", true}, {"message", "资料更新成功"}};
    } else {
        return {{"success", false}, {"error", "资料更新失败"}};
    }
}

QJsonObject ApiHandler::handleUploadAvatar(int userId, const QJsonObject &data)
{
    QString avatarBase64 = data.value("avatar_base64").toString();
    if (avatarBase64.isEmpty()) {
        return {{"success", false}, {"error", "没有提供头像数据"}};
    }

    QByteArray imageData = QByteArray::fromBase64(avatarBase64.toUtf8());
    if (imageData.isEmpty()) {
        return {{"success", false}, {"error", "无效的图片数据"}};
    }

    // 生成唯一文件名
    QString suffix = "jpg";
    QString imageType = data.value("image_type").toString();
    if (imageType == "image/png") suffix = "png";
    else if (imageType == "image/jpeg") suffix = "jpg";
    else if (imageType == "image/gif") suffix = "gif";

    QString newFileName = QString("%1_%2.%3")
                              .arg(QDateTime::currentSecsSinceEpoch())
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
                              .arg(suffix);

    QString savePath = QDir::currentPath() + "/uploads/avatar/" + newFileName;
    QDir().mkpath(QFileInfo(savePath).path());

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {{"success", false}, {"error", "无法保存头像文件"}};
    }
    file.write(imageData);
    file.close();

    QString avatarUrl = QString("/uploads/avatar/%1").arg(newFileName);

    // 更新数据库中的 avatar_url
    QJsonObject updates;
    updates["avatar_url"] = avatarUrl;
    if (m_db->updateUser(userId, updates)) {
        QJsonObject dataObj;
        dataObj["avatar_url"] = avatarUrl;
        return {{"success", true}, {"data", dataObj}};
    } else {
        return {{"success", false}, {"error", "头像保存失败"}};
    }
}

QJsonObject ApiHandler::handleUpdateGoods(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};

    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};

    // 检查权限：只有卖家或管理员可以修改
    int sellerId = goods.value("seller_id").toInt();
    QString role;
    int uid = userId;
    authenticate("", uid, role); // 获取角色，这里需要先有 token，但我们已经通过请求传入 userId，角色从数据库获取
    // 由于我们已经有了 userId，可以查询用户角色
    QJsonObject user = m_db->getUserById(userId);
    if (user.isEmpty()) return {{"success", false}, {"error", "用户不存在"}};
    bool isAdmin = user.value("role").toInt() == 1;
    if (userId != sellerId && !isAdmin) {
        return {{"success", false}, {"error", "无权限修改此商品"}};
    }

    QJsonObject updates;
    if (data.contains("name")) updates["name"] = data["name"].toString();
    if (data.contains("price")) updates["price"] = data["price"].toDouble();
    if (data.contains("category_id")) updates["category_id"] = data["category_id"].toInt();
    if (data.contains("description")) updates["description"] = data["description"].toString();
    if (data.contains("status")) {
        int newStatus = data["status"].toInt();
        // 状态变更可能需要额外处理（如已售出不可修改等），这里简单允许
        updates["status"] = newStatus;
    }
    if (data.contains("images")) {
        int goodsId = data.value("goods_id").toInt();
        // 删除原有图片
        QSqlQuery query(m_db->getDb());
        query.prepare("DELETE FROM goods_image WHERE goods_id = ?");
        query.addBindValue(goodsId);
        query.exec();
        // 插入新图片
        QJsonArray images = data.value("images").toArray();
        for (int i = 0; i < images.size(); ++i) {
            QString imageUrl = images[i].toString();
            m_db->addGoodsImage(goodsId, imageUrl, i);
        }
    }

    if (updates.isEmpty()) {
        return {{"success", false}, {"error", "没有可更新的字段"}};
    }

    if (m_db->updateGoods(goodsId, updates)) {
        return {{"success", true}, {"message", "商品更新成功"}};
    } else {
        return {{"success", false}, {"error", "商品更新失败"}};
    }
}

QJsonObject ApiHandler::handleDeleteGoods(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};

    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};

    // 权限检查
    int sellerId = goods.value("seller_id").toInt();
    QJsonObject user = m_db->getUserById(userId);
    bool isAdmin = user.value("role").toInt() == 1;
    if (userId != sellerId && !isAdmin) {
        return {{"success", false}, {"error", "无权限删除此商品"}};
    }

    if (m_db->deleteGoods(goodsId)) {
        return {{"success", true}, {"message", "商品删除成功"}};
    } else {
        return {{"success", false}, {"error", "商品删除失败"}};
    }
}

QJsonObject ApiHandler::handleGetRecommendedGoods(const QJsonObject &data)
{
    int limit = data.value("limit").toInt(10);
    // 推荐策略：按浏览量、收藏量排序，取前 limit 条
    QJsonArray goodsList = m_db->getRecommendedGoods(limit);
    return {{"success", true}, {"data", goodsList}};
}

QJsonObject ApiHandler::handleConfirmOrder(int userId, const QJsonObject &data)
{
    int orderId = data.value("order_id").toInt();
    if (orderId <= 0) return {{"success", false}, {"error", "无效订单ID"}};

    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};

    // 只有买家可以确认收货
    if (order.value("buyer_id").toInt() != userId) {
        return {{"success", false}, {"error", "无权操作此订单"}};
    }
    if (order.value("status").toString().toInt() != 1) { // 1=已支付待收货
        return {{"success", false}, {"error", "订单状态不正确"}};
    }

    if (m_db->updateOrderStatus(orderId, 2)) { // 2=已完成
        // 更新商品状态为已售出（3）
        int goodsId = order.value("goods_id").toInt();
        m_db->updateGoodsStatus(goodsId, 3);
        return {{"success", true}, {"message", "确认收货成功"}};
    } else {
        return {{"success", false}, {"error", "确认收货失败"}};
    }
}

QJsonObject ApiHandler::handleCancelOrder(int userId, const QJsonObject &data)
{
    int orderId = data.value("order_id").toInt();
    QString reason = data.value("reason").toString();

    if (orderId <= 0) return {{"success", false}, {"error", "无效订单ID"}};

    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};

    // 买家或卖家可以取消订单？通常只有买家在待支付时可以取消
    if (order.value("buyer_id").toInt() != userId && order.value("seller_id").toInt() != userId) {
        return {{"success", false}, {"error", "无权操作此订单"}};
    }
    if (order.value("status").toString().toInt() != 0&& order.value("status").toString().toInt() != 1) { // 0=待支付
        return {{"success", false}, {"error", "订单状态不正确，无法取消"}};
    }

    if (order.value("status").toString().toInt() == 1) { // 已支付待收货状态退款
        QJsonObject user = m_db->getUserById(userId);
        QString currentMonth = QDate::currentDate().toString("yyyy-MM");
        QString userMonth = user.value("refund_month").toString();
        int refundCount = user.value("refund_count_month").toInt();

        if (userMonth != currentMonth) {
            // 新月份，重置计数
            refundCount = 0;
            m_db->updateUser(userId, {{"refund_month", currentMonth}, {"refund_count_month", 0}});
        }
        if (refundCount >= 3) { // 每月最多3次
            return {{"success", false}, {"error", "本月退款次数已达上限（3次），无法退款"}};
        }
        // 增加计数
        m_db->updateUser(userId, {{"refund_count_month", refundCount + 1}});
    }

    if (m_db->updateOrderStatus(orderId, 3, reason)) { // 3=已取消
        // 恢复商品状态为上架（1）
        int goodsId = order.value("goods_id").toInt();
        m_db->updateGoodsStatus(goodsId, 1);
        return {{"success", true}, {"message", "订单已取消"}};
    } else {
        return {{"success", false}, {"error", "取消订单失败"}};
    }
}

QJsonObject ApiHandler::handleGetOrderList(int userId, const QJsonObject &data)
{
    QString status = data.value("status").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QString keyword = data.value("keyword").toString();

    int statusInt = -1;
    if (status == "pending_payment") statusInt = 0;
    else if (status == "paid") statusInt = 1;
    else if (status == "completed") statusInt = 2;
    else if (status == "cancelled") statusInt = 3;
    else if (status == "dispute") statusInt = 4;

    QJsonArray orders = m_db->getOrdersByUser(userId, statusInt, keyword, page, pageSize);

    // 转换为客户端期望的格式
    QJsonArray convertedOrders;
    for (const QJsonValue &val : orders) {
        QJsonObject orig = val.toObject();
        QJsonObject newOrder;

        // 字段映射
        newOrder["order_id"] = orig.value("id").toInt();
        newOrder["goods_title"] = orig.value("goods_title").toString();
        newOrder["deal_price"] = orig.value("deal_price").toDouble();
        newOrder["create_time"] = orig.value("create_time").toString();

        // 状态转换：int -> 中文字符串
        int statusVal = orig.value("status").toString().toInt();
        QString statusStr;
        if (statusVal == 0) statusStr = "待付款";
        else if (statusVal == 1) statusStr = "待收货";
        else if (statusVal == 2) statusStr = "已完成";
        else if (statusVal == 3) statusStr = "已取消";
        else if (statusVal == 4) statusStr = "纠纷处理中";
        else statusStr = "未知";
        newOrder["status"] = statusStr;

        // 获取卖家昵称
        int sellerId = orig.value("seller_id").toInt();
        QJsonObject seller = m_db->getUserById(sellerId);
        newOrder["seller_name"] = seller.value("nickname").toString();

        convertedOrders.append(newOrder);
    }

    return {{"success", true}, {"data", QJsonObject{{"orders", convertedOrders}}}};
}

QJsonObject ApiHandler::handleGetOrderDetail(int userId, const QJsonObject &data)
{
    int orderId = data.value("order_id").toInt();
    if (orderId <= 0) return {{"success", false}, {"error", "无效订单ID"}};

    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};

    // 只允许买家或卖家查看
    if (order.value("buyer_id").toInt() != userId && order.value("seller_id").toInt() != userId) {
        return {{"success", false}, {"error", "无权查看此订单"}};
    }

    // 可以附加商品信息、卖家/买家信息等
    int goodsId = order.value("goods_id").toInt();
    QJsonObject goods = m_db->getGoods(goodsId);
    order["goods"] = goods;
    int buyerId = order.value("buyer_id").toInt();
    QJsonObject buyer = m_db->getUserById(buyerId);
    order["buyer_name"] = buyer.value("nickname").toString();
    int sellerId = order.value("seller_id").toInt();
    QJsonObject seller = m_db->getUserById(sellerId);
    order["seller_name"] = seller.value("nickname").toString();

    return {{"success", true}, {"data", order}};
}

QJsonObject ApiHandler::handleGetMessageHistory(int userId, const QJsonObject &data)
{
    QString chatId = data.value("chat_id").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(50);
    if (chatId.isEmpty()) return {{"success", false}, {"error", "缺少 chat_id"}};

    // 验证用户是否属于该聊天会话（可选）
    QJsonArray messages = m_db->getChatMessages(chatId, pageSize, (page-1)*pageSize);
    qDebug() << "getMessageHistory: chatId=" << chatId << ", messages count=" << messages.size();
    // 标记已读
    // m_db->markChatAsRead(chatId, userId);
    return {{"success", true}, {"data", QJsonObject{{"messages", messages}}}};
}

QJsonObject ApiHandler::handleGetChatList(int userId)
{
    // 获取用户参与的所有会话，并返回最近一条消息等
    // 实现方式：查询 chat 表中所有 session_id，去重，并按最新消息时间排序
    // 由于 chat 表没有直接维护会话列表，需要复杂查询，这里简化：返回所有不同的 session_id 及最近消息
    QJsonArray chatList = m_db->getChatList(userId);
    return {{"success", true}, {"data", QJsonObject{{"chats", chatList}}}};
}

QJsonObject ApiHandler::handleMarkMessageRead(int userId, const QJsonObject &data)
{
    QString sessionId = data.value("session_id").toString();
    if (sessionId.isEmpty()) {
        return {{"success", false}, {"error", "缺少 session_id"}};
    }

    if (m_db->markChatAsRead(sessionId, userId)) {
        return {{"success", true}, {"message", "已标记为已读"}};
    } else {
        return {{"success", false}, {"error", "标记失败"}};
    }
}

QJsonObject ApiHandler::handleSubmitReview(int userId, const QJsonObject &data)
{
    QJsonObject user = m_db->getUserById(userId);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }
    int creditScore = user.value("credit_score").toInt();
    if (creditScore < 60) {
        return {{"success", false}, {"error", "您的信用分低于60分，暂时无法进行评价。请保持良好的交易行为，提高信用分后再试。"}};
    }

    int orderId = data.value("order_id").toInt();
    int score = data.value("score").toInt();
    QString content = data.value("comment").toString();
    QStringList images;
    if (data.contains("images")) {
        QJsonValue imagesVal = data.value("images");
        if (imagesVal.isArray()) {
            QJsonArray arr = imagesVal.toArray();
            for (const QJsonValue &v : arr) {
                images << v.toString();
            }
        } else if (imagesVal.isString()) {
            images = imagesVal.toString().split(',', Qt::SkipEmptyParts);
        }
    }
    bool isAnonymous = data.value("is_anonymous").toBool(false);

    if (orderId <= 0 || score < 1 || score > 5) {
        return {{"success", false}, {"error", "参数错误"}};
    }

    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};
    if (order.value("buyer_id").toInt() != userId) {
        return {{"success", false}, {"error", "只有买家可以评价"}};
    }
    if (order.value("status").toString().toInt() != 2) { // 2=已完成
        return {{"success", false}, {"error", "订单未完成，不能评价"}};
    }

    // 检查是否已经评价过
    // 可以通过 evaluation 表查询，此处简化：假设可以重复提交会报唯一键冲突，由数据库保证

    QJsonObject eval;
    eval["order_id"] = orderId;
    eval["buyer_id"] = userId;
    eval["seller_id"] = order.value("seller_id").toInt();
    eval["score"] = score;
    eval["content"] = content;
    eval["image_urls"] = images.join(",");
    eval["is_anonymous"] = isAnonymous ? 1 : 0;

    if (m_db->addEvaluation(eval)) {
        // 更新卖家信用分：根据评分调整
        int delta = (score >= 4) ? 2 : (score == 3 ? 0 : -1); // 简单示例
        if (delta != 0) {
            m_db->updateUserCreditScore(order.value("seller_id").toInt(), delta,
                                        QString("收到%1星评价").arg(score));
        }
        return {{"success", true}, {"message", "评价成功"}};
    } else {
        return {{"success", false}, {"error", "评价失败，可能已评价过"}};
    }
}

QJsonObject ApiHandler::handleGetGoodsReviews(const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(10);
    QJsonArray reviews = m_db->getEvaluationsByGoods(goodsId, page, pageSize);
    return {{"success", true}, {"data", reviews}};
}

QJsonObject ApiHandler::handleGetSellerReviews(const QJsonObject &data)
{
    int sellerId = data.value("seller_id").toString().toInt();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(10);
    if (sellerId <= 0) return {{"success", false}, {"error", "无效卖家ID"}};
    QJsonArray reviews = m_db->getEvaluationsByUser(sellerId, true, page, pageSize);
    return {{"success", true}, {"data", reviews}};
}

QJsonObject ApiHandler::handleRemoveFavorite(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};
    if (m_db->removeFavorite(userId, goodsId)) {
        return {{"success", true}, {"message", "取消收藏成功"}};
    } else {
        return {{"success", false}, {"error", "取消收藏失败"}};
    }
}

QJsonObject ApiHandler::handleGetFavorites(int userId, const QJsonObject &data)
{
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray favorites = m_db->getFavorites(userId, page, pageSize);
    return {{"success", true}, {"data", QJsonObject{{"list", favorites}}}};
}

QJsonObject ApiHandler::handleGetCreditScore(int userId, const QJsonObject &data)
{
    QString targetUserId = data.value("user_id").toString();
    int id = targetUserId.isEmpty() ? userId : targetUserId.toInt();
    QJsonObject user = m_db->getUserById(id);
    if (user.isEmpty()) return {{"success", false}, {"error", "用户不存在"}};
    QJsonObject dataObj;
    dataObj["credit_score"] = user.value("credit_score").toInt();
    dataObj["last_update_time"] = m_db->getCreditScoreLastUpdateTime(id);
    return {{"success", true}, {"data", dataObj}};
}

QJsonObject ApiHandler::handleGetCreditHistory(int userId, const QJsonObject &data)
{
    QString targetUserId = data.value("user_id").toString();
    int id = targetUserId.isEmpty() ? userId : targetUserId.toInt();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray history = m_db->getCreditHistory(id, page, pageSize);
    return {{"success", true}, {"data", QJsonObject{{"history", history}}}};
}

QJsonObject ApiHandler::handleSubmitReport(int userId, const QJsonObject &data)
{
    int targetId = data.value("target_id").toInt();
    int targetType = data.value("target_type").toInt(); // 1=商品，2=用户，3=评价
    int reasonType = data.value("reason_type").toInt();
    QString description = data.value("description").toString();
    QString evidence = data.value("evidence_urls").toString();

    if (targetId <= 0 || targetType <= 0 || reasonType <= 0 || description.isEmpty()) {
        return {{"success", false}, {"error", "参数不完整"}};
    }

    QJsonObject report;
    report["reporter_id"] = userId;
    report["reported_type"] = targetType;
    report["reported_id"] = targetId;
    report["reason_type"] = reasonType;
    report["description"] = description;
    report["evidence_urls"] = evidence;
    if (m_db->addReport(report)) {
        return {{"success", true}, {"message", "举报提交成功"}};
    } else {
        return {{"success", false}, {"error", "举报提交失败"}};
    }
}

QJsonObject ApiHandler::handleGetMyReports(int userId, const QJsonObject &data)
{
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    // 获取当前用户提交的举报
    QJsonArray reports = m_db->getMyReports(userId,page,pageSize);
    return {{"success", true}, {"data", reports}};
}

QJsonObject ApiHandler::handleSubmitDispute(int userId, const QJsonObject &data)
{
    QJsonObject user = m_db->getUserById(userId);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }
    int creditScore = user.value("credit_score").toInt();
    if (creditScore < 60) {
        return {{"success", false}, {"error", "您的信用分低于60分，暂时无法发起纠纷。请保持良好的交易行为，提高信用分后再试。"}};
    }

    int orderId = data.value("order_id").toInt();
    QString type = data.value("dispute_type").toString();
    QString description = data.value("description").toString();
    QStringList evidences;
    if (data.contains("evidence_urls")) {
        QJsonValue evidenceVal = data.value("evidence_urls");
        if (evidenceVal.isArray()) {
            QJsonArray arr = evidenceVal.toArray();
            for (const QJsonValue &v : arr) {
                evidences << v.toString();
            }
        } else if (evidenceVal.isString()) {
            evidences = evidenceVal.toString().split(',', Qt::SkipEmptyParts);
        }
    }

    if (orderId <= 0 || type.isEmpty() || description.isEmpty()) {
        return {{"success", false}, {"error", "参数不完整"}};
    }

    QJsonObject order = m_db->getOrder(orderId);
    if (order.isEmpty()) return {{"success", false}, {"error", "订单不存在"}};
    // 只有订单中的买家或卖家可以提交纠纷
    if (order.value("buyer_id").toInt() != userId && order.value("seller_id").toInt() != userId) {
        return {{"success", false}, {"error", "无权提交此订单纠纷"}};
    }

    // 检查订单状态是否允许纠纷（通常已支付或已完成？这里允许）
    int complainantId = userId;
    int defendantId = (order.value("buyer_id").toInt() == userId) ? order.value("seller_id").toInt() : order.value("buyer_id").toInt();

    QJsonObject dispute;
    dispute["order_id"] = orderId;
    dispute["complainant_id"] = complainantId;
    dispute["defendant_id"] = defendantId;
    dispute["type"] = type;
    dispute["description"] = description;
    dispute["evidence_urls"] = evidences.join(",");
    if (m_db->addDispute(dispute)) {
        // 更新订单状态为售后中（4）
        m_db->updateOrderStatus(orderId, 4);
        return {{"success", true}, {"message", "纠纷提交成功"}};
    } else {
        return {{"success", false}, {"error", "纠纷提交失败"}};
    }
}

QJsonObject ApiHandler::handleGetDisputeDetail(int userId, const QJsonObject &data)
{
    int disputeId = data.value("dispute_id").toInt();
    if (disputeId <= 0) return {{"success", false}, {"error", "无效纠纷ID"}};
    QJsonObject dispute = m_db->getDispute(disputeId);
    if (dispute.isEmpty()) return {{"success", false}, {"error", "纠纷不存在"}};
    // 只有涉及的用户或管理员可以查看
    if (dispute.value("complainant_id").toInt() != userId && dispute.value("defendant_id").toInt() != userId) {
        QJsonObject user = m_db->getUserById(userId);
        if (user.value("role").toString().toInt() != 1) {
            return {{"success", false}, {"error", "无权查看"}};
        }
    }
    return {{"success", true}, {"data", dispute}};
}

QJsonObject ApiHandler::handleGetMyDisputes(int userId, const QJsonObject &data)
{
    QString status = data.value("status").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    int statusInt = -1;
    if (status == "pending") statusInt = 0;
    else if (status == "processing") statusInt = 1;
    else if (status == "resolved") statusInt = 2;
    else if (status == "closed") statusInt = 3;
    // 获取用户作为投诉方或被投诉方的纠纷
    QJsonArray disputes = m_db->getMyDisputes(userId,statusInt,page,pageSize);
    return {{"success", true}, {"data", disputes}};
}

// apihandler.cpp 实现
QJsonObject ApiHandler::handleGetDisputeByOrder(int userId, const QJsonObject &data)
{
    int orderId = data.value("order_id").toInt();
    if (orderId <= 0) {
        return {{"success", false}, {"error", "无效订单ID"}};
    }

    // 查询该订单对应的纠纷（可能有多个，取第一个）
    QJsonArray disputes = m_db->getDisputesByOrder(orderId);
    if (disputes.isEmpty()) {
        return {{"success", false}, {"error", "未找到相关纠纷"}};
    }
    QJsonObject dispute = disputes[0].toObject();

    // 检查权限：只有订单相关方或管理员可查看
    int buyerId = dispute.value("complainant_id").toInt();  // 假设投诉方是买家
    int sellerId = dispute.value("defendant_id").toInt();
    QJsonObject user = m_db->getUserById(userId);
    bool isAdmin = user.value("role").toInt() == 1;
    if (userId != buyerId && userId != sellerId && !isAdmin) {
        return {{"success", false}, {"error", "无权查看"}};
    }

    return {{"success", true}, {"data", dispute}};
}

QJsonObject ApiHandler::handleGetPendingGoods(int userId, const QJsonObject &data)
{
    // 权限检查
    QJsonObject user = m_db->getUserById(userId);
    if (user.value("role").toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    QJsonArray goodsList = m_db->getPendingGoods(page, pageSize);
    int total = m_db->getPendingGoodsCount();

    QJsonArray newList;
    for (const QJsonValue &val : goodsList) {
        QJsonObject goods = val.toObject();
        QJsonObject newGoods;
        newGoods["goods_id"] = goods.value("id").toInt();
        newGoods["name"] = goods.value("name").toString();
        newGoods["price"] = goods.value("price").toDouble();
        newGoods["publish_time"] = goods.value("publish_time").toString();
        int statusInt = goods.value("status").toInt();
        QString statusStr;
        if (statusInt == 0) statusStr = "待审核";
        else if (statusInt == 1) statusStr = "已上架";
        else if (statusInt == 4) statusStr = "已拒绝";
        else statusStr = "未知";
        newGoods["status"] = statusStr;
        int sellerId = goods.value("seller_id").toInt();
        QJsonObject seller = m_db->getUserById(sellerId);
        newGoods["seller_name"] = seller.value("nickname").toString();
        newList.append(newGoods);
    }

    QJsonObject resultData;
    resultData["list"] = newList;
    resultData["total"] = total;
    return {{"success", true}, {"data", resultData}};
}

QJsonObject ApiHandler::handleReviewGoods(int userId, const QJsonObject &data)
{
    QJsonObject user = m_db->getUserById(userId);
    if (user.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int goodsId = data.value("goods_id").toInt();
    bool approved = data.value("approved").toBool();
    QString comment = data.value("comment").toString();

    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};
    int newStatus = approved ? 1 : 4; // 1=已上架，4=已下架（审核不通过）
    if (m_db->updateGoodsStatus(goodsId, newStatus)) {
        // 如果通过，可以发送系统消息通知卖家
        QJsonObject goods = m_db->getGoods(goodsId);
        int sellerId = goods.value("seller_id").toInt();
        QString title = approved ? "商品审核通过" : "商品审核未通过";
        QString content = approved ? QString("您的商品【%1】已通过审核，已上架销售。")
                                         .arg(goods.value("name").toString())
                                   : QString("您的商品【%1】审核未通过，原因：%2")
                                         .arg(goods.value("name").toString()).arg(comment);
        m_db->addSystemMessage(sellerId, 2, title, content, goodsId);
        return {{"success", true}, {"message", approved ? "审核通过" : "审核拒绝"}};
    } else {
        return {{"success", false}, {"error", "审核操作失败"}};
    }
}

QJsonObject ApiHandler::handleGetUserList(int userId, const QJsonObject &data)
{
    QJsonObject user = m_db->getUserById(userId);
    if (user.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    QString role = data.value("role").toString();
    QString status = data.value("status").toString();
    QString keyword = data.value("keyword").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    QJsonArray users = m_db->getUserList(role, status, keyword, page, pageSize);
    int total = m_db->getUserCount(role, status, keyword);

    QJsonObject result;
    result["list"] = users;
    result["total"] = total;
    return {{"success", true}, {"data", result}};
}

QJsonObject ApiHandler::handleUpdateUserStatus(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int targetUserId = data.value("user_id").toString().toInt();
    int newStatus = data.value("status").toString().toInt(); // 0=禁用，1=正常
    QString reason = data.value("reason").toString();

    if (targetUserId <= 0) return {{"success", false}, {"error", "无效用户ID"}};
    if (m_db->updateUserStatus(targetUserId, newStatus, reason)) {
        return {{"success", true}, {"message", "用户状态更新成功"}};
    } else {
        return {{"success", false}, {"error", "更新失败"}};
    }
}

QJsonObject ApiHandler::handleGetDisputeList(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    QString statusStr = data.value("status").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    int status = -1;
    if (statusStr == "0") status = 0;
    else if (statusStr == "1") status = 1;
    else if (statusStr == "2") status = 2;
    else if (statusStr == "3") status = 3;
    QJsonArray disputes = m_db->getDisputes(status, page, pageSize);
    int total = m_db->getDisputeCount(statusStr);

    QJsonObject resultData;
    resultData["list"] = disputes;
    resultData["total"] = total;
    return {{"success", true}, {"data", resultData}};
}

QJsonObject ApiHandler::handleProcessDispute(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int disputeId = data.value("dispute_id").toInt();
    QString result = data.value("result").toString(); // 处理结果说明
    QString responsibility = data.value("responsibility").toString(); // "seller" 或 "buyer"
    int changeValue = data.value("change_value").toInt(); // 扣分绝对值（卖家10~20，买家5~10）

    if (disputeId <= 0) return {{"success", false}, {"error", "无效纠纷ID"}};
    QJsonObject dispute = m_db->getDispute(disputeId);
    if (dispute.isEmpty()) return {{"success", false}, {"error", "纠纷不存在"}};

    // 确定扣分对象
    int targetUserId = -1;
    QString ruleCode;
    if (responsibility == "seller") {
        targetUserId = dispute.value("defendant_id").toInt();
        ruleCode = "DISPUTE_SELLER";
    } else if (responsibility == "buyer") {
        targetUserId = dispute.value("complainant_id").toInt();
        ruleCode = "DISPUTE_BUYER";
    }

    if (targetUserId > 0 && changeValue > 0) {
        // 扣分（传入负数）
        m_db->applyCreditChange(targetUserId, ruleCode, disputeId, userId, -changeValue);
    }

    if (m_db->updateDispute(disputeId, 2, result, userId)) {
        // 更新关联订单状态（如退款等），根据 result 可能还需要调整信用分
        int orderId = dispute.value("order_id").toInt();
        QJsonObject order = m_db->getOrder(orderId);
        int newOrderStatus;
        if (responsibility == "seller") {
            newOrderStatus = 3;  // 已取消（退款）
            // 恢复商品状态为上架
            m_db->updateGoodsStatus(order.value("goods_id").toInt(), 1);
        } else { // 买家责任
            newOrderStatus = 2;  // 已完成（交易成功）
            // 商品状态保持已售出（3），无需改变
        }
        m_db->updateOrderStatus(orderId, newOrderStatus, result);
        // 发送系统消息给双方
        m_db->addSystemMessage(dispute.value("complainant_id").toInt(), 3, "纠纷处理结果",
                               QString("您提交的纠纷已处理，结果：%1").arg(result), disputeId);
        m_db->addSystemMessage(dispute.value("defendant_id").toInt(), 3, "纠纷处理结果",
                               QString("涉及您的纠纷已处理，结果：%1").arg(result), disputeId);
        return {{"success", true}, {"message", "纠纷处理完成"}};
    } else {
        return {{"success", false}, {"error", "处理失败"}};
    }
}

QJsonObject ApiHandler::handleGetStatistics(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    QString period = data.value("period").toString("daily");
    // 这里根据 period 返回统计数据，简化：返回一些基本计数
    QJsonObject stats;
    QSqlQuery query(m_db->getDb());
    query.exec("SELECT COUNT(*) FROM user WHERE role=0");
    if (query.next()) stats["total_users"] = query.value(0).toInt();
    query.exec("SELECT COUNT(*) FROM goods");
    if (query.next()) stats["total_goods"] = query.value(0).toInt();
    query.exec("SELECT COUNT(*) FROM `order`");
    if (query.next()) stats["total_orders"] = query.value(0).toInt();
    query.exec("SELECT COUNT(*) FROM `order` WHERE status=0");
    if (query.next()) stats["pending_payment"] = query.value(0).toInt();
    query.exec("SELECT COUNT(*) FROM dispute WHERE status=0");
    if (query.next()) stats["pending_disputes"] = query.value(0).toInt();
    return {{"success", true}, {"data", stats}};
}

QJsonObject ApiHandler::handleEstimatePrice(int userId, const QJsonObject &data)
{
    QString description = data.value("description").toString();
    int goodsId = data.value("goods_id").toInt();
    qDebug() << "handleEstimatePrice: goodsId =" << goodsId;
    QJsonArray images = data.value("images").toArray();  // 支持客户端传base64图片

    if (description.isEmpty() && images.isEmpty()) {
        return {{"success", false}, {"error", "请提供商品描述或图片"}};
    }

    // 构建给AI的提示词
    QString prompt = QString(
                         "你是一个专业的二手商品估价助手。请根据以下商品信息进行估价：\n"
                         "商品描述：%1\n"
                         "请以 JSON 格式返回以下信息：\n"
                         "{\n"
                         "  \"min_price\": 最低估价（元），\n"
                         "  \"max_price\": 最高估价（元），\n"
                         "  \"confidence\": 置信度（0-100的整数），\n"
                         "  \"reason\": \"估价理由（详细说明为什么给出这个价格，考虑因素包括品牌、成色、市场行情等）\",\n"
                         "  \"condition_assessment\": \"成色评估（如：全新/9成新/8成新等）\",\n"
                         "  \"risk_level\": \"风险等级（低/中/高）\"\n"
                         "}\n\n"
                         "只返回 JSON，不要有其他内容。"
                         ).arg(description);

    QString resultText;
    bool apiSuccess = false;

    // 调用混元API
    HunyuanClient::instance()->analyzeImagesAndText(images, prompt,
                                                   [&](bool success, const QString& content) {
                                                       apiSuccess = success;
                                                       resultText = content;
                                                   });

    if (!apiSuccess) {
        return {{"success", false}, {"data", QJsonObject{{"min_price", -1}, {"reason", "error"}}}};
    }

    // 解析AI返回的JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(resultText.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return {{"success", false}, {"data", QJsonObject{{"min_price", -1}, {"reason", "AI 返回格式错误"}}}};
    }

    QJsonObject aiResult = doc.object();
    // 提取估价信息
    double minPrice = aiResult.value("min_price").toDouble();
    double maxPrice = aiResult.value("max_price").toDouble();
    double confidence = aiResult.value("confidence").toDouble();
    QString reason = aiResult.value("reason").toString();
    QString conditionAssessment = aiResult.value("condition_assessment").toString();
    QString riskLevel = aiResult.value("risk_level").toString();

    // 保存估价记录到数据库
    if (goodsId > 0) {
        QJsonObject valuation;
        valuation["goods_id"] = goodsId;
        valuation["user_id"] = userId;
        valuation["description"] = description;
        valuation["image_url"] = "";  // 如果有图片 URL 可以保存
        valuation["min_price"] = minPrice;
        valuation["max_price"] = maxPrice;
        valuation["confidence"] = confidence;
        valuation["condition_assessment"] = conditionAssessment;
        valuation["risk_level"] = riskLevel;
        valuation["reason"] = reason;
        m_db->addAIValuation(valuation);
    }

    QJsonObject dataObj;
    dataObj["min_price"] = minPrice;
    dataObj["max_price"] = maxPrice;
    dataObj["confidence"] = confidence;
    dataObj["reason"] = reason;
    dataObj["condition_assessment"] = conditionAssessment;
    dataObj["risk_level"] = riskLevel;

    return {{"success", true}, {"data", dataObj}};
}

QJsonObject ApiHandler::handleRefreshToken(int userId)
{
    QString newToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_tokenToUserId[newToken] = userId;
    QJsonObject data;
    data["token"] = newToken;
    return {{"success", true}, {"data", data}};
}

QJsonObject ApiHandler::handleAddFavorite(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) {
        return {{"success", false}, {"error", "无效商品ID"}};
    }
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) {
        return {{"success", false}, {"error", "商品不存在"}};
    }
    if (m_db->addFavorite(userId, goodsId)) {
        return {{"success", true}, {"message", "收藏成功"}};
    } else {
        return {{"success", false}, {"error", "收藏失败，可能已收藏"}};
    }
}

bool ApiHandler::authenticate(const QJsonObject &request, int &userId, QString &role)
{
    QString token = request.value("token").toString();
    return authenticate(token, userId, role);
}

QJsonObject ApiHandler::handleUploadImage(int userId, const QJsonObject &data)
{
    // 从请求中获取图片数据（假设字段名为 "image_base64"）
    QString imageBase64 = data.value("image_base64").toString();
    if (imageBase64.isEmpty()) {
        return {{"success", false}, {"error", "没有提供图片数据"}};
    }

    // 可选的图片类型（如 "image/png"），用于确定扩展名
    QString imageType = data.value("image_type").toString();

    // 解码 Base64
    QByteArray imageData = QByteArray::fromBase64(imageBase64.toUtf8());
    if (imageData.isEmpty()) {
        return {{"success", false}, {"error", "无效的图片数据"}};
    }

    // 生成唯一文件名
    QString suffix = "jpg"; // 默认
    if (imageType == "image/png") suffix = "png";
    else if (imageType == "image/jpeg") suffix = "jpg";
    else if (imageType == "image/gif") suffix = "gif";

    QString newFileName = QString("%1_%2.%3")
                              .arg(QDateTime::currentSecsSinceEpoch())
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
                              .arg(suffix);

    // 保存到服务器目录，例如 uploads/images/
    QString savePath = QDir::currentPath() + "/uploads/images/" + newFileName;
    QDir().mkpath(QFileInfo(savePath).path()); // 确保目录存在

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {{"success", false}, {"error", "无法保存文件"}};
    }
    file.write(imageData);
    file.close();

    // 返回可访问的 URL
    QString fileUrl = QString("/uploads/images/%1").arg(newFileName);
    QJsonObject dataObj;
    dataObj["file_url"] = fileUrl;
    return {{"success", true}, {"data", dataObj}};
}

QJsonObject ApiHandler::handleGetAllReports(int userId, const QJsonObject &data)
{
    // 检查管理员权限
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }

    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QString status = data.value("status").toString("");

    QJsonArray reports = m_db->getReports(status, page, pageSize);
    int total = m_db->getReportCount(status);

    QJsonObject resultData;
    resultData["list"] = reports;
    resultData["total"] = total;
    return {{"success", true}, {"data", resultData}};
}

QJsonObject ApiHandler::handleProcessReport(int userId, const QJsonObject &data)
{
    // 权限检查
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }

    int reportId = data.value("report_id").toInt();
    bool isTrue = data.value("is_true").toBool();   // 是否属实
    QString result = data.value("result").toString();
    int newStatus = isTrue ? 1 : 2; // 1=已处理(属实)，2=已驳回(不实)

    QJsonObject report = m_db->getReportById(reportId);
    if (report.isEmpty()) return {{"success", false}, {"error", "举报不存在"}};

    int reporterId = report.value("reporter_id").toInt();
    int reportedType = report.value("reported_type").toString().toInt();
    int reportedId = report.value("reported_id").toInt();

    // 如果属实，进行信用分变更
    if (isTrue) {
        // 被举报人扣分
        int targetUserId = -1;
        QString targetRuleCode;
        if (reportedType == 1) { // 举报商品
            QJsonObject goods = m_db->getGoods(reportedId);
            if (!goods.isEmpty()) {
                targetUserId = goods.value("seller_id").toInt();
                targetRuleCode = "REPORT_GOODS";
            }
        } else if (reportedType == 2) { // 举报用户
            targetUserId = reportedId;
            targetRuleCode = "REPORT_USER";
        }
        if (targetUserId > 0 && !targetRuleCode.isEmpty()) {
            // 扣分（change_value 已在规则中定义，这里直接调用）
            m_db->applyCreditChange(targetUserId, targetRuleCode, reportId, userId);
        }

        // 举报人加分
        if (reporterId > 0) {
            m_db->applyCreditChange(reporterId, "REPORT_VALID", reportId, userId);
        }
    }

    if (m_db->updateReportStatus(reportId, newStatus, result, userId)) {
        m_db->addSystemMessage(reporterId, 3, "举报处理结果",
                               QString("您举报的内容已处理，结果：%1").arg(result), reportId);
        return {{"success", true}, {"message", "处理成功"}};
    }
    return {{"success", false}, {"error", "处理失败"}};
}

QJsonObject ApiHandler::handleUpdateCreditScore(int userId, const QJsonObject &data)
{
    // 管理员权限检查
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }

    int targetUserId = data.value("user_id").toInt();
    int newScore = data.value("credit_score").toInt();
    QString reason = data.value("reason").toString();

    if (targetUserId <= 0 || newScore < 0 || newScore > 100) {
        return {{"success", false}, {"error", "参数无效"}};
    }

    QJsonObject targetUser = m_db->getUserById(targetUserId);
    if (targetUser.isEmpty()) {
        return {{"success", false}, {"error", "用户不存在"}};
    }

    int oldScore = targetUser.value("credit_score").toInt();
    int delta = newScore - oldScore;

    if (m_db->updateUserCreditScore(targetUserId, delta, reason)) {
        return {{"success", true}, {"message", "信用分已更新"}};
    }
    return {{"success", false}, {"error", "更新失败"}};
}

QJsonObject ApiHandler::handleUpdateGoodsStatus(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    int newStatus = data.value("status").toInt();
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};
    // 权限检查：只有卖家或管理员可以修改状态
    if (goods.value("seller_id").toInt() != userId) {
        QJsonObject admin = m_db->getUserById(userId);
        if (admin.value("role").toString().toInt() != 1)
            return {{"success", false}, {"error", "无权限"}};
    }
    if (m_db->updateGoodsStatus(goodsId, newStatus))
        return {{"success", true}, {"message", "状态更新成功"}};
    else
        return {{"success", false}, {"error", "更新失败"}};
}

QJsonObject ApiHandler::handleGetMyGoods(int userId, const QJsonObject &data)
{
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray goodsList = m_db->getGoodsBySeller(userId, -1, page, pageSize);
    return {{"success", true}, {"data", goodsList}};
}

QJsonObject ApiHandler::handleGetMyReviews(int userId, const QJsonObject &data)
{
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    // 只查询 buyer_id = userId 的评价（即用户作为买家给出的评价）
    QJsonArray reviews = m_db->getMyReviews(userId,page,pageSize);
    return {{"success", true}, {"data", reviews}};
}

QJsonObject ApiHandler::handleGetBrowseHistory(int userId, const QJsonObject &data)
{
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray history = m_db->getBrowseHistory(userId, page, pageSize);
    return {{"success", true}, {"data", history}};
}

QJsonObject ApiHandler::handleAddBrowseHistory(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};
    if (m_db->addOrUpdateBrowseHistory(userId, goodsId))
        return {{"success", true}, {"message", "记录成功"}};
    else
        return {{"success", false}, {"error", "记录失败"}};
}

QJsonObject ApiHandler::handleChangePassword(int userId, const QJsonObject &data)
{
    QString oldPasswordMd5 = data.value("old_password").toString(); // 客户端传的MD5
    QString newPasswordMd5 = data.value("new_password").toString();

    QJsonObject user = m_db->getUserById(userId);
    if (user.isEmpty()) return {{"success", false}, {"error", "用户不存在"}};

    QString salt = user.value("salt").toString();
    // 计算存储的密码哈希：MD5(客户端MD5(明文) + salt)
    QString oldHash = QCryptographicHash::hash((oldPasswordMd5 + salt).toUtf8(), QCryptographicHash::Md5).toHex();
    if (oldHash != user.value("password").toString()) {
        qDebug() << "原密码错误: oldPasswordMd5=" << oldPasswordMd5 << "salt=" << salt << "oldHash=" << oldHash << "dbHash=" << user.value("password").toString();
        return {{"success", false}, {"error", "原密码错误"}};
    }

    // 生成新盐值（可选，也可复用原盐值，但推荐新盐值）
    QString newSalt = DatabaseManager::generateSalt();
    QString newHash = QCryptographicHash::hash((newPasswordMd5 + newSalt).toUtf8(), QCryptographicHash::Md5).toHex();
    QJsonObject updates;
    updates["password"] = newHash;
    updates["salt"] = newSalt;
    if (m_db->updateUser(userId, updates)) {
        return {{"success", true}, {"message", "密码修改成功"}};
    }
    return {{"success", false}, {"error", "修改失败"}};
}

QJsonObject ApiHandler::handleAISearch(int userId, const QJsonObject &data)
{
    QString requirement = data.value("requirement").toString().trimmed();
    if (requirement.isEmpty()) {
        return {{"success", false}, {"error", "请输入需求描述"}};
    }

    bool schoolOnly = data.value("school_only").toBool(true);
    QString schoolFilter;
    if (schoolOnly && userId > 0) {
        schoolFilter = getUserSchool(userId);
    }

    // 1. 调用混元 AI 解析需求
    QString prompt = QString(
                         "你是一个二手商品搜索助手。用户输入的需求是：\"%1\"\n"
                         "请从需求中提取以下搜索参数，并以 JSON 格式返回（只返回 JSON，不要有其他内容）：\n"
                         "{\n"
                         "  \"keyword\": \"核心关键词，如无则留空\",\n"
                         "  \"expanded_keywords\": [\"同义词或相关词1\", \"相关词2\"], // 最多3个\n"
                         "  \"category\": \"商品分类，只能是：书籍教材、电子产品、服饰鞋包、生活用品、体育器材、学习工具、美妆个护、其他。无法确定则留空\",\n"
                         "  \"min_price\": 最低价格（数字），无要求则为0,\n"
                         "  \"max_price\": 最高价格（数字），无要求则为0,\n"
                         "  \"sort_by\": \"排序方式：newest/price_asc/price_desc/view_count，默认 newest\"\n"
                         "}\n"
                         "示例：{\"keyword\":\"笔记本电脑\",\"expanded_keywords\":[\"笔记本\",\"电脑\"],\"category\":\"电子产品\",\"min_price\":0,\"max_price\":0,\"sort_by\":\"newest\"}"
                         ).arg(requirement);

    QString aiResultText;
    bool aiSuccess = false;
    HunyuanClient::instance()->analyzeText(prompt, [&](bool success, const QString& content) {
        aiSuccess = success;
        aiResultText = content;
    });

    if (!aiSuccess) {
        qWarning() << "AI 解析需求失败：" << aiResultText;
        // 降级为普通关键词搜索
        QJsonArray fallbackResult = m_db->searchGoods(requirement, 0, 0, 0, "newest", 1, 20, schoolFilter);
        QJsonObject resp;
        resp["goods_list"] = fallbackResult;
        resp["total"] = fallbackResult.size();
        resp["ai_used"] = false;
        return {{"success", true}, {"data", resp}};
    }

    // 2. 解析 AI 返回的 JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(aiResultText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "AI 返回格式错误：" << aiResultText;
        // 降级
        QJsonArray fallbackResult = m_db->searchGoods(requirement, 0, 0, 0, "newest", 1, 20, schoolFilter);
        QJsonObject resp;
        resp["goods_list"] = fallbackResult;
        resp["total"] = fallbackResult.size();
        resp["ai_used"] = false;
        return {{"success", true}, {"data", resp}};
    }

    QJsonObject params = doc.object();
    QString keyword = params.value("keyword").toString();
    QJsonArray expandedArray = params.value("expanded_keywords").toArray();
    QStringList expandedKeywords;
    for (const QJsonValue& ev : expandedArray) {
        expandedKeywords << ev.toString();
    }
    QString categoryName = params.value("category").toString();
    double minPrice = params.value("min_price").toDouble();
    double maxPrice = params.value("max_price").toDouble();
    QString sortBy = params.value("sort_by").toString();
    if (sortBy.isEmpty()) sortBy = "newest";

    QSet<int> goodsIds;           // 用于去重
    QJsonArray goodsList;

    // 3. 分类名称转分类ID
    int categoryId = 0;
    if (!categoryName.isEmpty()) {
        categoryId = m_db->getCategoryIdByName(categoryName);
    }

    // 4. 执行搜索
    auto searchOne = [&](const QString& kw) {
        if (kw.isEmpty()) return;
        QJsonArray arr = m_db->searchGoods(kw, categoryId, minPrice, maxPrice, sortBy, 1, 20, schoolFilter);
        for (const QJsonValue& val : arr) {
            int id = val.toObject().value("id").toInt();
            if (!goodsIds.contains(id)) {
                goodsIds.insert(id);
                goodsList.append(val);
            }
        }
    };
    // 先搜索主关键词
    searchOne(keyword);
    // 再搜索扩展词
    for (const QString& ek : expandedKeywords) {
        searchOne(ek);
    }
    QJsonObject result;
    result["goods_list"] = goodsList;
    result["total"] = goodsList.size();
    result["ai_used"] = true;
    result["parsed_params"] = params;

    return {{"success", true}, {"data", result}};
}

QJsonObject ApiHandler::handleGetGoodsForReview(int userId, const QJsonObject &data)
{
    // 检查管理员权限
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }

    QString keyword = data.value("keyword").toString();
    QString statusStr = data.value("status").toString();
    int status = -1; // -1 表示全部
    if (statusStr == "待审核") status = 0;
    else if (statusStr == "在售") status = 1;
    else if (statusStr == "交易中") status = 2;
    else if (statusStr == "已售出") status = 3;
    else if (statusStr == "已拒绝") status = 4;
    else if (statusStr == "已下架") status = 5;

    QString startDate = data.value("start_date").toString();
    QString endDate = data.value("end_date").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    QJsonArray goodsList = m_db->getGoodsForReview(keyword, status, startDate, endDate, page, pageSize);
    int total = m_db->getGoodsForReviewCount(keyword, status, startDate, endDate);

    QJsonObject resultData;
    resultData["list"] = goodsList;
    resultData["total"] = total;
    resultData["page"] = page;
    resultData["page_size"] = pageSize;
    return {{"success", true}, {"data", resultData}};
}

QJsonObject ApiHandler::handleGetSystemMessages(int userId, const QJsonObject &data)
{
    bool unreadOnly = data.value("unread_only").toBool(false);
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    QJsonArray messages = m_db->getSystemMessages(userId, unreadOnly, page, pageSize);

    QJsonObject result;
    result["messages"] = messages;
    result["total"] = messages.size();  // 实际应返回总数，可扩展

    return {{"success", true}, {"data", result}};
}

QJsonObject ApiHandler::handleMarkSystemMessageRead(int userId, const QJsonObject &data)
{
    int messageId = data.value("message_id").toInt();
    if (messageId <= 0) {
        return {{"success", false}, {"error", "无效的消息ID"}};
    }

    // 调用数据库更新，同时校验该消息是否属于当前用户
    if (m_db->markSystemMessageRead(messageId, userId)) {
        return {{"success", true}};
    } else {
        return {{"success", false}, {"error", "标记失败，可能消息不存在或不属于您"}};
    }
}

QJsonObject ApiHandler::handleGetUnreadSystemMessageCount(int userId, const QJsonObject &data)
{
    int count = m_db->getUnreadSystemMessageCount(userId);
    QJsonObject dataObj;
    dataObj["count"] = count;
    return {{"success", true}, {"data", dataObj}};
}

QJsonObject ApiHandler::handleGetCollaborativeRecommendations(int userId, const QJsonObject &data)
{
    int limit = data.value("limit").toInt(10);

    bool schoolOnly = data.value("school_only").toBool(true);
    QString school;
    if (schoolOnly && userId > 0) {
        school = getUserSchool(userId);
    }
    QJsonArray recs = m_db->getCollaborativeRecommendations(userId, limit, school);

    // 冷启动降级：若没有协同推荐结果，返回热度推荐
    if (recs.isEmpty()) {
        recs = m_db->getHotRecommendations(limit);
    }

    QJsonObject result;
    result["recommendations"] = recs;
    result["type"] = recs.isEmpty() ? "none" : (recs.size() > 0 && recs[0].toObject().contains("common_users") ? "collaborative" : "hot");
    return {{"success", true}, {"data", result}};
}

QJsonObject ApiHandler::handleSendResetCode(const QJsonObject &data)
{
    QString account = data.value("username").toString();
    QString email = data.value("email").toString();

    if (account.isEmpty() || email.isEmpty()) {
        return {{"success", false}, {"error", "账号和邮箱不能为空"}};
    }

    // 1. 查询用户信息
    QJsonObject user = m_db->getUserByAccount(account);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "账号不存在"}};
    }

    // 2. 验证邮箱是否匹配
    QString registeredEmail = user.value("email").toString();
    if (registeredEmail.compare(email, Qt::CaseInsensitive) != 0) {
        return {{"success", false}, {"error", "账号与邮箱不匹配"}};
    }

    // 2. 生成6位随机验证码
    QString code = QString::number(QRandomGenerator::global()->bounded(100000, 999999));

    // 3. 返回打码后的邮箱和验证码（演示模式）
    QString maskedEmail = m_db->getMaskedEmail(email);
    QJsonObject dataObj;
    dataObj["masked_email"] = maskedEmail;
    dataObj["code"] = code;   // 演示模式直接返回验证码，实际生产环境不应返回
    return {{"success", true}, {"data", dataObj}, {"message", "验证码已生成"}};
}

QJsonObject ApiHandler::handleResetPassword(const QJsonObject &data)
{
    QString account = data.value("username").toString();
    QString code = data.value("code").toString();
    QString newPasswordMd5 = data.value("new_password").toString(); // 客户端已MD5

    if (account.isEmpty() || code.isEmpty() || newPasswordMd5.isEmpty()) {
        return {{"success", false}, {"error", "参数不完整"}};
    }

    // 2. 获取用户信息以获取盐值
    QJsonObject user = m_db->getUserByAccount(account);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "账号不存在"}};
    }

    // 3. 检查用户状态（必须为正常状态，1=正常）
    if (user.value("status").toInt() != 1) {
        return {{"success", false}, {"error", "账号已被封禁，无法重置密码，请联系管理员"}};
    }

    QString salt = user.value("salt").toString();
    QString newHash = QCryptographicHash::hash((newPasswordMd5 + salt).toUtf8(), QCryptographicHash::Md5).toHex();

    // 4. 更新密码
    QJsonObject updates;
    updates["password"] = newHash;
    if (!m_db->updateUser(user.value("id").toInt(), updates)) {
        return {{"success", false}, {"error", "密码重置失败"}};
    }

    return {{"success", true}, {"message", "密码已重置，请重新登录"}};
}

QJsonObject ApiHandler::handleCheckAccountEmail(const QJsonObject &data) {
    QString account = data.value("username").toString();
    QString email = data.value("email").toString();

    if (account.isEmpty() || email.isEmpty()) {
        return {{"success", false}, {"error", "账号和邮箱不能为空"}};
    }

    QJsonObject user = m_db->getUserByAccount(account);
    if (user.isEmpty()) {
        return {{"success", false}, {"error", "账号不存在"}};
    }

    QString registeredEmail = user.value("email").toString();
    if (registeredEmail.compare(email, Qt::CaseInsensitive) != 0) {
        return {{"success", false}, {"error", "账号与邮箱不匹配"}};
    }

    return {{"success", true}, {"message", "验证通过"}};
}

QJsonObject ApiHandler::checkGoodsContent(const QString& name, const QString& description)
{
    // 构建提示词
    QString prompt = QString(
                         "你是一个校园二手交易平台的商品内容审核助手。请判断以下商品是否违规。\n"
                         "商品名称：%1\n"
                         "商品描述：%2\n\n"
                         "违规类型包括：\n"
                         "1. 违禁品：毒品、枪支弹药、管制刀具、淫秽色情、赌博、诈骗、危害国家安全等违法犯罪内容。\n"
                         "2. 不文明用语：脏话、侮辱性词汇等。\n\n"
                         "如果商品信息明确属于以上任何违规类型，请输出违规；否则输出不违规。\n"
                         "只返回 JSON 格式，不要有其他内容：\n"
                         "{\n"
                         "  \"is_violation\": true/false,\n"
                         "  \"reason\": \"具体违规原因\"\n"
                         "}\n"
                         "如果没有违规，is_violation 为 false，reason 为 \"正常商品\"。"
                         ).arg(name).arg(description);

    bool aiSuccess = false;
    QString resultText;

    // 调用混元 AI（同步等待）
    HunyuanClient::instance()->analyzeText(prompt, [&](bool success, const QString& content) {
        aiSuccess = success;
        resultText = content;
    });

    if (!aiSuccess || resultText.isEmpty()) {
        // AI 调用失败，降级处理：允许发布（或根据需求改为拒绝）
        qWarning() << "Content audit AI failed, allowing publish by default";
        QJsonObject fallback;
        fallback["is_violation"] = false;
        fallback["risk_level"] = "未知";
        fallback["reason"] = "审核服务暂时不可用，已自动通过";
        return fallback;
    }

    // 解析 AI 返回的 JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(resultText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Failed to parse AI audit result:" << resultText;
        // 解析失败，降级允许发布
        QJsonObject fallback;
        fallback["is_violation"] = false;
        fallback["risk_level"] = "未知";
        fallback["reason"] = "审核结果解析失败，已自动通过";
        return fallback;
    }

    return doc.object();
}

QString ApiHandler::getUserSchool(int userId)
{
    QJsonObject user = m_db->getUserById(userId);
    return user.value("school").toString();
}
