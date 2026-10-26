#include "category_manager.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include "../util.h"
#include "cache_util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:cat2:";

static std::string categoryCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::CategoryInfo::ptr parseCategoryCache(const Json::Value& v)
{
    auto info = std::make_shared<data::CategoryInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setName(cacheJsonString(v, "name"));
    info->setColor(cacheJsonString(v, "color"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setUrl(cacheJsonString(v, "url"));
    info->setIcon(cacheJsonString(v, "icon"));
    info->setParentId(cacheJsonInt64(v, "parent_id"));
    info->setStatus(cacheJsonInt32(v, "status"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    // 缓存中的数据视为与 DB 一致，避免写回时全字段 UPDATE
    info->markClean();
    return info;
}

static data::CategoryInfo::ptr getCategoryCache(int64_t id)
{
    std::string key = categoryCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseCategoryCache(v);
}

static void setCategoryCache(const data::CategoryInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(categoryCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::CategoryInfo::ptr CategoryManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::CategoryInfoDao::ParseRow(rt);
}

void CategoryManager::add(blog::data::CategoryInfo::ptr info)
{
    setCategoryCache(info);
}

blog::data::CategoryInfo::ptr CategoryManager::get(int64_t id)
{
    auto v = getCategoryCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::CategoryInfoDao::Query(id, db);
    if (v) {
        setCategoryCache(v);
    }
    return v;
}

void CategoryManager::listAll(std::vector<blog::data::CategoryInfo::ptr>& infos, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::CategoryInfoDao::newQuery();
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    if (data::CategoryInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

bool CategoryManager::listByUserId(std::vector<blog::data::CategoryInfo::ptr>& infos, int64_t user_id, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::CategoryInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    if (data::CategoryInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

blog::data::CategoryInfo::ptr CategoryManager::getByUserIdName(int64_t user_id, const std::string& name)
{
    std::string cache_key = "cat:uid_name:" + std::to_string(user_id) + ":" + name;
    int64_t cachedId = getCachedIdMapping(cache_key);
    if (cachedId > 0) {
        auto cached = get(cachedId);
        // 改名后旧映射可能仍在缓存中，校验归属和名称，避免命中脏数据
        if (cached && cached->getUserId() == user_id && cached->getName() == name) {
            return cached;
        }
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::CategoryInfoDao::QueryByUserIdName(user_id, name, db);
    if (info) {
        setCategoryCache(info);
        cacheIdMapping(cache_key, info->getId());
    }
    return info;
}

} // namespace blog
