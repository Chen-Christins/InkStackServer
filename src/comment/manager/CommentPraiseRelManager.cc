#include "comment/manager/CommentPraiseRelManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:cpra:";

static std::string commentPraiseRelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::CommentPraiseRelInfo::ptr parseCommentPraiseRelCache(const Json::Value& v)
{
    auto info = std::make_shared<data::CommentPraiseRelInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setCommentId(cacheJsonInt64(v, "comment_id"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::CommentPraiseRelInfo::ptr getCommentPraiseRelCache(int64_t id)
{
    std::string key = commentPraiseRelCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseCommentPraiseRelCache(v);
}

static void setCommentPraiseRelCache(const data::CommentPraiseRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(commentPraiseRelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::CommentPraiseRelInfo::ptr CommentPraiseRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::CommentPraiseRelInfoDao::ParseRow(rt);
}

void CommentPraiseRelManager::add(data::CommentPraiseRelInfo::ptr info)
{
    setCommentPraiseRelCache(info);
}

data::CommentPraiseRelInfo::ptr CommentPraiseRelManager::get(int64_t id)
{
    auto v = getCommentPraiseRelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::CommentPraiseRelInfoDao::Query(id, db);
    if (v) {
        setCommentPraiseRelCache(v);
    }
    return v;
}

data::CommentPraiseRelInfo::ptr CommentPraiseRelManager::getByUserAndComment(int64_t user_id, int64_t comment_id)
{
    std::string ck = "cpra:" + std::to_string(user_id) + ":" + std::to_string(comment_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::CommentPraiseRelInfoDao::QueryByUserIdCommentId(user_id, comment_id, db);
    if (info) {
        setCommentPraiseRelCache(info);
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

data::CommentPraiseRelInfo::ptr CommentPraiseRelManager::praise(int64_t user_id, int64_t comment_id)
{
    auto existing = getByUserAndComment(user_id, comment_id);
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
        if (data::CommentPraiseRelInfoDao::Update(existing, db)) {
            ERROR(logger) << "CommentPraiseRelManager praise Update fail";
            return nullptr;
        }
        setCommentPraiseRelCache(existing);
        return existing;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::CommentPraiseRelInfo>();
    info->setUserId(user_id);
    info->setCommentId(comment_id);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::CommentPraiseRelInfoDao::Insert(info, db)) {
        ERROR(logger) << "CommentPraiseRelManager praise Insert fail";
        return nullptr;
    }

    setCommentPraiseRelCache(info);
    return info;
}

bool CommentPraiseRelManager::unpraise(int64_t user_id, int64_t comment_id)
{
    auto info = getByUserAndComment(user_id, comment_id);
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
    if (data::CommentPraiseRelInfoDao::Update(info, db)) {
        ERROR(logger) << "CommentPraiseRelManager unpraise Update fail";
        return false;
    }
    setCommentPraiseRelCache(info);
    return true;
}

bool CommentPraiseRelManager::isPraising(int64_t user_id, int64_t comment_id)
{
    auto info = getByUserAndComment(user_id, comment_id);
    return info && !info->getIsDeleted();
}

int64_t CommentPraiseRelManager::countByComment(int64_t comment_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::CommentPraiseRelInfoDao::newQuery();
    qb->where("comment_id", "=", comment_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

} // namespace blog
