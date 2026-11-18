#include "article/manager/ArticleLabelRelManager.hpp"

#include "common/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticleLabelRelInfoDao::ParseRow(rt);
}

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::ArticleLabelRelInfoDao::Query(id, db);
}

bool ArticleLabelRelManager::listByArticleId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleLabelRelInfoDao::newQuery();
    qb->where("article_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

bool ArticleLabelRelManager::listByLabelId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t label_id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleLabelRelInfoDao::newQuery();
    qb->where("label_id", "=", label_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::getByArticleIdLabelId(int64_t article_id, int64_t label_id)
{
    std::string ck = "alr:" + std::to_string(article_id) + ":" + std::to_string(label_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::ArticleLabelRelInfoDao::QueryByArticleIdLabelId(article_id, label_id, db);
    if (info) {
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

} // namespace blog
