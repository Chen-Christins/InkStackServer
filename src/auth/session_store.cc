#include "session_store.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>
#include <chen/util/encryptor_util.h>
#include <chen/util/string_util.h>
#include <chen/util/time_util.h>
#include <chen/util/type_util.h>

namespace blog {

// SDK 的 SessionDataMgr 只做进程内缓存，不提供 Redis 持久化；
// 这里用 Redis 保存服务端会话，Cookie 只保存随机 SESSIONID。
static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// 会话 key 前缀
static const char* kSessionKeyPrefix = "auth:sess:";
/// 用户会话集合 key 前缀
static const char* kUserSessionKeyPrefix = "auth:uid:";

static std::string sessionKey(const std::string& hash)
{
    return std::string(kSessionKeyPrefix) + hash;
}

static std::string userSessionKey(int64_t uid)
{
    return std::string(kUserSessionKeyPrefix) + std::to_string(uid);
}

static bool parseSessionReply(const redisReply* rpy, SessionInfo& info)
{
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY) {
        return false;
    }
    try {
        for (size_t i = 0; i + 1 < rpy->elements; i += 2) {
            const redisReply* key = rpy->element[i];
            const redisReply* val = rpy->element[i + 1];
            if (!key || !key->str || !val || !val->str) {
                continue;
            }
            std::string field(key->str, key->len);
            std::string value(val->str, val->len);
            if (field == "uid") {
                info.uid = chen::TypeUtil::Atoi(value);
            } else if (field == "ip") {
                info.ip = value;
            } else if (field == "ua") {
                info.user_agent = value;
            } else if (field == "created_at") {
                info.created_at = chen::TypeUtil::Atoi(value);
            } else if (field == "last_seen") {
                info.last_seen = chen::TypeUtil::Atoi(value);
            } else if (field == "expires_at") {
                info.expires_at = chen::TypeUtil::Atoi(value);
            }
        }
    } catch (...) {
        return false;
    }
    return info.uid > 0;
}

std::string SessionStore::HashId(const std::string& session_id)
{
    return chen::EncryptorUtil::SHA256(session_id);
}

std::string SessionStore::Create(int64_t uid, const std::string& ip, const std::string& user_agent, int64_t idle_ttl, int64_t absolute_ttl)
{
    if (!uid) {
        return "";
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    int64_t idle = idle_ttl > 0 ? idle_ttl : 24 * 3600;
    int64_t expires_at = now + idle;
    if (absolute_ttl > 0 && expires_at > now + absolute_ttl) {
        expires_at = now + absolute_ttl;
    }

    std::string random;
    try {
        random = chen::EncryptorUtil::GenerateRandomKey(32);
    } catch (...) {
        ERROR(logger) << "generate session random failed";
        return "";
    }
    std::string session_id = chen::StringUtil::HexEncode(random, false);
    if (session_id.empty()) {
        ERROR(logger) << "generate session id failed";
        return "";
    }

    std::string hash = HashId(session_id);
    std::string key = sessionKey(hash);
    std::string user_key = userSessionKey(uid);

    auto rpy = chen::RedisUtil::Cmd(
        g_redis_pool_name->getValue(), "hmset %s uid %lld ip %s ua %s created_at %lld last_seen %lld expires_at %lld", key.c_str(), uid, ip.c_str(),
        user_agent.c_str(), now, now, expires_at);
    if (!rpy) {
        ERROR(logger) << "create session failed uid=" << uid;
        return "";
    }

    int64_t ttl = expires_at - now;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", key.c_str(), ttl);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zadd %s %lld %s", user_key.c_str(), expires_at, hash.c_str());
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", user_key.c_str(), (absolute_ttl > 0 ? absolute_ttl : idle));

    return session_id;
}

bool SessionStore::Get(const std::string& session_id, SessionInfo& info)
{
    if (session_id.empty()) {
        return false;
    }

    std::string hash = HashId(session_id);
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hgetall %s", sessionKey(hash).c_str());
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY || rpy->elements == 0) {
        return false;
    }

    info = SessionInfo();
    info.id = hash;
    if (!parseSessionReply(rpy.get(), info)) {
        return false;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (info.expires_at > 0 && info.expires_at <= now) {
        DestroyByHash(hash, info.uid);
        return false;
    }
    return true;
}

bool SessionStore::Touch(const std::string& session_id, const SessionInfo& info, int64_t idle_ttl, int64_t absolute_ttl)
{
    if (session_id.empty() || !info.uid) {
        return false;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    int64_t idle = idle_ttl > 0 ? idle_ttl : 24 * 3600;
    int64_t expires_at = now + idle;
    if (absolute_ttl > 0 && info.created_at > 0 && expires_at > info.created_at + absolute_ttl) {
        expires_at = info.created_at + absolute_ttl;
    }
    if (expires_at <= now) {
        DestroyByHash(info.id, info.uid);
        return false;
    }

    std::string hash = info.id.empty() ? HashId(session_id) : info.id;
    std::string key = sessionKey(hash);
    std::string user_key = userSessionKey(info.uid);
    int64_t ttl = expires_at - now;

    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hmset %s last_seen %lld expires_at %lld", key.c_str(), now, expires_at);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", key.c_str(), ttl);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zadd %s %lld %s", user_key.c_str(), expires_at, hash.c_str());
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", user_key.c_str(), (absolute_ttl > 0 ? absolute_ttl : idle));
    return true;
}

void SessionStore::Destroy(const std::string& session_id)
{
    if (session_id.empty()) {
        return;
    }

    std::string hash = HashId(session_id);
    SessionInfo info;
    if (Get(session_id, info)) {
        DestroyByHash(hash, info.uid);
        return;
    }
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", sessionKey(hash).c_str());
}

void SessionStore::DestroyByHash(const std::string& hash, int64_t uid)
{
    if (hash.empty()) {
        return;
    }

    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", sessionKey(hash).c_str());
    if (uid > 0) {
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrem %s %s", userSessionKey(uid).c_str(), hash.c_str());
    }
}

void SessionStore::DestroyAll(int64_t uid)
{
    if (!uid) {
        return;
    }

    std::string user_key = userSessionKey(uid);
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrange %s 0 -1", user_key.c_str());
    if (rpy && rpy->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < rpy->elements; ++i) {
            const redisReply* item = rpy->element[i];
            if (!item || !item->str) {
                continue;
            }
            std::string hash(item->str, item->len);
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", sessionKey(hash).c_str());
        }
    }
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", user_key.c_str());
}

bool SessionStore::DestroyForUser(int64_t uid, const std::string& hash)
{
    if (!uid || hash.empty()) {
        return false;
    }

    std::string user_key = userSessionKey(uid);
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zscore %s %s", user_key.c_str(), hash.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING) {
        return false;
    }

    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", sessionKey(hash).c_str());
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrem %s %s", user_key.c_str(), hash.c_str());
    return true;
}

void SessionStore::List(int64_t uid, std::vector<SessionInfo>& sessions)
{
    sessions.clear();
    if (!uid) {
        return;
    }

    std::string user_key = userSessionKey(uid);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zremrangebyscore %s 0 %lld", user_key.c_str(), now);

    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrange %s 0 -1", user_key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY) {
        return;
    }

    for (size_t i = 0; i < rpy->elements; ++i) {
        const redisReply* item = rpy->element[i];
        if (!item || !item->str) {
            continue;
        }
        std::string hash(item->str, item->len);
        auto detail = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hgetall %s", sessionKey(hash).c_str());
        if (!detail || detail->type != REDIS_REPLY_ARRAY || detail->elements == 0) {
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrem %s %s", user_key.c_str(), hash.c_str());
            continue;
        }
        SessionInfo info;
        info.id = hash;
        if (parseSessionReply(detail.get(), info) && info.expires_at > now) {
            sessions.push_back(info);
        }
    }
}

} // namespace blog
