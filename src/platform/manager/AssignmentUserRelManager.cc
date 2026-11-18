#include "platform/manager/AssignmentUserRelManager.hpp"

#include <chen/log/log.h>

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::AssignmentUserRelInfo::ptr AssignmentUserRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::AssignmentUserRelInfoDao::ParseRow(rt);
}

blog::data::AssignmentUserRelInfo::ptr AssignmentUserRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::AssignmentUserRelInfoDao::Query(id, db);
}

blog::data::AssignmentUserRelInfo::ptr AssignmentUserRelManager::getByAssignAndUser(int64_t assign_id, int64_t user_id)
{
    int64_t cachedId = getCachedIdMapping("assign_usr:" + std::to_string(assign_id) + ":" + std::to_string(user_id));
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::AssignmentUserRelInfoDao::QueryByAssignmentIdUserId(assign_id, user_id, db);
    if (info) {
        cacheIdMapping("assign_usr:" + std::to_string(assign_id) + ":" + std::to_string(user_id), info->getId());
    }
    return info;
}

} // namespace blog
