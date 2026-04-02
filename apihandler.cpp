#include "apihandler.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QDir>

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
        (method == "POST" && path == "/api/auth/register")) {
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
    } else if (routeKey == "GET:/api/user/profile") {
        response = handleGetUserProfile(userId, request);
    } else if (routeKey == "POST:/api/user/profile") {
        response = handleUpdateUserProfile(userId, request);
    } else if (routeKey == "POST:/api/user/avatar") {
        response = handleUploadAvatar(userId, request);
    } else if (routeKey == "POST:/api/goods/publish") {
        response = handlePublishGoods(userId, request);
    } else if (routeKey == "GET:/api/goods/search") {
        response = handleSearchGoods(request);
    } else if (routeKey == "GET:/api/goods/detail") {
        response = handleGetGoodsDetail(request);
    } else if (routeKey == "POST:/api/goods/update") {
        response = handleUpdateGoods(userId, request);
    } else if (routeKey == "POST:/api/goods/delete") {
        response = handleDeleteGoods(userId, request);
    } else if (routeKey == "GET:/api/goods/recommended") {
        response = handleGetRecommendedGoods(request);
    } else if (routeKey == "POST:/api/order/create") {
        response = handleCreateOrder(userId, request);
    } else if (routeKey == "POST:/api/order/pay") {
        response = handlePayOrder(userId, request);
    } else if (routeKey == "POST:/api/order/confirm") {
        response = handleConfirmOrder(userId, request);
    } else if (routeKey == "POST:/api/order/cancel") {
        response = handleCancelOrder(userId, request);
    } else if (routeKey == "GET:/api/order/list") {
        response = handleGetOrderList(userId, request);
    } else if (routeKey == "GET:/api/order/detail") {
        response = handleGetOrderDetail(userId, request);
    } else if (routeKey == "POST:/api/message/send") {
        response = handleSendMessage(userId, request);
    } else if (routeKey == "GET:/api/message/history") {
        response = handleGetMessageHistory(userId, request);
    } else if (routeKey == "GET:/api/message/chatlist") {
        response = handleGetChatList(userId);
    } else if (routeKey == "POST:/api/review/submit") {
        response = handleSubmitReview(userId, request);
    } else if (routeKey == "GET:/api/review/goods") {
        response = handleGetGoodsReviews(request);
    } else if (routeKey == "GET:/api/review/seller") {
        response = handleGetSellerReviews(request);
    } else if (routeKey == "POST:/api/favorite/add") {
        response = handleAddFavorite(userId, request);
    } else if (routeKey == "POST:/api/favorite/remove") {
        response = handleRemoveFavorite(userId, request);
    } else if (routeKey == "GET:/api/favorite/list") {
        response = handleGetFavorites(userId, request);
    } else if (routeKey == "GET:/api/credit/score") {
        response = handleGetCreditScore(userId, request);
    } else if (routeKey == "GET:/api/credit/history") {
        response = handleGetCreditHistory(userId, request);
    } else if (routeKey == "POST:/api/report/submit") {
        response = handleSubmitReport(userId, request);
    } else if (routeKey == "GET:/api/report/mylist") {
        response = handleGetMyReports(userId, request);
    } else if (routeKey == "POST:/api/dispute/submit") {
        response = handleSubmitDispute(userId, request);
    } else if (routeKey == "GET:/api/dispute/detail") {
        response = handleGetDisputeDetail(userId, request);
    } else if (routeKey == "GET:/api/dispute/mylist") {
        response = handleGetMyDisputes(userId, request);
    } else if (routeKey == "GET:/api/admin/pending_goods") {
        response = handleGetPendingGoods(userId, request);
    } else if (routeKey == "POST:/api/admin/review_goods") {
        response = handleReviewGoods(userId, request);
    } else if (routeKey == "GET:/api/admin/user_list") {
        response = handleGetUserList(userId, request);
    } else if (routeKey == "POST:/api/admin/update_user_status") {
        response = handleUpdateUserStatus(userId, request);
    } else if (routeKey == "GET:/api/admin/dispute_list") {
        response = handleGetDisputeList(userId, request);
    } else if (routeKey == "POST:/api/admin/process_dispute") {
        response = handleProcessDispute(userId, request);
    } else if (routeKey == "GET:/api/admin/statistics") {
        response = handleGetStatistics(userId, request);
    } else if (routeKey == "POST:/api/ai/estimate") {
        response = handleEstimatePrice(userId, request);
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

    // 检查角色（可选）
    if (role == "admin" && user.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "非管理员账号"}};
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
    QJsonObject goods;
    goods["seller_id"] = userId;
    int categoryId = m_db->getCategoryIdByName(data.value("category").toString());
    goods["category_id"] = categoryId;
    goods["name"] = data.value("name").toString();
    goods["price"] = data.value("price").toDouble();
    goods["description"] = data.value("description").toString();
    goods["status"] = 0; // 待审核

    if (m_db->addGoods(goods)) {
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
    double minPrice = data.value("min_price").toDouble();
    double maxPrice = data.value("max_price").toDouble();
    QString sortBy = data.value("sort_by").toString("publish_time");
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);

    QJsonArray goodsList = m_db->searchGoods(keyword, categoryId, minPrice, maxPrice, sortBy, page, pageSize);
    QJsonObject result;
    result["goods_list"] = goodsList;
    result["total"] = goodsList.size(); // 实际应返回总数，这里简化
    return {{"success", true}, {"data", result}};
}

// 获取商品详情
QJsonObject ApiHandler::handleGetGoodsDetail(const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    if (goodsId <= 0) return {{"success", false}, {"error", "无效商品ID"}};

    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) {
        return {{"success", false}, {"error", "商品不存在"}};
    }
    // 获取商品图片
    QJsonArray images = m_db->getGoodsImages(goodsId);
    goods["images"] = images;

    // 获取卖家信息
    int sellerId = goods.value("seller_id").toInt();
    QJsonObject seller = m_db->getUserById(sellerId);
    goods["seller_name"] = seller.value("nickname").toString();
    goods["seller_avatar"] = seller.value("avatar_url").toString();
    goods["seller_credit"] = seller.value("credit_score").toInt();

    return {{"success", true}, {"data", goods}};
}

// 创建订单
QJsonObject ApiHandler::handleCreateOrder(int userId, const QJsonObject &data)
{
    int goodsId = data.value("goods_id").toInt();
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};
    if (goods.value("status").toString().toInt() != 1) { // 1=已上架
        return {{"success", false}, {"error", "商品不可购买"}};
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
    int receiverId = data.value("receiver_id").toInt();
    QString content = data.value("content").toString();
    int goodsId = data.value("goods_id").toInt();

    // 生成sessionId
    QString sessionId = QString("%1_%2_%3").arg(qMin(userId, receiverId))
                            .arg(qMax(userId, receiverId))
                            .arg(goodsId);

    QJsonObject message;
    message["session_id"] = sessionId;
    message["buyer_id"] = userId; // 注意：这里需要区分买家和卖家，简单起见存双方ID，但你的表有buyer_id和seller_id，需要正确填充
    // 实际上chat表需要buyer_id和seller_id，我们需要根据商品获取卖家ID
    QJsonObject goods = m_db->getGoods(goodsId);
    if (goods.isEmpty()) return {{"success", false}, {"error", "商品不存在"}};
    int sellerId = goods.value("seller_id").toInt();
    if (userId == sellerId) {
        // 卖家发送消息，买家是receiverId
        message["buyer_id"] = receiverId;
        message["seller_id"] = userId;
    } else {
        // 买家发送消息
        message["buyer_id"] = userId;
        message["seller_id"] = sellerId;
    }
    message["goods_id"] = goodsId;
    message["message_type"] = 1; // 文本
    message["content"] = content;

    if (m_db->addChatMessage(message)) {
        // 通过WebSocket实时推送
        QJsonObject wsMsg;
        wsMsg["type"] = 1; // 聊天消息
        wsMsg["data"] = message;
        m_ws->sendToUser(receiverId, wsMsg);
        return {{"success", true}, {"message", "发送成功"}};
    } else {
        return {{"success", false}, {"error", "发送失败"}};
    }
}

// 处理WebSocket消息
void ApiHandler::handleWebSocketMessage(int senderId, const QJsonObject &message)
{
    int type = message.value("type").toInt();
    QJsonObject data = message.value("data").toObject();

    switch (type) {
    case 1: // 聊天消息
    {
        int receiverId = data.value("receiver_id").toInt();
        QString content = data.value("content").toString();
        int goodsId = data.value("goods_id").toInt();

        // 构造消息对象并保存
        QJsonObject chatMsg;
        chatMsg["session_id"] = QString("%1_%2_%3").arg(qMin(senderId, receiverId))
                                    .arg(qMax(senderId, receiverId))
                                    .arg(goodsId);
        chatMsg["buyer_id"] = senderId; // 简化，实际需判断
        chatMsg["seller_id"] = receiverId;
        chatMsg["goods_id"] = goodsId;
        chatMsg["message_type"] = 1;
        chatMsg["content"] = content;
        m_db->addChatMessage(chatMsg);

        // 转发给接收者
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
    // 也可以允许更新其他字段如 status, role 但需要管理员权限，此处暂不处理

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
    if (order.value("status").toInt() != 0) { // 0=待支付
        return {{"success", false}, {"error", "订单状态不正确，无法取消"}};
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
    // status 可能为字符串，需要转换为 int
    int statusInt = -1;
    if (status == "pending_payment") statusInt = 0;
    else if (status == "paid") statusInt = 1;
    else if (status == "completed") statusInt = 2;
    else if (status == "cancelled") statusInt = 3;
    else if (status == "dispute") statusInt = 4;
    // 如果是卖家查看订单，可能需要根据卖家ID查询
    QJsonArray orders = m_db->getOrdersByUser(userId, statusInt, page, pageSize);
    return {{"success", true}, {"data", QJsonObject{{"orders", orders}}}};
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
    // 标记已读
    m_db->markChatAsRead(chatId, userId);
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
    int sellerId = data.value("seller_id").toInt();
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
    int orderId = data.value("order_id").toInt();
    QString type = data.value("type").toString();
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
        if (user.value("role").toInt() != 1) {
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
    // 检查管理员权限
    QJsonObject user = m_db->getUserById(userId);
    if (user.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray goodsList = m_db->getPendingGoods(page,pageSize);
    return {{"success", true}, {"data", goodsList}};
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
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    QJsonArray users = m_db->getUserList(role, status, page, pageSize);
    return {{"success", true}, {"data", users}};
}

QJsonObject ApiHandler::handleUpdateUserStatus(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int targetUserId = data.value("user_id").toInt();
    int newStatus = data.value("status").toInt(); // 0=禁用，1=正常
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
    QString status = data.value("status").toString();
    int page = data.value("page").toInt(1);
    int pageSize = data.value("page_size").toInt(20);
    int statusInt = -1;
    if (status == "pending") statusInt = 0;
    else if (status == "processing") statusInt = 1;
    else if (status == "resolved") statusInt = 2;
    else if (status == "closed") statusInt = 3;
    QJsonArray disputes = m_db->getDisputes(statusInt, page, pageSize);
    return {{"success", true}, {"data", disputes}};
}

QJsonObject ApiHandler::handleProcessDispute(int userId, const QJsonObject &data)
{
    QJsonObject admin = m_db->getUserById(userId);
    if (admin.value("role").toString().toInt() != 1) {
        return {{"success", false}, {"error", "需要管理员权限"}};
    }
    int disputeId = data.value("dispute_id").toInt();
    int newStatus = data.value("status").toInt(); // 2=已解决，3=已关闭
    QString result = data.value("result").toString(); // 处理结果说明

    if (disputeId <= 0) return {{"success", false}, {"error", "无效纠纷ID"}};
    QJsonObject dispute = m_db->getDispute(disputeId);
    if (dispute.isEmpty()) return {{"success", false}, {"error", "纠纷不存在"}};

    if (m_db->updateDispute(disputeId, newStatus, result, userId)) {
        // 更新关联订单状态（如退款等），根据 result 可能还需要调整信用分
        int orderId = dispute.value("order_id").toInt();
        // 例如，如果纠纷解决结果为退款，将订单状态改为已取消（3）
        // 这里简单处理：将订单状态改为 4（售后中）改为 2（已完成）或 3（已取消）？根据实际业务
        // 我们假设纠纷解决后订单状态变为已完成（2）
        m_db->updateOrderStatus(orderId, 2);
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
    QString imagePath = data.value("image_path").toString();

    if (description.isEmpty() && imagePath.isEmpty()) {
        return {{"success", false}, {"error", "请提供商品描述或图片"}};
    }

    // 模拟AI估价：根据描述关键词简单返回
    double minPrice = 0, maxPrice = 0;
    if (description.contains("手机") || description.contains("iPhone")) {
        minPrice = 2000; maxPrice = 3000;
    } else if (description.contains("教材") || description.contains("书")) {
        minPrice = 20; maxPrice = 80;
    } else if (description.contains("电脑") || description.contains("笔记本")) {
        minPrice = 2000; maxPrice = 5000;
    } else {
        minPrice = 50; maxPrice = 200;
    }

    // 保存估价记录
    QJsonObject valuation;
    valuation["goods_id"] = 0; // 暂时没有关联商品，设为0
    valuation["user_id"] = userId;
    valuation["description"] = description;
    valuation["image_url"] = imagePath;
    valuation["min_price"] = minPrice;
    valuation["max_price"] = maxPrice;
    valuation["confidence"] = 85.0;
    m_db->addAIValuation(valuation);

    QJsonObject dataObj;
    dataObj["min_price"] = minPrice;
    dataObj["max_price"] = maxPrice;
    dataObj["confidence"] = 85;
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
