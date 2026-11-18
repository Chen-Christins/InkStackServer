#include "platform/manager/AssignmentManager.hpp"

#include "common/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:asg:";

static std::string assignmentCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::AssignmentInfo::ptr parseAssignmentCache(const Json::Value& v)
{
    auto info = std::make_shared<data::AssignmentInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setName(cacheJsonString(v, "name"));
    info->setSubjectName(cacheJsonString(v, "subject_name"));
    info->setPriority(cacheJsonInt32(v, "priority"));
    info->setStatus(cacheJsonInt32(v, "status"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setMaxSize(cacheJsonInt32(v, "max_size"));
    info->setFileType(cacheJsonString(v, "file_type"));
    info->setDeadline(cacheJsonTime(v, "deadline"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::AssignmentInfo::ptr getAssignmentCache(int64_t id)
{
    std::string key = assignmentCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseAssignmentCache(v);
}

static void setAssignmentCache(const data::AssignmentInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(assignmentCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::AssignmentInfo::ptr AssignmentManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::AssignmentInfoDao::ParseRow(rt);
}

void AssignmentManager::add(data::AssignmentInfo::ptr info)
{
    setAssignmentCache(info);
}

data::AssignmentInfo::ptr AssignmentManager::get(int64_t id)
{
    auto v = getAssignmentCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::AssignmentInfoDao::Query(id, db);
    if (v) {
        setAssignmentCache(v);
    }
    return v;
}

data::AssignmentInfo::ptr AssignmentManager::getByName(const std::string& subject_name, const std::string& name)
{
    std::string ck = "assign:name:" + subject_name + ":" + name;
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    std::vector<data::AssignmentInfo::ptr> results;
    if (data::AssignmentInfoDao::QueryBySubjectNameName(results, subject_name, name, db)) {
        return nullptr;
    }
    if (!results.empty()) {
        setAssignmentCache(results[0]);
        cacheIdMapping(ck, results[0]->getId());
        return results[0];
    }
    return nullptr;
}

uint64_t AssignmentManager::listByPages(std::vector<data::AssignmentInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::AssignmentInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::AssignmentInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

AssignmentManager::AssignmentStats AssignmentManager::getStats()
{
    AssignmentStats stats;
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query status counts with GROUP BY
    {
        auto qb = data::AssignmentInfoDao::newQuery();
        qb->select("status, COUNT(*) AS cnt");
        qb->where("is_deleted", "=", (int64_t)0);
        qb->groupBy("status");
        std::vector<std::pair<int32_t, int64_t>> rows;
        if (qb->queryPairs<int32_t, int64_t>(rows, db) == 0) {
            for (auto& [s, cnt] : rows) {
                stats.total += cnt;
                switch (s) {
                case ACTIVE:
                    stats.active = cnt;
                    break;
                case INACTIVE:
                    stats.closed = cnt;
                    break;
                case DRAFT:
                    stats.draft = cnt;
                    break;
                default:
                    break;
                }
            }
        }
    }

    return stats;
}

} // namespace blog
