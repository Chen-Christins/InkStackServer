#include "common/manager/CacheUtil.hpp"

#include <chen/config/config.h>
#include <chen/db/query_builder.h>
#include <chen/db/redis.h>
#include <chen/util/time_util.h>
#include <chen/util/type_util.h>
#include <chen/util/util.h>

namespace blog {

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// 对象缓存开关：Redis 故障或本地调试时可通过 cache.obj.enabled=false 关闭
static chen::ConfigVar<bool>::ptr g_obj_cache_enabled = chen::Config::Lookup("cache.obj.enabled", true, "enable redis object cache");

int64_t executeCountCached(chen::QueryBuilder::ptr qb, chen::IDB::ptr conn, const std::string& cacheKey, int ttlSec)
{
    std::string redisKey = "cache:count:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), ttlSec, total);
    return total;
}

void cacheListResult(const std::string& cacheKey, const std::vector<int64_t>& ids, int ttlSec)
{
    if (ids.empty()) {
        return;
    }
    std::stringstream ss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) {
            ss << ",";
        }
        ss << ids[i];
    }
    std::string redisKey = "cache:list:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", redisKey.c_str(), ttlSec, ss.str().c_str());
}

bool getCachedListResult(const std::string& cacheKey, std::vector<int64_t>& ids)
{
    std::string redisKey = "cache:list:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    std::string data(rpy->str);
    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find(',', pos);
        if (end == std::string::npos) {
            end = data.size();
        }
        ids.push_back(chen::TypeUtil::Atoi(data.substr(pos, end - pos)));
        pos = end + 1;
    }
    return true;
}

void cacheStringResult(const std::string& cacheKey, const std::string& value, int ttlSec)
{
    std::string redisKey = "cache:str:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", redisKey.c_str(), ttlSec, value.c_str());
}

bool getCachedStringResult(const std::string& cacheKey, std::string& value)
{
    std::string redisKey = "cache:str:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    value = rpy->str;
    return true;
}

void cacheIdMapping(const std::string& cacheKey, int64_t id, int ttlSec)
{
    std::string redisKey = "cache:map:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), ttlSec, id);
}

int64_t getCachedIdMapping(const std::string& cacheKey)
{
    std::string redisKey = "cache:map:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

bool isObjectCacheEnabled()
{
    return g_obj_cache_enabled && g_obj_cache_enabled->getValue();
}

std::string redisCacheGet(const std::string& key)
{
    if (!isObjectCacheEnabled()) {
        return std::string();
    }
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
        return std::string();
    }
    return std::string(rpy->str, rpy->len);
}

bool redisCacheSetex(const std::string& key, const std::string& value, int ttlSec)
{
    if (!isObjectCacheEnabled()) {
        return false;
    }
    chen::ReplyPtr rpy;
    if (ttlSec > 0) {
        rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", key.c_str(), ttlSec, value.c_str());
    } else {
        rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "set %s %s", key.c_str(), value.c_str());
    }
    return rpy && rpy->type == REDIS_REPLY_STATUS;
}

bool redisCacheDel(const std::string& key)
{
    if (!isObjectCacheEnabled()) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.c_str());
    return rpy && rpy->type == REDIS_REPLY_INTEGER && rpy->integer > 0;
}

bool redisCacheExists(const std::string& key)
{
    if (!isObjectCacheEnabled()) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "exists %s", key.c_str());
    return rpy && rpy->type == REDIS_REPLY_INTEGER && rpy->integer > 0;
}

int64_t cacheJsonInt64(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return chen::TypeUtil::Atoi(field.asString());
    }
    if (field.isBool()) {
        return field.asBool() ? 1 : 0;
    }
    if (field.isInt64() || field.isInt() || field.isUInt() || field.isUInt64()) {
        return field.asInt64();
    }
    if (field.isDouble()) {
        return static_cast<int64_t>(field.asDouble());
    }
    return 0;
}

int32_t cacheJsonInt32(const Json::Value& v, const char* key)
{
    return static_cast<int32_t>(cacheJsonInt64(v, key));
}

std::string cacheJsonString(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return field.asString();
    }
    if (field.isBool()) {
        return field.asBool() ? "1" : "0";
    }
    if (field.isInt64() || field.isInt() || field.isUInt() || field.isUInt64()) {
        return std::to_string(field.asInt64());
    }
    return std::string();
}

int64_t cacheJsonTime(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return chen::TimeUtil::Str2Time(field.asString());
    }
    return cacheJsonInt64(v, key);
}

} // namespace blog
