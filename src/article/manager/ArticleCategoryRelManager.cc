#include "article/manager/ArticleCategoryRelManager.hpp"

#include "common/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticleCategoryRelInfoDao::ParseRow(rt);
}

data::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::ArticleCategoryRelInfoDao::Query(id, db);
}

bool ArticleCategoryRelManager::listByArticleId(std::vector<data::ArticleCategoryRelInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleCategoryRelInfoDao::newQuery();
    qb->where("article_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

bool ArticleCategoryRelManager::listByCategoryId(std::vector<data::ArticleCategoryRelInfo::ptr>& infos, int64_t category_id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleCategoryRelInfoDao::newQuery();
    qb->where("category_id", "=", category_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

data::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::getByArticleIdCategoryId(int64_t article_id, int64_t category_id)
{
    std::string ck = "acr:" + std::to_string(article_id) + ":" + std::to_string(category_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::ArticleCategoryRelInfoDao::QueryByArticleIdCategoryId(article_id, category_id, db);
    if (info) {
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

} // namespace blog
