#include "platform/manager/OrganizationManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:org:";

static std::string organizationCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::OrganizationInfo::ptr parseOrganizationCache(const Json::Value& v)
{
    auto info = std::make_shared<data::OrganizationInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setName(cacheJsonString(v, "name"));
    info->setType(cacheJsonString(v, "type"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setOwnerId(cacheJsonInt64(v, "owner_id"));
    info->setStatus(cacheJsonInt32(v, "status"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::OrganizationInfo::ptr getOrganizationCache(int64_t id)
{
    std::string key = organizationCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseOrganizationCache(v);
}

static void setOrganizationCache(const data::OrganizationInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(organizationCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::OrganizationInfo::ptr OrganizationManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationInfoDao::ParseRow(rt);
}

void OrganizationManager::add(data::OrganizationInfo::ptr info)
{
    setOrganizationCache(info);
}

data::OrganizationInfo::ptr OrganizationManager::get(int64_t id)
{
    auto v = getOrganizationCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::OrganizationInfoDao::Query(id, db);
    if (v) {
        setOrganizationCache(v);
    }
    return v;
}

data::OrganizationInfo::ptr OrganizationManager::getByName(const std::string& name)
{
    int64_t cachedId = getCachedIdMapping("org:name:" + name);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationInfoDao::QueryByName(name, db);
    if (info) {
        setOrganizationCache(info);
        cacheIdMapping("org:name:" + name, info->getId());
    }
    return info;
}

int64_t OrganizationManager::listByPages(
    std::vector<data::OrganizationInfo::ptr>& orgs,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::OrganizationInfoDao::QueryByBuilderPages(orgs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    return total;
}

OrganizationManager::OrganizationStats OrganizationManager::getStats()
{
    OrganizationStats stats;

    std::string cached;
    if (getCachedStringResult("org:stats", cached)) {
        std::stringstream ss(cached);
        std::string token;
        auto next = [&]() -> int64_t {
            std::getline(ss, token, '|');
            return chen::TypeUtil::Atoi(token);
        };
        stats.total = next();
        stats.active = next();
        stats.inactive = next();
        return stats;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query total (non-deleted)
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        int64_t total = 0;
        if (qb->executeCount(total, db) == 0) {
            stats.total = total;
        }
    }

    // Query active
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::ACTIVE);
        int64_t active = 0;
        if (qb->executeCount(active, db) == 0) {
            stats.active = active;
        }
    }

    // Query inactive
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::INACTIVE);
        int64_t inactive = 0;
        if (qb->executeCount(inactive, db) == 0) {
            stats.inactive = inactive;
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.active << "|" << stats.inactive;
    cacheStringResult("org:stats", ss.str());

    return stats;
}

} // namespace blog
