#include "platform/manager/OrganizationUserRelManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"
#include "platform/manager/OrganizationManager.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:org_usr:";

static std::string orgUserRelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::OrganizationUserRelInfo::ptr parseOrgUserRelCache(const Json::Value& v)
{
    auto info = std::make_shared<data::OrganizationUserRelInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setOrgId(cacheJsonInt64(v, "org_id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setRole(cacheJsonInt32(v, "role"));
    info->setStatus(cacheJsonInt32(v, "status"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::OrganizationUserRelInfo::ptr getOrgUserRelCache(int64_t id)
{
    std::string key = orgUserRelCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseOrgUserRelCache(v);
}

static void setOrgUserRelCache(const data::OrganizationUserRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(orgUserRelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationUserRelInfoDao::ParseRow(rt);
}

void OrganizationUserRelManager::add(data::OrganizationUserRelInfo::ptr info)
{
    setOrgUserRelCache(info);
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::get(int64_t id)
{
    auto v = getOrgUserRelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::OrganizationUserRelInfoDao::Query(id, db);
    if (v) {
        setOrgUserRelCache(v);
    }
    return v;
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::getByOrgAndUser(int64_t o_id, int64_t u_id)
{
    int64_t cachedId = getCachedIdMapping("org_usr:" + std::to_string(o_id) + ":" + std::to_string(u_id));
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationUserRelInfoDao::QueryByOrgIdUserId(o_id, u_id, db);
    if (info) {
        setOrgUserRelCache(info);
        cacheIdMapping("org_usr:" + std::to_string(o_id) + ":" + std::to_string(u_id), info->getId());
    }
    return info;
}

int64_t OrganizationUserRelManager::getByPages(
    std::vector<data::OrganizationUserRelInfo::ptr>& results,
    int64_t o_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("org_id", "=", o_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::OrganizationUserRelInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

int64_t OrganizationUserRelManager::getOrgByUserId(
    std::vector<data::OrganizationUserRelInfo::ptr>& results,
    int64_t u_id,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("user_id", "=", u_id);
    qb->where("status", "!=", (int64_t)Status::PENDING);
    qb->where("status", "!=", (int64_t)Status::EXITED);
    qb->where("status", "!=", (int64_t)Status::REJECTED);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::OrganizationUserRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }
    return results.size();
}

int64_t OrganizationUserRelManager::getMemberCount(int64_t o_id, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("org_id", "=", o_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

OrganizationUserRelManager::Stats OrganizationUserRelManager::getStats(int64_t org_id)
{
    Stats stats;
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // total_members: 所有已批准且未删除的成员
    stats.total_members = getMemberCount(org_id, Status::APPROVED, true);

    // 按角色 GROUP BY 拿到 org_admin_count 和 regular_count
    {
        auto qb = data::OrganizationUserRelInfoDao::newQuery();
        qb->select("role, COUNT(*) as cnt");
        qb->where("org_id", "=", org_id);
        qb->where("status", "=", (int64_t)Status::APPROVED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->groupBy("role");
        std::vector<std::pair<int32_t, int64_t>> rows;
        if (qb->queryPairs<int32_t, int64_t>(rows, db) == 0) {
            for (auto& [role, cnt] : rows) {
                if (role == OrganizationManager::Role::ORG_ADMIN) {
                    stats.org_admin_count = cnt;
                } else if (role == OrganizationManager::Role::MEMBER) {
                    stats.regular_count = cnt;
                }
            }
        }
    }

    // active_members: 已批准成员中，其用户账号状态为正常的数量
    {
        auto qb = data::OrganizationUserRelInfoDao::newQuery("r");
        qb->join("INNER", "user u", "r.user_id = u.id");
        qb->where("r.org_id", "=", org_id);
        qb->where("r.status", "=", (int64_t)Status::APPROVED);
        qb->where("r.is_deleted", "=", (int64_t)0);
        qb->where("u.state", "=", (int64_t)1);
        qb->executeCount(stats.active_members, db);
    }

    return stats;
}

} // namespace blog
