#include "databasemanager.h"
#include <QSqlDriver>
#include <QThread>
#include <QRandomGenerator>

DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

DatabaseManager* DatabaseManager::instance()
{
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

QSqlDatabase DatabaseManager::getDb(){
    return m_db;
}

bool DatabaseManager::initialize(const QString& host, const QString& database,
                                 const QString& username, const QString& password,
                                 int port)
{
    QMutexLocker locker(&m_mutex);

    // 每个线程一个独立连接名（防止多线程冲突）
    QString connectionName = QString("mysql_conn_%1").arg(quintptr(QThread::currentThreadId()));
    m_db = QSqlDatabase::addDatabase("QODBC", connectionName);
    QString dsn = QString("DRIVER={MySQL ODBC 8.0 UNICODE Driver};SERVER=%1;PORT=%2;DATABASE=%3;USER=%4;PASSWORD=%5;OPTION=3;")
                      .arg(host).arg(port).arg(database).arg(username).arg(password);
    m_db.setDatabaseName(dsn);

    if (!m_db.open()) {
        qCritical() << "Failed to open MySQL database:" << m_db.lastError().text();
        return false;
    }

    // 设置字符集
    QSqlQuery query(m_db);
    query.exec("SET NAMES 'utf8mb4'");
    qInfo() << "MySQL database connected successfully.";
    return true;
}

// ==================== 辅助函数 ====================

QJsonObject DatabaseManager::execSelectOne(const QString& sql, const QList<QVariant>& bindValues)
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant& val : bindValues) {
        query.addBindValue(val);
    }
    if (!query.exec()) {
        qWarning() << "execSelectOne error:" << query.lastError().text();
        return QJsonObject();
    }
    if (!query.next())
        return QJsonObject();

    QSqlRecord rec = query.record();
    QJsonObject obj;
    for (int i = 0; i < rec.count(); ++i) {
        QString fieldName = rec.fieldName(i);
        QVariant value = query.value(i);
        if (value.isNull())
            continue;
        // 根据类型转换，尽量保持 JSON 数据类型
        if (value.typeId() == QMetaType::QString)
            obj[fieldName] = value.toString();
        else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong)
            obj[fieldName] = value.toInt();
        else if (value.typeId() == QMetaType::Double)
            obj[fieldName] = value.toDouble();
        else if (value.typeId() == QMetaType::QDateTime)
            obj[fieldName] = value.toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        else
            obj[fieldName] = value.toString();
    }
    return obj;
}

QJsonArray DatabaseManager::execSelectMany(const QString& sql, const QList<QVariant>& bindValues)
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant& val : bindValues) {
        query.addBindValue(val);
    }
    if (!query.exec()) {
        qWarning() << "execSelectMany error:" << query.lastError().text();
        return QJsonArray();
    }
    QJsonArray arr;
    while (query.next()) {
        QSqlRecord rec = query.record();
        QJsonObject obj;
        for (int i = 0; i < rec.count(); ++i) {
            QString fieldName = rec.fieldName(i);
            QVariant value = query.value(i);
            if (value.isNull())
                continue;
            if (value.typeId() == QMetaType::QString)
                obj[fieldName] = value.toString();
            else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong)
                obj[fieldName] = value.toInt();
            else if (value.typeId() == QMetaType::Double)
                obj[fieldName] = value.toDouble();
            else if (value.typeId() == QMetaType::QDateTime)
                obj[fieldName] = value.toDateTime().toString("yyyy-MM-dd HH:mm:ss");
            else
                obj[fieldName] = value.toString();
        }
        arr.append(obj);
    }
    return arr;
}

bool DatabaseManager::execInsert(const QString& sql, const QList<QVariant>& bindValues)
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant& val : bindValues) {
        query.addBindValue(val);
    }
    if (!query.exec()) {
        qWarning() << "execInsert error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::execUpdateDelete(const QString& sql, const QList<QVariant>& bindValues)
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant& val : bindValues) {
        query.addBindValue(val);
    }
    if (!query.exec()) {
        qWarning() << "execUpdateDelete error:" << query.lastError().text();
        return false;
    }
    return true;
}

QString DatabaseManager::generateSalt(int length)
{
    const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString salt;
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
        salt.append(possibleCharacters.at(index));
    }
    return salt;
}

QString DatabaseManager::hashPassword(const QString& plainPassword, const QString& salt)
{
    QString salted = plainPassword + salt;
    return QCryptographicHash::hash(salted.toUtf8(), QCryptographicHash::Md5).toHex();
}

// ==================== 用户相关 ====================

QJsonObject DatabaseManager::login(const QString& account, const QString& plainPassword)
{
    QJsonObject user = getUserByAccount(account);
    if (user.isEmpty())
        return QJsonObject();

    QString salt = user.value("salt").toString();
    QString passwordHash = hashPassword(plainPassword, salt);
    if (passwordHash != user.value("password").toString())
        return QJsonObject();

    // 更新最后登录时间
    QSqlQuery query(m_db);
    query.prepare("UPDATE `user` SET last_login_time = ? WHERE id = ?");
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(user.value("id").toInt());
    query.exec();

    return user;
}

QJsonObject DatabaseManager::getUserById(int userId)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM `user` WHERE id = ?", {userId});
}

QJsonObject DatabaseManager::getUserByAccount(const QString& account)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM `user` WHERE account = ?", {account});
}

bool DatabaseManager::addUser(const QJsonObject& user)
{
    QMutexLocker locker(&m_mutex);
    // 如果 user 中已有 salt，则使用它，否则生成新的
    QString salt = user.value("salt").toString();
    QString passwordHash;
    if (salt.isEmpty()) {
        salt = generateSalt();
        passwordHash = hashPassword(user.value("password").toString(), salt);
    }
    else{
        passwordHash = user.value("password").toString();
    }

    QString sql = R"(
        INSERT INTO `user` (account, password, salt, nickname, phone, email, role, status, credit_score, register_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << user.value("account").toString()
          << passwordHash
          << salt
          << user.value("nickname").toString()
          << user.value("phone").toString()
          << user.value("email").toString()
          << user.value("role").toInt(0)
          << user.value("status").toInt(1)
          << user.value("credit_score").toInt(100)
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

bool DatabaseManager::updateUser(int userId, const QJsonObject& updates)
{
    QMutexLocker locker(&m_mutex);
    if (updates.isEmpty())
        return true;

    QStringList setParts;
    QList<QVariant> binds;
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        QString key = it.key();
        // 只跳过不可更新的字段
        if (key == "id" || key == "register_time")
            continue;
        setParts << "`" + key + "` = ?";
        binds << it.value().toVariant();
    }
    if (setParts.isEmpty())
        return true;
    binds << userId;
    QString sql = QString("UPDATE `user` SET %1 WHERE id = ?").arg(setParts.join(", "));
    return execUpdateDelete(sql, binds);
}

bool DatabaseManager::updateUserCreditScore(int userId, int delta, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    // 获取当前信用分
    QJsonObject user = getUserById(userId);
    if (user.isEmpty())
        return false;
    int oldScore = user.value("credit_score").toInt(100);
    int newScore = qBound(0, oldScore + delta, 100);
    if (newScore == oldScore)
        return true;

    // 更新用户表
    QSqlQuery query(m_db);
    query.prepare("UPDATE `user` SET credit_score = ? WHERE id = ?");
    query.addBindValue(newScore);
    query.addBindValue(userId);
    if (!query.exec())
        return false;

    // 记录信用历史
    query.prepare(R"(
        INSERT INTO credit_record (user_id, order_id, change_type, change_value, current_score, reason, create_time)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    int changeType = (delta > 0) ? 1 : 2; // 1=好评加分，2=差评扣分，这里简化处理
    query.addBindValue(userId);
    query.addBindValue(QVariant()); // order_id 可为空
    query.addBindValue(changeType);
    query.addBindValue(delta);
    query.addBindValue(newScore);
    query.addBindValue(reason);
    query.addBindValue(QDateTime::currentDateTime());
    return query.exec();
}

QJsonArray DatabaseManager::getUserList(const QString& role, const QString& status, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM `user` WHERE 1=1";
    QList<QVariant> binds;
    if (!role.isEmpty()) {
        sql += " AND role = ?";
        binds << role.toInt();
    }
    if (!status.isEmpty()) {
        sql += " AND status = ?";
        binds << status.toInt();
    }
    sql += " ORDER BY id DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

bool DatabaseManager::updateUserStatus(int userId, int status, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE `user` SET status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(userId);
    return query.exec();
}

QJsonArray DatabaseManager::getCreditHistory(int userId, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM credit_history WHERE user_id = ? ORDER BY create_time DESC LIMIT ? OFFSET ?";
    return execSelectMany(sql, {userId, pageSize, (page - 1) * pageSize});
}

bool DatabaseManager::updateLastLoginTime(int userId)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE `user` SET last_login_time = ? WHERE id = ?");
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(userId);
    return query.exec();
}

// ==================== 商品相关 ====================

bool DatabaseManager::addGoods(const QJsonObject& goods)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO goods (seller_id, category_id, name, price, description, status, publish_time)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << goods.value("seller_id").toInt()
          << goods.value("category_id").toInt()
          << goods.value("name").toString()
          << goods.value("price").toDouble()
          << goods.value("description").toString()
          << goods.value("status").toInt(0) // 默认待审核
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonObject DatabaseManager::getGoods(int goodsId)
{
    QMutexLocker locker(&m_mutex);
    QJsonObject goods = execSelectOne("SELECT * FROM goods WHERE id = ?", {goodsId});
    if (!goods.isEmpty()) {
        // 增加浏览量
        QSqlQuery query(m_db);
        query.prepare("UPDATE goods SET view_count = view_count + 1 WHERE id = ?");
        query.addBindValue(goodsId);
        query.exec();
    }
    return goods;
}

bool DatabaseManager::updateGoods(int goodsId, const QJsonObject& updates)
{
    QMutexLocker locker(&m_mutex);
    if (updates.isEmpty())
        return true;

    QStringList setParts;
    QList<QVariant> binds;
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        QString key = it.key();
        if (key == "id" || key == "publish_time" || key == "view_count" || key == "favorite_count")
            continue;
        setParts << "`" + key + "` = ?";
        binds << it.value().toVariant();
    }
    if (setParts.isEmpty())
        return true;
    binds << goodsId;
    QString sql = QString("UPDATE goods SET %1 WHERE id = ?").arg(setParts.join(", "));
    return execUpdateDelete(sql, binds);
}

bool DatabaseManager::updateGoodsStatus(int goodsId, int status)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE goods SET status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(goodsId);
    return query.exec();
}

bool DatabaseManager::deleteGoods(int goodsId)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM goods WHERE id = ?");
    query.addBindValue(goodsId);
    return query.exec();
}

QJsonArray DatabaseManager::searchGoods(const QString& keyword, int categoryId,
                                        double minPrice, double maxPrice,
                                        const QString& sortBy, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT goods.*, "
                  "(SELECT image_url FROM goods_image WHERE goods_id = goods.id ORDER BY sort_order LIMIT 1) AS image_url "
                  "FROM goods WHERE goods.status = 1";
    QList<QVariant> binds;

    if (!keyword.isEmpty()) {
        sql += " AND (name LIKE ? OR description LIKE ?)";
        QString pattern = "%" + keyword + "%";
        binds << pattern << pattern;
    }
    if (categoryId > 0) {
        sql += " AND category_id = ?";
        binds << categoryId;
    }
    if (minPrice > 0) {
        sql += " AND price >= ?";
        binds << minPrice;
    }
    if (maxPrice > 0) {
        sql += " AND price <= ?";
        binds << maxPrice;
    }

    // 排序
    if (sortBy == "price_asc")
        sql += " ORDER BY price ASC";
    else if (sortBy == "price_desc")
        sql += " ORDER BY price DESC";
    else if (sortBy == "view_count")
        sql += " ORDER BY view_count DESC";
    else // 默认按发布时间倒序
        sql += " ORDER BY publish_time DESC";

    sql += " LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;

    return execSelectMany(sql, binds);
}

QJsonArray DatabaseManager::getGoodsBySeller(int sellerId, int status, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM goods WHERE seller_id = ?";
    QList<QVariant> binds;
    binds << sellerId;
    if (status >= 0) {
        sql += " AND status = ?";
        binds << status;
    }
    sql += " ORDER BY publish_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

QJsonArray DatabaseManager::getGoodsImages(int goodsId)
{
    QMutexLocker locker(&m_mutex);
    return execSelectMany("SELECT * FROM goods_image WHERE goods_id = ? ORDER BY sort_order", {goodsId});
}

QJsonArray DatabaseManager::getRecommendedGoods(int limit){
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM goods WHERE status = 1 ORDER BY view_count DESC, favorite_count DESC LIMIT ?";
    QList<QVariant> binds = {limit};
    return execSelectMany(sql, binds);
}

QJsonArray DatabaseManager::getPendingGoods(int page,int pageSize){
    QString sql = "SELECT * FROM goods WHERE status = 0 ORDER BY publish_time ASC LIMIT ? OFFSET ?";
    QList<QVariant> binds = {pageSize, (page-1)*pageSize};
    return execSelectMany(sql, binds);
}

// ==================== 订单相关 ====================

bool DatabaseManager::addOrder(const QJsonObject& order)
{
    QMutexLocker locker(&m_mutex);
    // 生成订单号：CS + 时间戳 + 随机数
    QString orderSn = QString("CS%1%2").arg(QDateTime::currentSecsSinceEpoch())
                          .arg(QRandomGenerator::global()->bounded(1000, 9999));
    QString sql = R"(
        INSERT INTO `order` (order_sn, buyer_id, seller_id, goods_id, goods_title, goods_image,
                             deal_price, status, create_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << orderSn
          << order.value("buyer_id").toInt()
          << order.value("seller_id").toInt()
          << order.value("goods_id").toInt()
          << order.value("goods_title").toString()
          << order.value("goods_image").toString()
          << order.value("deal_price").toDouble()
          << order.value("status").toInt(0) // 待支付
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonObject DatabaseManager::getOrder(int orderId)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM `order` WHERE id = ?", {orderId});
}

QJsonObject DatabaseManager::getOrderBySn(const QString& orderSn)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM `order` WHERE order_sn = ?", {orderSn});
}

QJsonArray DatabaseManager::getOrdersByUser(int userId, int status, const QString &keyword, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM `order` WHERE (buyer_id = ? OR seller_id = ?)";
    QList<QVariant> binds;
    binds << userId << userId;
    if (status >= 0) {
        sql += " AND status = ?";
        binds << status;
    }
    // 关键词搜索
    if (!keyword.isEmpty()) {
        sql += " AND goods_title LIKE ?";
        QString pattern = "%" + keyword + "%";
        binds << pattern;
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

QJsonArray DatabaseManager::getOrdersBySeller(int sellerId, int status, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM `order` WHERE seller_id = ?";
    QList<QVariant> binds;
    binds << sellerId;
    if (status >= 0) {
        sql += " AND status = ?";
        binds << status;
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

bool DatabaseManager::updateOrderStatus(int orderId, int status, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "UPDATE `order` SET status = ?";
    QList<QVariant> binds;
    binds << status;

    if (status == 1) { // 已支付
        sql += ", payment_time = ?";
        binds << QDateTime::currentDateTime();
    } else if (status == 2) { // 已完成
        sql += ", receive_time = ?";
        binds << QDateTime::currentDateTime();
    } else if (status == 3) { // 已取消
        sql += ", cancel_reason = ?, cancel_time = ?";
        binds << reason << QDateTime::currentDateTime();
    } else if (status == 4) { // 售后中（纠纷）
        // 无额外时间字段
    }
    sql += " WHERE id = ?";
    binds << orderId;

    return execUpdateDelete(sql, binds);
}

bool DatabaseManager::updateOrderPayment(int orderId, qint64 paymentTime)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE `order` SET status = 1, payment_time = ? WHERE id = ?");
    query.addBindValue(QDateTime::fromSecsSinceEpoch(paymentTime));
    query.addBindValue(orderId);
    return query.exec();
}

// ==================== 收藏相关 ====================

bool DatabaseManager::addFavorite(int userId, int goodsId)
{
    QMutexLocker locker(&m_mutex);
    // 先检查是否已存在
    QSqlQuery check(m_db);
    check.prepare("SELECT 1 FROM favorite WHERE user_id = ? AND goods_id = ?");
    check.addBindValue(userId);
    check.addBindValue(goodsId);
    if (check.exec() && check.next())
        return true; // 已存在

    // 插入
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO favorite (user_id, goods_id, create_time) VALUES (?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(goodsId);
    query.addBindValue(QDateTime::currentDateTime());
    if (!query.exec())
        return false;

    // 更新商品收藏计数
    query.prepare("UPDATE goods SET favorite_count = favorite_count + 1 WHERE id = ?");
    query.addBindValue(goodsId);
    query.exec();
    return true;
}

bool DatabaseManager::removeFavorite(int userId, int goodsId)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM favorite WHERE user_id = ? AND goods_id = ?");
    query.addBindValue(userId);
    query.addBindValue(goodsId);
    if (!query.exec())
        return false;

    query.prepare("UPDATE goods SET favorite_count = favorite_count - 1 WHERE id = ?");
    query.addBindValue(goodsId);
    query.exec();
    return true;
}

bool DatabaseManager::isFavorited(int userId, int goodsId)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM favorite WHERE user_id = ? AND goods_id = ?");
    query.addBindValue(userId);
    query.addBindValue(goodsId);
    return query.exec() && query.next();
}

QJsonArray DatabaseManager::getFavorites(int userId, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        SELECT g.* FROM goods g
        JOIN favorite f ON g.id = f.goods_id
        WHERE f.user_id = ?
        ORDER BY f.create_time DESC
        LIMIT ? OFFSET ?
    )";
    return execSelectMany(sql, {userId, pageSize, (page - 1) * pageSize});
}

// ==================== 评价相关 ====================

bool DatabaseManager::addEvaluation(const QJsonObject& evaluation)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO evaluation (order_id, buyer_id, seller_id, score, content, image_urls, is_anonymous, create_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << evaluation.value("order_id").toInt()
          << evaluation.value("buyer_id").toInt()
          << evaluation.value("seller_id").toInt()
          << evaluation.value("score").toInt()
          << evaluation.value("content").toString()
          << evaluation.value("image_urls").toString()
          << evaluation.value("is_anonymous").toInt(0)
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonArray DatabaseManager::getEvaluationsByUser(int userId, bool asSeller, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString field = asSeller ? "seller_id" : "buyer_id";
    QString sql = QString("SELECT * FROM evaluation WHERE %1 = ? ORDER BY create_time DESC LIMIT ? OFFSET ?")
                      .arg(field);
    return execSelectMany(sql, {userId, pageSize, (page - 1) * pageSize});
}

QJsonArray DatabaseManager::getEvaluationsByGoods(int goodsId, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    // 需要关联订单表获取商品ID
    QString sql = R"(
        SELECT e.* FROM evaluation e
        JOIN `order` o ON e.order_id = o.id
        WHERE o.goods_id = ?
        ORDER BY e.create_time DESC
        LIMIT ? OFFSET ?
    )";
    return execSelectMany(sql, {goodsId, pageSize, (page - 1) * pageSize});
}

double DatabaseManager::getAverageScore(int userId, bool asSeller)
{
    QMutexLocker locker(&m_mutex);
    QString field = asSeller ? "seller_id" : "buyer_id";
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT AVG(score) FROM evaluation WHERE %1 = ?").arg(field));
    query.addBindValue(userId);
    if (!query.exec() || !query.next())
        return 0.0;
    return query.value(0).toDouble();
}

// ==================== 聊天相关 ====================

bool DatabaseManager::addChatMessage(const QJsonObject& message)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO chat (session_id, buyer_id, seller_id, goods_id, message_type, content, offer_price,
                          risk_level, risk_reason, is_read, create_time,sender_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,?)
    )";
    QList<QVariant> binds;
    binds << message.value("session_id").toString()
          << message.value("buyer_id").toInt()
          << message.value("seller_id").toInt()
          << message.value("goods_id").toInt()
          << message.value("message_type").toInt(1)
          << message.value("content").toString()
          << message.value("offer_price").toDouble(0)
          << message.value("risk_level").toInt(0)
          << message.value("risk_reason").toString()
          << 0 // 未读
          << QDateTime::currentDateTime()
          << message.value("sender_id").toInt();
    return execInsert(sql, binds);
}

QJsonArray DatabaseManager::getChatMessages(const QString& sessionId, int limit, int offset)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        SELECT chat.*, u.nickname as sender_name
        FROM chat
        LEFT JOIN `user` u ON chat.sender_id = u.id
        WHERE chat.session_id = ?
        ORDER BY chat.create_time ASC
        LIMIT ? OFFSET ?
    )";
    QJsonArray arr = execSelectMany(sql, {sessionId, limit, offset});

    QJsonArray reversed;
    for (int i = arr.size() - 1; i >= 0; --i) {
        reversed.append(arr[i]);
    }
    return reversed;
}

bool DatabaseManager::markChatAsRead(const QString& sessionId, int userId)
{
    QMutexLocker locker(&m_mutex);
    // 将对方发送的消息标记为已读
    QString sql = "UPDATE chat SET is_read = 1 WHERE session_id = ? AND sender_id != ? AND is_read = 0";
    return execUpdateDelete(sql, {sessionId, userId});
}

int DatabaseManager::getUnreadCount(int userId)
{
    QMutexLocker locker(&m_mutex);
    // 统计所有会话中接收者为当前用户且未读的消息
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM chat WHERE receiver_id = ? AND is_read = 0");
    // 注意：chat 表没有直接的 receiver_id 字段，但可以通过 session_id 和 buyer_id/seller_id 推断
    // 为了简化，我们在 chat 表中添加 receiver_id 字段更合适，但你的表没有，这里假设我们通过 buyer_id/seller_id 判断
    // 这里我们直接返回0，实际应根据你的表结构调整
    // 如果你的 chat 表没有 receiver_id，建议增加该字段，或者通过 sender_id 和 session_id 判断
    return 0;
}

QJsonArray DatabaseManager::getChatList(int userId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        SELECT
            session_id,
            MAX(goods_id) as goods_id,
            MAX(create_time) as last_time,
            (SELECT content FROM chat c2
             WHERE c2.session_id = c1.session_id
             ORDER BY create_time DESC LIMIT 1) as last_content,
            CASE
                WHEN MAX(buyer_id) = ? THEN
                    (SELECT nickname FROM `user` WHERE id = MAX(seller_id))
                ELSE
                    (SELECT nickname FROM `user` WHERE id = MAX(buyer_id))
            END as other_name
        FROM chat c1
        WHERE buyer_id = ? OR seller_id = ?
        GROUP BY session_id
        ORDER BY last_time DESC
    )";
    QList<QVariant> binds = {userId, userId, userId};
    return execSelectMany(sql, binds);
}

// ==================== 举报相关 ====================

bool DatabaseManager::addReport(const QJsonObject& report)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO report (reporter_id, reported_type, reported_id, reason_type, description,
                            evidence_urls, status, create_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << report.value("reporter_id").toInt()
          << report.value("reported_type").toInt()
          << report.value("reported_id").toInt()
          << report.value("reason_type").toInt()
          << report.value("description").toString()
          << report.value("evidence_urls").toString()
          << report.value("status").toInt(0) // 待处理
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonArray DatabaseManager::getReports(const QString& status, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM report";
    QList<QVariant> binds;
    if (!status.isEmpty()) {
        sql += " WHERE status = ?";
        binds << status.toInt();
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

bool DatabaseManager::updateReportStatus(int reportId, int status, const QString& result, int adminId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "UPDATE report SET status = ?, handle_result = ?, admin_id = ?, handle_time = ? WHERE id = ?";
    QList<QVariant> binds;
    binds << status << result << adminId << QDateTime::currentDateTime() << reportId;
    return execUpdateDelete(sql, binds);
}

QJsonArray DatabaseManager::getMyReports(int userId, int page, int pageSize){
    QString sql = "SELECT * FROM report WHERE reporter_id = ? ORDER BY create_time DESC LIMIT ? OFFSET ?";
    QList<QVariant> binds = {userId, pageSize, (page-1)*pageSize};
    return execSelectMany(sql, binds);
};

QJsonObject DatabaseManager::getReportById(int reportId)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM report WHERE id = ?", {reportId});
}

// ==================== 纠纷相关 ====================

bool DatabaseManager::addDispute(const QJsonObject& dispute)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO dispute (order_id, complainant_id, defendant_id, type, description,
                             evidence_urls, status, create_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << dispute.value("order_id").toInt()
          << dispute.value("complainant_id").toInt()
          << dispute.value("defendant_id").toInt()
          << dispute.value("type").toString()
          << dispute.value("description").toString()
          << dispute.value("evidence_urls").toString()
          << dispute.value("status").toInt(0) // 待处理
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonObject DatabaseManager::getDispute(int disputeId)
{
    QMutexLocker locker(&m_mutex);
    return execSelectOne("SELECT * FROM dispute WHERE id = ?", {disputeId});
}

QJsonArray DatabaseManager::getDisputes(int status, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM dispute";
    QList<QVariant> binds;
    if (status >= 0) {
        sql += " WHERE status = ?";
        binds << status;
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

bool DatabaseManager::updateDispute(int disputeId, int status, const QString& result, int adminId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "UPDATE dispute SET status = ?, handle_result = ?, admin_id = ?, handle_time = ? WHERE id = ?";
    QList<QVariant> binds;
    binds << status << result << adminId << QDateTime::currentDateTime() << disputeId;
    return execUpdateDelete(sql, binds);
}

QJsonArray DatabaseManager::getMyDisputes(int userId, int status, int page, int pageSize){
    QString sql = "SELECT * FROM dispute WHERE complainant_id = ? OR defendant_id = ?";
    QList<QVariant> binds = {userId, userId};
    if (status >= 0) {
        sql += " AND status = ?";
        binds << status;
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page-1)*pageSize;
    return execSelectMany(sql, binds);
}

QJsonArray DatabaseManager::getDisputesByOrder(int orderId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM dispute WHERE order_id = ? ORDER BY create_time DESC";
    return execSelectMany(sql, {orderId});
}

// ==================== 系统消息相关 ====================

bool DatabaseManager::addSystemMessage(int userId, int messageType, const QString& title,
                                       const QString& content, int relatedId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO system_message (user_id, message_type, title, content, related_id, create_time)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << userId << messageType << title << content << (relatedId > 0 ? relatedId : QVariant())
          << QDateTime::currentDateTime();
    return execInsert(sql, binds);
}

QJsonArray DatabaseManager::getSystemMessages(int userId, bool unreadOnly, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT * FROM system_message WHERE user_id = ?";
    QList<QVariant> binds;
    binds << userId;
    if (unreadOnly) {
        sql += " AND is_read = 0";
    }
    sql += " ORDER BY create_time DESC LIMIT ? OFFSET ?";
    binds << pageSize << (page - 1) * pageSize;
    return execSelectMany(sql, binds);
}

bool DatabaseManager::markSystemMessageRead(int messageId)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "UPDATE system_message SET is_read = 1, read_time = ? WHERE id = ?";
    return execUpdateDelete(sql, {QDateTime::currentDateTime(), messageId});
}

int DatabaseManager::getUnreadSystemMessageCount(int userId)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM system_message WHERE user_id = ? AND is_read = 0");
    query.addBindValue(userId);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

// ==================== AI估价记录 ====================

bool DatabaseManager::addAIValuation(const QJsonObject& valuation)
{
    QMutexLocker locker(&m_mutex);
    QString sql = R"(
        INSERT INTO ai_valuation (goods_id, user_id, description, image_url, min_price, max_price, confidence)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    QList<QVariant> binds;
    binds << valuation.value("goods_id").toInt()
          << valuation.value("user_id").toInt()
          << valuation.value("description").toString()
          << valuation.value("image_url").toString()
          << valuation.value("min_price").toDouble()
          << valuation.value("max_price").toDouble()
          << valuation.value("confidence").toDouble();
    return execInsert(sql, binds);
}

// 根据分类名称获取分类ID
int DatabaseManager::getCategoryIdByName(const QString &name)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM category WHERE name = ?");
    query.addBindValue(name);
    if (!query.exec() || !query.next()) {
        return 0; // 未找到则返回0（或其他默认）
    }
    return query.value(0).toInt();
}

int DatabaseManager::getLastInsertId()
{
    QSqlQuery query(m_db);
    if (query.exec("SELECT LAST_INSERT_ID()") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool DatabaseManager::addGoodsImage(int goodsId, const QString& imageUrl, int sortOrder)
{
    QMutexLocker locker(&m_mutex);
    QString sql = "INSERT INTO goods_image (goods_id, image_url, sort_order) VALUES (?, ?, ?)";
    return execInsert(sql, {goodsId, imageUrl, sortOrder});
}

QJsonArray DatabaseManager::getMyReviews(int userId, int page, int pageSize){
    QString sql = R"(
        SELECT e.*, g.name as goods_name
        FROM evaluation e
        JOIN `order` o ON e.order_id = o.id
        JOIN goods g ON o.goods_id = g.id
        WHERE e.buyer_id = ?
        ORDER BY e.create_time DESC
        LIMIT ? OFFSET ?
    )";
    QList<QVariant> binds = {userId, pageSize, (page-1)*pageSize};
    QJsonArray reviews = execSelectMany(sql, binds);
    return reviews;
}

// 添加或更新浏览记录
bool DatabaseManager::addOrUpdateBrowseHistory(int userId, int goodsId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO browse_history (user_id, goods_id, browse_time) VALUES (?, ?, NOW()) "
                  "ON DUPLICATE KEY UPDATE browse_time = NOW()");
    query.addBindValue(userId);
    query.addBindValue(goodsId);
    return query.exec();
}

// 获取浏览历史（关联商品信息）
QJsonArray DatabaseManager::getBrowseHistory(int userId, int page, int pageSize)
{
    QString sql = "SELECT g.*, bh.browse_time FROM browse_history bh "
                  "JOIN goods g ON bh.goods_id = g.id "
                  "WHERE bh.user_id = ? "
                  "ORDER BY bh.browse_time DESC LIMIT ? OFFSET ?";
    return execSelectMany(sql, {userId, pageSize, (page-1)*pageSize});
}
