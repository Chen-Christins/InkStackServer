#include "platform/manager/TaskManager.hpp"

#include "../../common/util/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::TaskInfo::ptr TaskManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::TaskInfoDao::ParseRow(rt);
}

data::TaskInfo::ptr TaskManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::TaskInfoDao::Query(id, db);
}

uint64_t TaskManager::listByPages(std::vector<data::TaskInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::TaskInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::TaskInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

uint64_t TaskManager::listByOrg(std::vector<data::TaskInfo::ptr>& infos, int64_t orgId, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::TaskInfoDao::newQuery();
    qb->where("org_id", "=", orgId);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::TaskInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}
} // namespace blog
