#include "message_manager.h"

#include "../util.h"
#include "cache_util.h"
#include "ws_connection_manager.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include <algorithm>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:conv:";

static std::string conversationCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::ConversationInfo::ptr parseConversationCache(const Json::Value& v)
{
    auto info = std::make_shared<data::ConversationInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserAId(cacheJsonInt64(v, "user_a_id"));
    info->setUserBId(cacheJsonInt64(v, "user_b_id"));
    info->setLastMessageId(cacheJsonInt64(v, "last_message_id"));
    info->setLastSenderId(cacheJsonInt64(v, "last_sender_id"));
    info->setLastMessage(cacheJsonString(v, "last_message"));
    info->setLastMessageTime(cacheJsonInt64(v, "last_message_time"));
    info->setUnreadA(cacheJsonInt32(v, "unread_a"));
    info->setUnreadB(cacheJsonInt32(v, "unread_b"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->setUserADeleted(cacheJsonInt32(v, "user_a_deleted"));
    info->setUserBDeleted(cacheJsonInt32(v, "user_b_deleted"));
    info->markClean();
    return info;
}

static data::ConversationInfo::ptr getConversationCache(int64_t id)
{
    std::string key = conversationCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseConversationCache(v);
}

static void setConversationCache(const data::ConversationInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(conversationCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::ConversationInfo::ptr MessageManager::get(int64_t id)
{
    auto v = getConversationCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::ConversationInfoDao::Query(id, db);
    if (v) {
        setConversationCache(v);
    }
    return v;
}

data::ConversationInfo::ptr MessageManager::getByUsers(int64_t uid_a, int64_t uid_b)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto qb = data::ConversationInfoDao::newQuery();
    qb->where("user_a_id", "=", NormalizeUserA(uid_a, uid_b));
    qb->where("user_b_id", "=", NormalizeUserB(uid_a, uid_b));
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<data::ConversationInfo::ptr> results;
    if (data::ConversationInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return nullptr;
    }
    if (results.empty()) {
        return nullptr;
    }
    setConversationCache(results[0]);
    return results[0];
}

data::ConversationInfo::ptr MessageManager::getOrCreate(int64_t uid_a, int64_t uid_b)
{
    std::lock_guard<std::mutex> lock(m_convMutex);
    auto conv = getByUsers(uid_a, uid_b);
    if (conv) {
        // 本端曾隐藏过该会话，重新发起时复活
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "Get DB connection fail";
            return nullptr;
        }
        bool need_update = false;
        if (conv->getUserAId() == uid_a && conv->getUserADeleted()) {
            conv->setUserADeleted(0);
            need_update = true;
        } else if (conv->getUserBId() == uid_a && conv->getUserBDeleted()) {
            conv->setUserBDeleted(0);
            need_update = true;
        }
        if (need_update) {
            conv->setUpdateTime(time(0));
            if (data::ConversationInfoDao::Update(conv, db) == 0) {
                setConversationCache(conv);
            }
        }
        return conv;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::ConversationInfo>();
    info->setUserAId(NormalizeUserA(uid_a, uid_b));
    info->setUserBId(NormalizeUserB(uid_a, uid_b));
    info->setLastMessageId(0);
    info->setLastSenderId(0);
    info->setLastMessage("");
    info->setLastMessageTime(0);
    info->setUnreadA(0);
    info->setUnreadB(0);
    info->setUserADeleted(0);
    info->setUserBDeleted(0);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::ConversationInfoDao::Insert(info, db)) {
        // 并发下唯一索引冲突时回查
        conv = getByUsers(uid_a, uid_b);
        if (conv) {
            return conv;
        }
        ERROR(logger) << "MessageManager getOrCreate Insert fail";
        return nullptr;
    }

    setConversationCache(info);
    return info;
}

void MessageManager::listConversations(std::vector<data::ConversationInfo::ptr>& results, int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    // 分两次查询（user_a_id / user_b_id），避免 OR 优先级问题；并按本端隐藏标志过滤
    auto qb = data::ConversationInfoDao::newQuery();
    qb->where("user_a_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("user_a_deleted", "=", (int64_t)0);
    if (data::ConversationInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }

    auto qb2 = data::ConversationInfoDao::newQuery();
    qb2->where("user_b_id", "=", uid);
    qb2->where("is_deleted", "=", (int64_t)0);
    qb2->where("user_b_deleted", "=", (int64_t)0);
    std::vector<data::ConversationInfo::ptr> part;
    if (data::ConversationInfoDao::QueryByBuilder(part, qb2, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    results.insert(results.end(), part.begin(), part.end());

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a->getLastMessageTime() > b->getLastMessageTime();
    });
}

void MessageManager::listMessages(std::vector<data::ConversationMessageInfo::ptr>& results, int64_t conversation_id, int32_t offset, int32_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    auto qb = data::ConversationMessageInfoDao::newQuery();
    qb->where("conversation_id", "=", conversation_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit(limit);
    qb->offset(offset);

    std::vector<data::ConversationMessageInfo::ptr> tmp;
    if (data::ConversationMessageInfoDao::QueryByBuilder(tmp, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    std::reverse(tmp.begin(), tmp.end());
    results.swap(tmp);
}

data::ConversationMessageInfo::ptr MessageManager::sendMessage(int64_t conversation_id, int64_t sender_id, const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_convMutex);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto msg = std::make_shared<data::ConversationMessageInfo>();
    msg->setConversationId(conversation_id);
    msg->setSenderId(sender_id);
    msg->setContent(text);
    msg->setIsRead(0);
    msg->setReadTime(0);
    msg->setIsDeleted(0);
    msg->setCreateTime(time(0));
    msg->setUpdateTime(time(0));

    if (data::ConversationMessageInfoDao::Insert(msg, db)) {
        ERROR(logger) << "MessageManager sendMessage Insert fail";
        return nullptr;
    }

    // 更新会话冗余信息与接收方未读数
    auto conv = get(conversation_id);
    if (conv) {
        conv->setLastMessageId(msg->getId());
        conv->setLastSenderId(sender_id);
        conv->setLastMessage(text);
        conv->setLastMessageTime(msg->getCreateTime());
        conv->setUpdateTime(msg->getCreateTime());
        if (sender_id == conv->getUserAId()) {
            conv->setUnreadB(conv->getUnreadB() + 1);
            // 对端曾隐藏过该会话，收到新消息时复活
            if (conv->getUserBDeleted()) {
                conv->setUserBDeleted(0);
            }
        } else {
            conv->setUnreadA(conv->getUnreadA() + 1);
            if (conv->getUserADeleted()) {
                conv->setUserADeleted(0);
            }
        }
        if (data::ConversationInfoDao::Update(conv, db) == 0) {
            setConversationCache(conv);
        }
    }

    return msg;
}

bool MessageManager::markRead(int64_t conversation_id, int64_t uid)
{
    std::lock_guard<std::mutex> lock(m_convMutex);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto conv = get(conversation_id);
    if (!conv) {
        return false;
    }
    if (uid == conv->getUserAId()) {
        if (conv->getUnreadA() != 0) {
            conv->setUnreadA(0);
        }
    } else if (uid == conv->getUserBId()) {
        if (conv->getUnreadB() != 0) {
            conv->setUnreadB(0);
        }
    } else {
        return false;
    }
    conv->setUpdateTime(time(0));
    if (data::ConversationInfoDao::Update(conv, db)) {
        return false;
    }
    setConversationCache(conv);

    // 将该会话内对方发来的未读消息置为已读
    auto qb = data::ConversationMessageInfoDao::newQuery();
    qb->set("is_read", (int64_t)1);
    qb->set("read_time", (int64_t)time(0));
    qb->set("update_time", (int64_t)time(0));
    qb->where("conversation_id", "=", conversation_id);
    qb->where("sender_id", "!=", uid);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    auto stmt = db->prepare(qb->buildUpdateSQL());
    if (!stmt) {
        return true;
    }
    qb->bindUpdateParams(stmt);
    stmt->execute();
    return true;
}

bool MessageManager::deleteForUser(int64_t conversation_id, int64_t uid)
{
    std::lock_guard<std::mutex> lock(m_convMutex);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto conv = get(conversation_id);
    if (!conv) {
        return false;
    }
    if (uid == conv->getUserAId()) {
        if (conv->getUserADeleted()) {
            return true;
        }
        conv->setUserADeleted(1);
    } else if (uid == conv->getUserBId()) {
        if (conv->getUserBDeleted()) {
            return true;
        }
        conv->setUserBDeleted(1);
    } else {
        return false;
    }
    conv->setUpdateTime(time(0));
    if (data::ConversationInfoDao::Update(conv, db)) {
        return false;
    }
    setConversationCache(conv);
    return true;
}

bool MessageManager::isChatConnected(int64_t user_id)
{
    return WSMgr::GetInstance()->isUserOnline(user_id);
}

int32_t MessageManager::sendToUser(int64_t user_id, const std::string& message)
{
    return WSMgr::GetInstance()->sendToUser(user_id, BuildChannelMessage(message, "chat"));
}

} // namespace blog
