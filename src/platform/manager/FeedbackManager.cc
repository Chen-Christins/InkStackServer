#include "platform/manager/FeedbackManager.hpp"

#include "common/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

FeedbackManager::FeedbackManager()
{
}

bool FeedbackManager::list(std::vector<data::UserFeedbackInfo::ptr>& infos, const std::string& type, int32_t page, int32_t page_size, int64_t* total)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto qb = data::UserFeedbackInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);

    if (!type.empty()) {
        qb->where("type", "=", type);
    }

    qb->orderBy("id", "DESC");

    int32_t offset = (page - 1) * page_size;
    int64_t total_cnt = 0;
    if (data::UserFeedbackInfoDao::QueryByBuilderPages(infos, total_cnt, qb, offset, page_size, db)) {
        ERROR(logger) << "QueryByBuilderPages failed";
        return false;
    }
    if (total) {
        *total = total_cnt;
    }
    return true;
}

data::UserFeedbackInfo::ptr FeedbackManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto qb = data::UserFeedbackInfoDao::newQuery();
    qb->where("id", "=", id);
    qb->where("is_deleted", "=", (int64_t)0);

    std::vector<data::UserFeedbackInfo::ptr> results;
    if (data::UserFeedbackInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    return results[0];
}

bool FeedbackManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = get(id);
    if (!info) {
        ERROR(logger) << "feedback not found: " << id;
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));

    if (data::UserFeedbackInfoDao::Update(info, db)) {
        ERROR(logger) << "soft delete feedback failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }

    return true;
}

} // namespace blog
