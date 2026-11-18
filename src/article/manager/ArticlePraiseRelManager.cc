#include "article/manager/ArticlePraiseRelManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:apra:";

static std::string articlePraiseRelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::ArticlePraiseRelInfo::ptr parseArticlePraiseRelCache(const Json::Value& v)
{
    auto info = std::make_shared<data::ArticlePraiseRelInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setArticleId(cacheJsonInt64(v, "article_id"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::ArticlePraiseRelInfo::ptr getArticlePraiseRelCache(int64_t id)
{
    std::string key = articlePraiseRelCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseArticlePraiseRelCache(v);
}

static void setArticlePraiseRelCache(const data::ArticlePraiseRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(articlePraiseRelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticlePraiseRelInfoDao::ParseRow(rt);
}

void ArticlePraiseRelManager::add(data::ArticlePraiseRelInfo::ptr info)
{
    setArticlePraiseRelCache(info);
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::get(int64_t id)
{
    auto v = getArticlePraiseRelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::ArticlePraiseRelInfoDao::Query(id, db);
    if (v) {
        setArticlePraiseRelCache(v);
    }
    return v;
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::getByUserAndArticle(int64_t user_id, int64_t article_id)
{
    std::string ck = "pra:" + std::to_string(user_id) + ":" + std::to_string(article_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::ArticlePraiseRelInfoDao::QueryByUserIdArticleId(user_id, article_id, db);
    if (info) {
        setArticlePraiseRelCache(info);
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::praise(int64_t user_id, int64_t article_id)
{
    auto existing = getByUserAndArticle(user_id, article_id);
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "Get DB connection fail";
            return nullptr;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(time(0));
        if (data::ArticlePraiseRelInfoDao::Update(existing, db)) {
            ERROR(logger) << "ArticlePraiseRelManager praise Update fail";
            return nullptr;
        }
        setArticlePraiseRelCache(existing);
        return existing;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::ArticlePraiseRelInfo>();
    info->setUserId(user_id);
    info->setArticleId(article_id);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::ArticlePraiseRelInfoDao::Insert(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager praise Insert fail";
        return nullptr;
    }

    setArticlePraiseRelCache(info);
    return info;
}

bool ArticlePraiseRelManager::unpraise(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));
    if (data::ArticlePraiseRelInfoDao::Update(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager unpraise Update fail";
        return false;
    }
    setArticlePraiseRelCache(info);
    return true;
}

bool ArticlePraiseRelManager::isPraising(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    return info && !info->getIsDeleted();
}

void ArticlePraiseRelManager::listByArticle(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t article_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

void ArticlePraiseRelManager::listByUser(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

int64_t ArticlePraiseRelManager::countByArticle(int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

int64_t ArticlePraiseRelManager::countByUser(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

} // namespace blog
