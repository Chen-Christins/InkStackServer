#include "notification_manager.h"

#include "../util.h"
#include "cache_util.h"
#include "ws_connection_manager.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:notif:";

static std::string notificationCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::NotificationInfo::ptr parseNotificationCache(const Json::Value& v)
{
    auto info = std::make_shared<data::NotificationInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setTitle(cacheJsonString(v, "title"));
    info->setContent(cacheJsonString(v, "content"));
    info->setType(cacheJsonString(v, "type"));
    info->setSenderId(cacheJsonInt64(v, "sender_id"));
    info->setArticleId(cacheJsonInt64(v, "article_id"));
    info->setCommentId(cacheJsonInt64(v, "comment_id"));
    info->setIsRead(cacheJsonInt32(v, "is_read"));
    info->setReadTime(cacheJsonTime(v, "read_time"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->setIsBroadcast(cacheJsonInt32(v, "is_broadcast"));
    info->setLevel(cacheJsonString(v, "level"));
    info->setStartTime(cacheJsonInt64(v, "start_time"));
    info->setEndTime(cacheJsonInt64(v, "end_time"));
    info->markClean();
    return info;
}

static data::NotificationInfo::ptr getNotificationCache(int64_t id)
{
    std::string key = notificationCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseNotificationCache(v);
}

static void setNotificationCache(const data::NotificationInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(notificationCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::NotificationInfo::ptr NotificationManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::NotificationInfoDao::ParseRow(rt);
}

// ========== WS推送 ==========

int32_t NotificationManager::sendToUser(int64_t user_id, const std::string& message)
{
    return WSMgr::GetInstance()->sendToUser(user_id, BuildChannelMessage(message, "notification"));
}

void NotificationManager::broadcast(const std::string& message)
{
    WSMgr::GetInstance()->broadcast(BuildChannelMessage(message, "notification"));
}

int32_t NotificationManager::getOnlineCount()
{
    return WSMgr::GetInstance()->onlineUserCount();
}

// ========== DB persistence ==========

data::NotificationInfo::ptr NotificationManager::addNotification(const NotificationData& data)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::NotificationInfo>();
    info->setUserId(data.user_id);
    info->setTitle(data.title);
    info->setContent(data.content);
    info->setType(data.type);
    info->setSenderId(data.sender_id);
    info->setArticleId(data.article_id);
    info->setCommentId(data.comment_id);
    info->setIsRead(0);
    info->setReadTime(0);
    info->setIsDeleted(0);
    info->setIsBroadcast(data.is_broadcast);
    info->setLevel(data.level);
    info->setStartTime(data.start_time);
    info->setEndTime(data.end_time);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::NotificationInfoDao::Insert(info, db)) {
        ERROR(logger) << "NotificationManager addNotification Insert fail";
        return nullptr;
    }

    setNotificationCache(info);
    return info;
}

void NotificationManager::update(data::NotificationInfo::ptr info)
{
    setNotificationCache(info);
}

void NotificationManager::listByUser(
    std::vector<data::NotificationInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);

    // 结果缓存：仅对首页做缓存
    std::string typeKey = type.empty() ? "all" : type;
    std::string listKey = "notif:list:" + std::to_string(user_id) + ":" + typeKey + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (getCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    if (data::NotificationInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    std::vector<int64_t> ids;
    for (auto& info : results) {
        ids.push_back(info->getId());
    }
    cacheListResult(listKey, ids);
}

int64_t NotificationManager::countByUser(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    std::string ck = "notif:cnt:" + std::to_string(user_id) + ":" + (type.empty() ? "all" : type);
    return executeCountCached(qb, db, ck);
}

int64_t NotificationManager::unreadCount(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    return executeCountCached(qb, db, "notif:unread:" + std::to_string(user_id));
}

bool NotificationManager::markRead(int64_t notification_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = get(notification_id);
    if (!info) {
        return false;
    }

    info->setIsRead(1);
    info->setReadTime(time(0));
    if (data::NotificationInfoDao::Update(info, db)) {
        return false;
    }
    setNotificationCache(info);
    return true;
}

bool NotificationManager::markRead(const std::vector<int64_t>& ids)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    for (auto id : ids) {
        auto info = get(id);
        if (!info) {
            continue;
        }
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            setNotificationCache(info);
        }
    }
    return true;
}

int64_t NotificationManager::markAllRead(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    // Find all unread, non-deleted notifications for this user
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            setNotificationCache(info);
            count++;
        }
    }
    return count;
}

int64_t NotificationManager::markReadByType(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("type", "=", type);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            setNotificationCache(info);
            count++;
        }
    }
    return count;
}

data::NotificationInfo::ptr NotificationManager::get(int64_t id)
{
    auto v = getNotificationCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::NotificationInfoDao::Query(id, db);
    if (v) {
        setNotificationCache(v);
    }
    return v;
}

void NotificationManager::cleanupExpiredBroadcasts()
{
    auto db = GetDB();
    if (!db) {
        return;
    }
    int64_t now = time(0);
    // 将已过期的广播的 is_broadcast 置 0，前端 /broadcast/list 不再返回
    auto qb = data::NotificationInfoDao::newQuery();
    qb->set("is_broadcast", (int64_t)0);
    qb->set("update_time", now);
    qb->where("is_broadcast", "=", (int64_t)1);
    qb->whereSQL("end_time > ?", (int64_t)0);
    qb->where("end_time", "<=", now);
    auto stmt = db->prepare(qb->buildUpdateSQL());
    if (!stmt) {
        return;
    }
    qb->bindUpdateParams(stmt);
    stmt->execute();
}

} // namespace blog
