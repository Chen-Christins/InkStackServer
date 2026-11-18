#include "platform/manager/AssignmentOrganizationRelManager.hpp"

#include <chen/log/log.h>

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::AssignmentOrganizationRelInfoDao::ParseRow(rt);
}

blog::data::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::AssignmentOrganizationRelInfoDao::Query(id, db);
}

blog::data::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelManager::getByOrgAndAssign(int64_t org_id, int64_t assign_id)
{
    int64_t cachedId = getCachedIdMapping("assign_org:" + std::to_string(org_id) + ":" + std::to_string(assign_id));
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::AssignmentOrganizationRelInfoDao::QueryByAssignmentIdOrganizationId(assign_id, org_id, db);
    if (info) {
        cacheIdMapping("assign_org:" + std::to_string(org_id) + ":" + std::to_string(assign_id), info->getId());
    }
    return info;
}

int64_t AssignmentOrganizationRelManager::getByAssignmentId(std::vector<data::AssignmentOrganizationRelInfo::ptr>& results, int64_t assign_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::AssignmentOrganizationRelInfoDao::newQuery();
    qb->where("assignment_id", "=", assign_id);
    qb->orderBy("id", "DESC");
    if (data::AssignmentOrganizationRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }
    return results.size();
}

int64_t AssignmentOrganizationRelManager::getByPages(
    std::vector<data::AssignmentOrganizationRelInfo::ptr>& results,
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
    auto qb = data::AssignmentOrganizationRelInfoDao::newQuery();
    qb->where("organization_id", "=", o_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::AssignmentOrganizationRelInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)size, db)) {
        ERROR(logger) << "QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

} // namespace blog
