#include "article/manager/ArticleManager.hpp"

#include "../../common/util/Util.hpp"
#include "article/logic/Index.hpp"
#include "common/manager/CacheUtil.hpp"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/iomanager/iomanager.h>
#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include <ctime>
#include <set>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:art:";
/// 浏览去重窗口（秒）
static const int kViewDedupTTL = 10 * 60;

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

static std::string articleCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::ArticleInfo::ptr parseArticleCache(const Json::Value& v)
{
    auto info = std::make_shared<data::ArticleInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setUserId(cacheJsonInt64(v, "user_id"));
    info->setTitle(cacheJsonString(v, "title"));
    info->setContent(cacheJsonString(v, "content"));
    info->setType(cacheJsonInt32(v, "type"));
    info->setState(cacheJsonInt32(v, "state"));
    info->setChannel(cacheJsonInt64(v, "channel"));
    info->setIsDeleted(cacheJsonInt32(v, "is_deleted"));
    info->setPublishTime(cacheJsonTime(v, "publish_time"));
    info->setWeight(cacheJsonInt64(v, "weight"));
    info->setViews(cacheJsonInt64(v, "views"));
    info->setPraise(cacheJsonInt64(v, "praise"));
    info->setFavorites(cacheJsonInt64(v, "favorites"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::ArticleInfo::ptr getArticleCache(int64_t id)
{
    std::string key = articleCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseArticleCache(v);
}

static void setArticleCache(const data::ArticleInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(articleCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static void delArticleCache(int64_t id)
{
    redisCacheDel(articleCacheKey(id));
}

data::ArticleInfo::ptr ArticleManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticleInfoDao::ParseRow(rt);
}

void ArticleManager::add(blog::data::ArticleInfo::ptr info)
{
    setArticleCache(info);
}

void ArticleManager::del(int64_t id)
{
    delArticleCache(id);
}

blog::data::ArticleInfo::ptr ArticleManager::get(int64_t id)
{
    auto v = getArticleCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::ArticleInfoDao::Query(id, db);
    if (v) {
        setArticleCache(v);
    }
    return v;
}

bool ArticleManager::listByUserId(std::vector<data::ArticleInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleInfoDao::newQuery();
    qb->where("user_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

int64_t ArticleManager::listByUserIdPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t id, int32_t offset, int32_t size, bool valid, int state)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticleInfoDao::newQuery();
    qb->whereIf(id != 0, "user_id", "=", id);
    qb->whereIf(state != 0, "state", "=", (int64_t)state);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    return total;
}

int64_t ArticleManager::listByLabelPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t label_id, int32_t offset, int32_t size, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");
    qb->join("article_label_rel alr", "a.id = alr.article_id");
    qb->where("alr.label_id", "=", label_id);
    qb->where("alr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)PUBLISHED);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    // 结果缓存：仅对首页做缓存
    std::string listKey =
        "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0") + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (getCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                infos.push_back(info);
            }
        }
        return executeCountCached(qb, db, "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (data::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    std::vector<int64_t> ids;
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    cacheListResult(listKey, ids);
    return total;
}

int64_t ArticleManager::listByCategoryPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t category_id, int32_t offset, int32_t size, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");
    qb->join("article_category_rel acr", "a.id = acr.article_id");
    qb->where("acr.category_id", "=", category_id);
    qb->where("acr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)PUBLISHED);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    // 结果缓存：仅对首页做缓存
    std::string listKey =
        "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0") + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (getCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                infos.push_back(info);
            }
        }
        return executeCountCached(qb, db, "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (data::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    std::vector<int64_t> ids;
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    cacheListResult(listKey, ids);
    return total;
}

int64_t ArticleManager::listByPages(
    std::vector<data::ArticleInfo::ptr>& infos,
    int32_t offset,
    int state,
    int category,
    int32_t role,
    int32_t days,
    int32_t size,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");

    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->whereIf(state != 0, "a.state", "=", (int64_t)state);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        time_t now = time(0);
        qb->whereSQL("a.create_time >= ?", (int64_t)(now - days * 24 * 3600));
    }
    qb->orderBy("a.id", "DESC");

    {
        std::stringstream ck;
        ck << "art:list:" << state << ":" << category << ":" << role << ":" << days << ":" << (valid ? "1" : "0");
        int64_t total = 0;
        if (data::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
            return 0;
        }
        return total;
    }
}

void ArticleManager::delVerify(int64_t id)
{
    delArticleCache(id);
}

void ArticleManager::addVerify(data::ArticleInfo::ptr info)
{
    setArticleCache(info);
}

int64_t ArticleManager::listVerifyPages(std::vector<data::ArticleInfo::ptr>& infos, int32_t offset, int32_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticleInfoDao::newQuery();
    qb->where("state", "=", (int64_t)CHECKING);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (info->getIsDeleted()) {
            delArticleCache(info->getId());
            continue;
        }
    }
    return total;
}

std::pair<data::ArticleInfo::ptr, data::ArticleInfo::ptr> ArticleManager::nearby(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return std::pair(nullptr, nullptr);
    }

    // 查找上一篇（id 最大的小于给定 id 的已发布文章）
    data::ArticleInfo::ptr prev;
    {
        auto qb = data::ArticleInfoDao::newQuery();
        qb->whereSQL("id < ?", id);
        qb->where("state", "=", (int64_t)PUBLISHED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->orderBy("id", "DESC");
        qb->limit(1);
        std::vector<data::ArticleInfo::ptr> results;
        if (data::ArticleInfoDao::QueryByBuilder(results, qb, db) == 0 && !results.empty()) {
            prev = results[0];
        }
    }

    // 查找下一篇（id 最小的大于给定 id 的已发布文章）
    data::ArticleInfo::ptr next;
    {
        auto qb = data::ArticleInfoDao::newQuery();
        qb->whereSQL("id > ?", id);
        qb->where("state", "=", (int64_t)PUBLISHED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->orderBy("id", "ASC");
        qb->limit(1);
        std::vector<data::ArticleInfo::ptr> results;
        if (data::ArticleInfoDao::QueryByBuilder(results, qb, db) == 0 && !results.empty()) {
            next = results[0];
        }
    }

    return std::pair(prev, next);
}

ArticleManager::ArticleStats ArticleManager::getStats(int32_t category, int32_t role, int32_t days, const std::string& keyword)
{
    ArticleStats stats;

    // 无过滤条件时走缓存
    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::string cached;
        if (getCachedStringResult("art:stats", cached)) {
            std::stringstream ss(cached);
            std::string token;
            auto next = [&]() -> int64_t {
                std::getline(ss, token, '|');
                return chen::TypeUtil::Atoi(token);
            };
            stats.total = next();
            stats.pending = next();
            stats.published = next();
            stats.rejected = next();
            stats.reported = next();
            return stats;
        }
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    auto qb = data::ArticleInfoDao::newQuery("a");
    qb->select("a.state, COUNT(*) as cnt");

    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->where("a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        time_t now = time(0);
        qb->whereSQL("a.create_time >= ?", (int64_t)(now - days * 24 * 3600));
    }
    if (!keyword.empty()) {
        qb->where("a.title", "LIKE", "%" + keyword + "%");
    }
    qb->groupBy("a.state");

    std::vector<std::pair<int32_t, int64_t>> rows;
    if (qb->queryPairs<int32_t, int64_t>(rows, db)) {
        return stats;
    }
    for (auto& [st, cnt] : rows) {
        stats.total += cnt;
        switch (st) {
        case CHECKING:
            stats.pending += cnt;
            break;
        case PUBLISHED:
            stats.published += cnt;
            break;
        case REJECTED:
            stats.rejected += cnt;
            break;
        default:
            break;
        }
    }

    // 无过滤条件时缓存结果
    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::stringstream ss;
        ss << stats.total << "|" << stats.pending << "|" << stats.published << "|" << stats.rejected << "|" << stats.reported;
        cacheStringResult("art:stats", ss.str());
    }

    return stats;
}

std::string ArticleManager::statusString()
{
    std::stringstream ss;
    ss << "ArticleManager cache=redis prefix=" << kCachePrefix << " ttl=" << kCacheTTL;
    return ss.str();
}

void ArticleManager::start()
{
    syncStatsFromDB();
}

void ArticleManager::stop()
{
    // 停止前将脏数据（浏览/点赞/收藏数）刷新到 DB，避免 reload/stop 时丢失
    flushDirty();
}

void ArticleManager::flushDirty()
{
    std::set<int64_t> updates;
    {
        std::unique_lock<std::shared_mutex> lock(m_updatesMutex);
        updates.swap(m_updates);
    }

    if (updates.empty()) {
        return;
    }
    auto conn = GetDB();
    if (!conn) {
        ERROR(logger) << "flushDirty: get db connect fail";

        std::unique_lock<std::shared_mutex> lock(m_updatesMutex);
        for (auto& i : updates) {
            m_updates.insert(i);
        }
        return;
    }
    auto trans = conn->openTransaction();
    if (!trans) {
        ERROR(logger) << "flushDirty: open transaction failed";
        std::unique_lock<std::shared_mutex> lock(m_updatesMutex);
        for (auto& i : updates) {
            m_updates.insert(i);
        }
        return;
    }
    for (auto& i : updates) {
        auto info = get(i);
        if (!info) {
            continue;
        }
        // Redis 反序列化后对象是 clean 的，这里显式标记计数字段，确保能写回 DB
        info->setViews(info->getViews());
        info->setPraise(info->getPraise());
        info->setFavorites(info->getFavorites());
        if (data::ArticleInfoDao::Update(info, conn)) {
            addUpdate(i);
        } else {
            setArticleCache(info);
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "flushDirty: commit failed";
        std::unique_lock<std::shared_mutex> lock(m_updatesMutex);
        for (auto& i : updates) {
            m_updates.insert(i);
        }
        return;
    }
    INFO(logger) << "flushDirty: flushed " << updates.size() << " articles";
}

bool ArticleManager::incViews(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = addViews(id, cookie_id);
    if (v) {
        info->setViews(info->getViews() + 1);
        setArticleCache(info);
        addUpdate(id);
        chen::IOManager::GetThis()->schedule([user_id]() {
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "incr blog:total_visits");
            auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "incr blog:today_visits");
            if (rpy && rpy->integer == 1) {
                int64_t now = chen::TimeUtil::GetCurrentSec();
                int64_t tomorrow_midnight = chen::TimeUtil::GetDayStart(now + 86400);
                chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expireat blog:today_visits %lld", tomorrow_midnight);
            }
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "pfadd blog:visitors %lld", user_id);
        });
    }
    return true;
}

bool ArticleManager::incPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hexist pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset pra_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset pra_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setPraise(info->getPraise() + 1);
    setArticleCache(info);
    addUpdate(id);

    return true;
}

bool ArticleManager::incFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hexist fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset fav_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset fav_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setFavorites(info->getFavorites() + 1);
    setArticleCache(info);
    addUpdate(id);

    return true;
}

bool ArticleManager::decPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = false;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hdel pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hdel pra_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    if (v) {
        info->setPraise(info->getPraise() - 1);
        setArticleCache(info);
        addUpdate(id);
    }

    return true;
}

bool ArticleManager::decFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = false;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hdel fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset fav_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    if (v) {
        info->setFavorites(info->getFavorites() - 1);
        setArticleCache(info);
        addUpdate(id);
    }

    return true;
}

bool ArticleManager::listUserFav(int64_t id, std::map<int64_t, int64_t>& articles)
{
#define PROC(id, mask, articles)                                                                               \
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), mask, id);                                  \
    if (!rpy) {                                                                                                \
        ERROR(logger) << "hgetall fail";                                                                       \
        return false;                                                                                          \
    }                                                                                                          \
    for (size_t i = 0; i < rpy->elements; i += 2) {                                                            \
        articles[chen::TypeUtil::Atoi(rpy->element[i]->str)] = chen::TypeUtil::Atoi(rpy->element[i + 1]->str); \
    }                                                                                                          \
    return true;

    PROC(id, "hgetall fav_u2a:%lld", articles);
}

bool ArticleManager::listUserPra(int64_t id, std::map<int64_t, int64_t>& articles)
{
    PROC(id, "hgetall pra_u2a:%lld", articles);
}

bool ArticleManager::listArticleFav(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall fav_a2u:%lld", users);
}

bool ArticleManager::listArticlePra(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall pra_a2u:%lld", users);
}
#undef PROC

static const char* kScheduleKey = "article:schedule";

void ArticleManager::scheduleArticle(int64_t article_id, int64_t publish_time)
{
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zadd %s %lld %lld", kScheduleKey, (long long)publish_time, (long long)article_id);
}

void ArticleManager::unscheduleArticle(int64_t article_id)
{
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrem %s %lld", kScheduleKey, (long long)article_id);
}

void ArticleManager::onTick()
{
    doArticlePublishSchedule();

    flushDirty();
}

bool ArticleManager::addViews(uint64_t id, const std::string& cookie_id)
{
    // 用 Redis SET NX EX 做 10 分钟去重，避免进程内 m_viewsCache 无界增长
    std::string key = "art:view:" + std::to_string(id) + ":" + cookie_id;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "set %s 1 ex %d nx", key.c_str(), kViewDedupTTL);
    if (!rpy) {
        // Redis 异常时不去重，避免丢掉有效浏览
        return true;
    }
    // NX 未命中返回 nil，说明该用户 10 分钟内已经浏览过
    return rpy->type != REDIS_REPLY_NIL;
}

void ArticleManager::addUpdate(int64_t id)
{
    std::unique_lock<std::shared_mutex> lock(m_updatesMutex);
    m_updates.insert(id);
}

void ArticleManager::incPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return;
    }
    info->setPraise(info->getPraise() + 1);
    setArticleCache(info);
    addUpdate(id);
}

void ArticleManager::decPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(m_counterMutex);
    auto info = get(id);
    if (!info) {
        return;
    }
    if (info->getPraise() > 0) {
        info->setPraise(info->getPraise() - 1);
        setArticleCache(info);
        addUpdate(id);
    }
}

int64_t ArticleManager::getTodayViews()
{
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get blog:today_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

int64_t ArticleManager::getTotalViews()
{
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get blog:total_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    // Redis 未初始化 — 从 DB 计算并回填
    auto db = GetDB();
    if (!db) {
        return 0;
    }
    auto qb = data::ArticleInfoDao::newQuery();
    qb->select("CAST(COALESCE(SUM(views), 0) AS SIGNED)");
    qb->where("state", "=", (int64_t)PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    qb->queryScalarInt64(total, db);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "set blog:total_visits %lld", total);
    return total;
}

int64_t ArticleManager::getTotalVisitors()
{
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "pfcount blog:visitors");
    if (rpy) {
        return rpy->integer;
    }
    return 0;
}

void ArticleManager::syncStatsFromDB()
{
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del blog:total_visits");
    int64_t total = getTotalViews();
    INFO(logger) << "syncStatsFromDB: blog:total_visits recalculated from DB = " << total;
}

void ArticleManager::getCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days)
{
    // 先查 Redis 缓存
    {
        auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "GET calendar:%lld:%d:%d", user_id, year, month);
        if (rpy && rpy->str && strlen(rpy->str) > 0) {
            Json::Value cached;
            if (chen::JsonUtil::FromString(cached, rpy->str) && cached.isArray() && cached.size() > 0) {
                for (auto& v : cached) {
                    days.push_back(v.asInt());
                }
                return;
            }
        }
    }

    // 缓存未命中：查数据库
    // 直接用日期字符串比较，避免 mktime/localtime_r 与 MySQL UNIX_TIMESTAMP() 之间
    // 的时区不一致问题（前者用服务器本地时区，后者用 MySQL session 时区）
    char start_str[32], end_str[32];
    snprintf(start_str, sizeof(start_str), "%04d-%02d-01 00:00:00", year, month);
    int next_month = month + 1;
    int next_year = year;
    if (next_month > 12) {
        next_month = 1;
        next_year++;
    }
    snprintf(end_str, sizeof(end_str), "%04d-%02d-01 00:00:00", next_year, next_month);

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getCalendarDays: Get DB connection fail";
        return;
    }

    auto qb = data::ArticleInfoDao::newQuery();
    qb->select("DISTINCT DAY(publish_time) as d");
    qb->where("user_id", "=", user_id);
    qb->where("state", "=", (int64_t)PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("publish_time", ">=", std::string(start_str));
    qb->where("publish_time", "<", std::string(end_str));
    qb->orderBy("d", "ASC");

    qb->queryColumn<int32_t>(days, db, "d");

    // 写入 Redis 缓存（TTL 5 分钟），空结果不缓存，避免因首次查询无数据而导致
    // 后续文章发布后仍返回空
    if (!days.empty()) {
        Json::Value daysJson(Json::arrayValue);
        for (auto d : days) {
            daysJson.append(d);
        }
        std::string jsonStr = chen::JsonUtil::ToString(daysJson);
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "SETEX calendar:%lld:%d:%d 300 %s", user_id, year, month, jsonStr.c_str());
    }
}

void ArticleManager::clearCalendarCache(int64_t user_id, int64_t publishTime)
{
    time_t pt = static_cast<time_t>(publishTime);
    struct tm tm_pt = {};
    localtime_r(&pt, &tm_pt);
    int32_t year = tm_pt.tm_year + 1900;
    int32_t month = tm_pt.tm_mon + 1;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "DEL calendar:%lld:%d:%d", user_id, year, month);
}

void ArticleManager::doArticlePublishSchedule()
{
    time_t now = chen::TimeUtil::GetCurrentSec();

    // 从 Redis sorted set 获取已到期的文章 ID
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "zrangebyscore %s 0 %lld", kScheduleKey, now);
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY || rpy->elements == 0) {
        return;
    }

    // 收集需要发布的文章 ID
    std::vector<int64_t> article_ids;
    for (size_t i = 0; i < rpy->elements; ++i) {
        if (rpy->element[i]->type == REDIS_REPLY_STRING && rpy->element[i]->str) {
            article_ids.push_back(chen::TypeUtil::Atoi(rpy->element[i]->str));
        }
    }

    if (article_ids.empty()) {
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getDB error";
        return;
    }

    // 批量从 sorted set 中移除（原子操作）
    std::string zrem_cmd = "zrem " + std::string(kScheduleKey);
    for (auto id : article_ids) {
        zrem_cmd += " " + std::to_string(id);
    }
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), zrem_cmd.c_str());

    // 逐个更新文章状态
    auto trans = db->openTransaction();
    for (auto id : article_ids) {
        auto info = get(id);
        if (!info || info->getIsDeleted()) {
            continue;
        }
        if (info->getState() == PUBLISHED) {
            continue;
        }

        info->setState(PUBLISHED);
        info->setUpdateTime(now);

        if (data::ArticleInfoDao::Update(info, db)) {
            ERROR(logger) << "Update error errno=" << db->getErrno() << " errstr=" << db->getErrStr() << " data=" << info->toJsonString();
            continue;
        }
        setArticleCache(info);

        clearCalendarCache(info->getUserId(), info->getPublishTime());

        IndexMgr::GetInstance()->updateArticle(info);
    }
    trans->commit();
}

} // namespace blog
