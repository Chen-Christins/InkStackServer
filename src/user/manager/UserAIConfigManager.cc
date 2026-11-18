/**
 * @file UserAIConfigManager.cc
 * @brief 用户AI配置管理器实现
 * @author Christins
 * @date 2026-06-09
 * @copyright Apache 2.0
 */
#include "user/manager/UserAIConfigManager.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include "common/Util.hpp"
#include "common/manager/CacheUtil.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:ai_cfg:";

static std::string aiConfigCacheKey(int64_t user_id)
{
    return std::string(kCachePrefix) + std::to_string(user_id);
}

static data::UserAiConfigInfo::ptr parseAiConfigCache(const Json::Value& v)
{
    auto info = std::make_shared<data::UserAiConfigInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setType(cacheJsonString(v, "type"));
    info->setUrl(cacheJsonString(v, "url"));
    info->setApiKey(cacheJsonString(v, "api_key"));
    info->setModel(cacheJsonString(v, "model"));
    info->setMaxTokens(cacheJsonInt32(v, "max_tokens"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::UserAiConfigInfo::ptr getAiConfigCache(int64_t user_id)
{
    std::string key = aiConfigCacheKey(user_id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseAiConfigCache(v);
}

static void setAiConfigCache(const data::UserAiConfigInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(aiConfigCacheKey(info->getUserId()), info->toJsonString(), kCacheTTL);
}

blog::data::UserAiConfigInfo::ptr UserAIConfigManager::getByUserId(int64_t user_id)
{
    auto v = getAiConfigCache(user_id);
    if (v) {
        return v;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    v = data::UserAiConfigInfoDao::QueryByUserId(user_id, db);
    if (v) {
        setAiConfigCache(v);
    }
    return v;
}

bool UserAIConfigManager::save(blog::data::UserAiConfigInfo::ptr info)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setUpdateTime(time(0));
    if (!info->getId()) {
        info->setCreateTime(time(0));
    }

    if (data::UserAiConfigInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "UserAIConfigManager save fail";
        return false;
    }

    // save 之后需要重新查询以获取 DB 生成的 id
    auto saved = data::UserAiConfigInfoDao::QueryByUserId(info->getUserId(), db);
    if (saved) {
        setAiConfigCache(saved);
    }
    return true;
}

} // namespace blog
