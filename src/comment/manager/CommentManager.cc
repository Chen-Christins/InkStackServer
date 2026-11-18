#include "comment/manager/CommentManager.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:cmt:";

static std::string commentCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::CommentInfo::ptr parseCommentCache(const Json::Value& v)
{
    auto info = std::make_shared<data::CommentInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setArticleId(cacheJsonInt64(v, "article_id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setParentId(cacheJsonInt64(v, "parent_id"));
    info->setContent(cacheJsonString(v, "content"));
    info->setIp(cacheJsonString(v, "ip"));
    info->setUserAgent(cacheJsonString(v, "user_agent"));
    info->setStatus(cacheJsonInt32(v, "status"));
    info->setIsReported(cacheJsonInt32(v, "is_reported"));
    info->setReportCount(cacheJsonInt32(v, "report_count"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::CommentInfo::ptr getCommentCache(int64_t id)
{
    std::string key = commentCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseCommentCache(v);
}

static void setCommentCache(const data::CommentInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(commentCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::CommentInfo::ptr CommentManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::CommentInfoDao::ParseRow(rt);
}

void CommentManager::add(data::CommentInfo::ptr info)
{
    setCommentCache(info);
}

data::CommentInfo::ptr CommentManager::get(int64_t id)
{
    auto v = getCommentCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::CommentInfoDao::Query(id, db);
    if (v) {
        setCommentCache(v);
    }
    return v;
}

data::CommentInfo::ptr CommentManager::create(
    int64_t article_id,
    int64_t user_id,
    const std::string& content,
    int64_t parent_id,
    const std::string& ip,
    const std::string& user_agent)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::CommentInfo>();
    info->setArticleId(article_id);
    info->setUserId(user_id);
    info->setContent(content);
    info->setParentId(parent_id);
    info->setIp(ip);
    info->setUserAgent(user_agent);
    info->setStatus(APPROVED);
    info->setIsReported(0);
    info->setReportCount(0);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::CommentInfoDao::Insert(info, db)) {
        ERROR(logger) << "CommentManager create Insert fail";
        return nullptr;
    }

    setCommentCache(info);
    return info;
}

bool CommentManager::update(int64_t id, const std::string& content)
{
    auto info = get(id);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setContent(content);
    info->setUpdateTime(time(0));
    if (data::CommentInfoDao::Update(info, db)) {
        ERROR(logger) << "CommentManager update fail";
        return false;
    }
    setCommentCache(info);
    return true;
}

bool CommentManager::del(int64_t id)
{
    auto info = get(id);
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
    if (data::CommentInfoDao::Update(info, db)) {
        ERROR(logger) << "CommentManager del Update fail";
        return false;
    }
    setCommentCache(info);
    return true;
}

void CommentManager::listAllByArticle(std::vector<data::CommentInfo::ptr>& results, int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)APPROVED);
    qb->orderBy("id", "ASC");
    if (data::CommentInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

void CommentManager::listByArticle(std::vector<data::CommentInfo::ptr>& results, int64_t article_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("parent_id", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)APPROVED);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);

    // 结果缓存：仅对首页做缓存
    std::string listKey = "cmt:art:" + std::to_string(article_id) + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (getCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    if (data::CommentInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    std::vector<int64_t> ids;
    for (auto& info : results) {
        ids.push_back(info->getId());
    }
    cacheListResult(listKey, ids);
}

void CommentManager::listReplies(std::vector<data::CommentInfo::ptr>& results, int64_t parent_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("parent_id", "=", parent_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)APPROVED);
    qb->orderBy("id", "ASC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::CommentInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

int64_t CommentManager::countByArticle(int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("parent_id", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)APPROVED);
    return executeCountCached(qb, db, "cmt:cnt:" + std::to_string(article_id));
}

int64_t CommentManager::countReplies(int64_t parent_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("parent_id", "=", parent_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)APPROVED);
    return executeCountCached(qb, db, "cmt:reply:" + std::to_string(parent_id));
}

int64_t CommentManager::listByAdmin(
    std::vector<data::CommentInfo::ptr>& results,
    int64_t page_num,
    int64_t page_size,
    int32_t status,
    const std::string& keyword,
    int64_t article_id,
    int32_t is_reported)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::CommentInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    qb->whereIf(status > 0, "status", "=", (int64_t)status);
    qb->whereIf(article_id > 0, "article_id", "=", article_id);
    qb->whereIf(is_reported >= 0, "is_reported", "=", (int64_t)is_reported);
    qb->whereIf(!keyword.empty(), "content", "LIKE", "%" + keyword + "%");
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    int32_t offset = (int32_t)((page_num - 1) * page_size);
    if (data::CommentInfoDao::QueryByBuilderPages(results, total, qb, offset, (int32_t)page_size, db)) {
        ERROR(logger) << "listByAdmin QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

int64_t CommentManager::batchUpdateStatus(const std::vector<int64_t>& ids, int32_t status)
{
    if (ids.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::CommentInfoDao::newQuery();
    qb->set("status", (int64_t)status);
    qb->whereIn("id", ids);
    if (qb->executeUpdate(db)) {
        ERROR(logger) << "batchUpdateStatus executeUpdate fail errno=" << db->getErrno();
        return 0;
    }

    for (auto& id : ids) {
        auto info = get(id);
        if (info) {
            info->setStatus(status);
            info->setUpdateTime(time(0));
            setCommentCache(info);
        }
    }
    return ids.size();
}

int64_t CommentManager::batchDelete(const std::vector<int64_t>& ids)
{
    if (ids.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::CommentInfoDao::newQuery();
    qb->set("is_deleted", (int64_t)1);
    qb->whereIn("id", ids);
    if (qb->executeUpdate(db)) {
        ERROR(logger) << "batchDelete executeUpdate fail errno=" << db->getErrno();
        return 0;
    }

    for (auto& id : ids) {
        auto info = get(id);
        if (info) {
            info->setIsDeleted(1);
            info->setUpdateTime(time(0));
            setCommentCache(info);
        }
    }
    return ids.size();
}

CommentManager::CommentStats CommentManager::getStats()
{
    CommentStats stats;

    std::string cached;
    if (getCachedStringResult("cmt:stats", cached)) {
        std::stringstream ss(cached);
        std::string token;
        auto next = [&]() -> int64_t {
            std::getline(ss, token, '|');
            return chen::TypeUtil::Atoi(token);
        };
        stats.total = next();
        stats.pending = next();
        stats.approved = next();
        stats.spam = next();
        stats.reported = next();
        return stats;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query status counts with GROUP BY
    {
        auto qb = data::CommentInfoDao::newQuery();
        qb->select("status, COUNT(*) AS cnt");
        qb->where("is_deleted", "=", (int64_t)0);
        qb->groupBy("status");
        std::vector<std::pair<int32_t, int64_t>> rows;
        if (qb->queryPairs<int32_t, int64_t>(rows, db) == 0) {
            for (auto& [s, cnt] : rows) {
                stats.total += cnt;
                switch (s) {
                case PENDING:
                    stats.pending = cnt;
                    break;
                case APPROVED:
                    stats.approved = cnt;
                    break;
                case SPAM:
                    stats.spam = cnt;
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Query reported count
    {
        auto qb = data::CommentInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("is_reported", "=", (int64_t)1);
        int64_t reported = 0;
        if (qb->executeCount(reported, db) == 0) {
            stats.reported = reported;
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.pending << "|" << stats.approved << "|" << stats.spam << "|" << stats.reported;
    cacheStringResult("cmt:stats", ss.str());

    return stats;
}

} // namespace blog
