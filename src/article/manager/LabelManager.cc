#include "article/manager/LabelManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:lbl:";

static std::string labelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::LabelInfo::ptr parseLabelCache(const Json::Value& v)
{
    auto info = std::make_shared<data::LabelInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setName(cacheJsonString(v, "name"));
    info->setColor(cacheJsonString(v, "color"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::LabelInfo::ptr getLabelCache(int64_t id)
{
    std::string key = labelCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseLabelCache(v);
}

static void setLabelCache(const data::LabelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(labelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::LabelInfo::ptr LabelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::LabelInfoDao::ParseRow(rt);
}

void LabelManager::add(data::LabelInfo::ptr info)
{
    setLabelCache(info);
}

data::LabelInfo::ptr LabelManager::get(int64_t id)
{
    auto v = getLabelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::LabelInfoDao::Query(id, db);
    if (v) {
        setLabelCache(v);
    }
    return v;
}

data::LabelInfo::ptr LabelManager::getByUserIdName(int64_t id, const std::string& name)
{
    int64_t cachedId = getCachedIdMapping("lbl:uid_name:" + std::to_string(id) + ":" + name);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::LabelInfoDao::QueryByUserIdName(id, name, db);
    if (info) {
        setLabelCache(info);
        cacheIdMapping("lbl:uid_name:" + std::to_string(id) + ":" + name, info->getId());
    }
    return info;
}

bool LabelManager::listByUserId(std::vector<data::LabelInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::LabelInfoDao::newQuery();
    qb->where("user_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::LabelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

} // namespace blog
