#include "search_sync.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include <unistd.h>

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/iomanager/iomanager.h>
#include <chen/log/log.h>
#include <chen/socket/address.h>
#include <chen/util/time_util.h>
#include <chen/util/type_util.h>

#include "blog/data/article_category_rel_info.h"
#include "blog/data/article_info.h"
#include "blog/data/article_label_rel_info.h"

#include "../manager/article_category_rel_manager.h"
#include "../manager/article_label_rel_manager.h"
#include "../manager/article_manager.h"
#include "../util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<bool>::ptr g_enabled = chen::Config::Lookup("search.enabled", true, "enable search service client");
static chen::ConfigVar<std::string>::ptr g_hub_address =
    chen::Config::Lookup("search.hub_address", std::string("127.0.0.1:9010"), "search RPC hub address");
static chen::ConfigVar<uint64_t>::ptr g_timeout_ms = chen::Config::Lookup("search.timeout_ms", (uint64_t)1500, "search query RPC timeout ms");
static chen::ConfigVar<uint64_t>::ptr g_update_timeout_ms =
    chen::Config::Lookup("search.update_timeout_ms", (uint64_t)5000, "search update RPC timeout ms");
static chen::ConfigVar<uint64_t>::ptr g_connect_timeout_ms =
    chen::Config::Lookup("search.connect_timeout_ms", (uint64_t)1000, "search RPC connect timeout ms");
static chen::ConfigVar<uint64_t>::ptr g_reconnect_interval_ms =
    chen::Config::Lookup("search.reconnect_interval_ms", (uint64_t)5000, "search RPC reconnect interval ms");
static chen::ConfigVar<uint64_t>::ptr g_flush_interval_ms =
    chen::Config::Lookup("search.flush_interval_ms", (uint64_t)1000, "dirty sync interval ms");
static chen::ConfigVar<uint64_t>::ptr g_stats_interval_ms =
    chen::Config::Lookup("search.stats_interval_ms", (uint64_t)30000, "search server stats check interval ms");
static chen::ConfigVar<int32_t>::ptr g_flush_batch_size = chen::Config::Lookup("search.flush_batch_size", (int32_t)200, "dirty ids per flush");
static chen::ConfigVar<int32_t>::ptr g_snapshot_page_size =
    chen::Config::Lookup("search.snapshot_page_size", (int32_t)200, "articles per snapshot page");
static chen::ConfigVar<uint64_t>::ptr g_snapshot_batch_bytes =
    chen::Config::Lookup("search.snapshot_batch_bytes", (uint64_t)524288, "snapshot docs batch bytes");
static chen::ConfigVar<uint64_t>::ptr g_snapshot_rpc_timeout_ms =
    chen::Config::Lookup("search.snapshot_rpc_timeout_ms", (uint64_t)10000, "snapshot RPC timeout ms");
static chen::ConfigVar<std::string>::ptr g_dirty_key =
    chen::Config::Lookup("search.dirty_key", std::string("search:dirty"), "redis dirty article hash key");
static chen::ConfigVar<std::string>::ptr g_redis_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

static bool ParseDirtyValue(const std::string& value, int32_t& op, int64_t& ts)
{
    auto pos = value.find(':');
    if (pos == std::string::npos) {
        op = static_cast<int32_t>(chen::TypeUtil::Atoi(value));
        ts = 0;
        return true;
    }
    op = static_cast<int32_t>(chen::TypeUtil::Atoi(value.substr(0, pos)));
    ts = chen::TypeUtil::Atoi(value.substr(pos + 1));
    return true;
}

bool SearchSync::init()
{
    m_enabled.store(g_enabled->getValue());
    if (!m_enabled.load()) {
        INFO(logger) << "search client disabled";
        return true;
    }

    m_stopped.store(false);
    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        ERROR(logger) << "search client init failed: no IOManager";
        m_enabled.store(false);
        return false;
    }

    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
    uint64_t interval = g_flush_interval_ms->getValue();
    if (interval == 0) {
        interval = 1000;
    }
    m_timer = iom->addTimer(
        interval,
        [this]() {
            wake();
        },
        true);
    wake();
    INFO(logger) << "search client initialized, hub=" << g_hub_address->getValue();
    return true;
}

void SearchSync::stop()
{
    m_stopped.store(true);
    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
    invalidateClient();
}

void SearchSync::invalidateClient()
{
    std::lock_guard<std::mutex> lock(m_client_mutex);
    if (m_client) {
        m_client->close();
        m_client.reset();
    }
    m_last_stats_ms.store(0);
}

bool SearchSync::ensureConnected()
{
    if (!m_enabled.load() || m_stopped.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_client_mutex);
    if (m_client && m_client->isConnected()) {
        return true;
    }

    if (m_client) {
        m_client->close();
        m_client.reset();
    }

    uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
    uint64_t reconnect_interval = g_reconnect_interval_ms->getValue();
    if (reconnect_interval > 0 && now_ms < m_last_connect_ms.load() + reconnect_interval) {
        return false;
    }
    m_last_connect_ms.store(now_ms);

    auto addr = chen::Address::LookupAny(g_hub_address->getValue());
    if (!addr) {
        ERROR(logger) << "invalid search hub address: " << g_hub_address->getValue();
        return false;
    }

    auto client = std::make_shared<chen::rpc::RpcClient>();
    if (!client->connect(addr, g_connect_timeout_ms->getValue())) {
        DEBUG(logger) << "connect search hub failed: " << g_hub_address->getValue();
        return false;
    }
    m_client = client;
    m_last_stats_ms.store(0);
    INFO(logger) << "connected search hub: " << g_hub_address->getValue();
    return true;
}

void SearchSync::wake()
{
    if (!m_enabled.load() || m_stopped.load()) {
        return;
    }
    if (m_sync_scheduled.exchange(true)) {
        return;
    }

    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        m_sync_scheduled.store(false);
        return;
    }
    iom->schedule([this]() {
        m_sync_scheduled.store(false);
        syncLoop();
    });
}

void SearchSync::syncLoop()
{
    if (!m_enabled.load() || m_stopped.load()) {
        return;
    }
    if (m_syncing.exchange(true)) {
        return;
    }

    try {
        do {
            uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
            bool need_stats = now_ms >= m_last_stats_ms.load() + g_stats_interval_ms->getValue();
            if (need_stats) {
                m_last_stats_ms.store(now_ms);
            }

            if (!ensureConnected()) {
                break;
            }
            // 新建连接后强制做一次 stats，避免 SearchServer 重启后长时间返回未就绪
            if (m_last_stats_ms.load() == 0) {
                need_stats = true;
                m_last_stats_ms.store(now_ms);
            }

            if (need_stats) {
                tagSearchStats stats;
                if (!checkStats(stats)) {
                    break;
                }
                if (!stats.Ready || stats.IndexVersion != SS_SEARCH_INDEX_VERSION) {
                    INFO(logger) << "search index not ready, ready=" << stats.Ready << " version=" << stats.IndexVersion << ", start snapshot";
                    if (!runSnapshot()) {
                        m_last_stats_ms.store(0);
                        break;
                    }
                    m_last_stats_ms.store(chen::TimeUtil::GetCurrentMs());
                }
            }

            flushDirty();
        } while (0);
    } catch (const std::exception& e) {
        ERROR(logger) << "search sync loop failed: " << e.what();
        m_last_stats_ms.store(0);
        invalidateClient();
    } catch (...) {
        ERROR(logger) << "search sync loop failed: unknown error";
        m_last_stats_ms.store(0);
        invalidateClient();
    }

    m_syncing.store(false);
}

bool SearchSync::checkStats(tagSearchStats& stats)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(m_client_mutex);
        client = m_client;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        stats = client->callWithTimeout<tagSearchStats>(g_timeout_ms->getValue(), static_cast<uint32_t>(SS_CMD_SEARCH_GET_STATS));
        return stats.Code == SEARCH_OK;
    } catch (const std::exception& e) {
        WARN(logger) << "search stats failed: " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::query(const tagSearchQuery& query, tagSearchResult& result)
{
    if (!m_enabled.load() || m_stopped.load()) {
        return false;
    }
    if (!ensureConnected()) {
        return false;
    }

    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(m_client_mutex);
        client = m_client;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        result = client->callWithTimeout<tagSearchResult>(g_timeout_ms->getValue(), static_cast<uint32_t>(SS_CMD_SEARCH_ARTICLES), query);
        if (result.Code != SEARCH_OK) {
            WARN(logger) << "search query rejected, code=" << result.Code;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "search query failed: " << e.what();
        invalidateClient();
        return false;
    }
}

void SearchSync::markDirty(uint64_t article_id, int32_t op)
{
    if (!m_enabled.load() || m_stopped.load() || article_id == 0) {
        return;
    }

    DirtyEntry entry;
    entry.op = op;
    entry.ts = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());

    {
        std::lock_guard<std::mutex> lock(m_dirty_mutex);
        auto it = m_local_dirty.find(article_id);
        if (it == m_local_dirty.end() || entry.ts >= it->second.ts) {
            m_local_dirty[article_id] = entry;
        }
    }
    cacheDirty(article_id, entry);
    wake();
}

void SearchSync::cacheDirty(uint64_t article_id, const DirtyEntry& entry)
{
    static const char* kCacheScript = "local cur = redis.call('hget', KEYS[1], ARGV[1]) "
                                      "if cur then "
                                      "  local i = string.find(cur, ':') "
                                      "  if i then "
                                      "    local t = tonumber(string.sub(cur, i + 1)) "
                                      "    if t and t > tonumber(ARGV[2]) then return 0 end "
                                      "  end "
                                      "end "
                                      "return redis.call('hset', KEYS[1], ARGV[1], ARGV[3])";
    std::string value = std::to_string(entry.op) + ":" + std::to_string(entry.ts);
    auto rpy = chen::RedisUtil::Cmd(
        g_redis_name->getValue(), "eval %s 1 %s %llu %lld %s", kCacheScript, g_dirty_key->getValue().c_str(), (unsigned long long)article_id,
        (long long)entry.ts, value.c_str());
    if (!rpy) {
        WARN(logger) << "cache dirty article to redis failed, article_id=" << article_id;
    }
}

std::map<uint64_t, SearchSync::DirtyEntry> SearchSync::loadDirty()
{
    std::map<uint64_t, DirtyEntry> dirty;
    {
        std::lock_guard<std::mutex> lock(m_dirty_mutex);
        dirty = m_local_dirty;
    }

    auto rpy = chen::RedisUtil::Cmd(g_redis_name->getValue(), "hgetall %s", g_dirty_key->getValue().c_str());
    if (rpy && rpy->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < rpy->elements; i += 2) {
            if (!rpy->element[i] || !rpy->element[i + 1] || !rpy->element[i]->str || !rpy->element[i + 1]->str) {
                continue;
            }
            uint64_t article_id = static_cast<uint64_t>(chen::TypeUtil::Atoi(rpy->element[i]->str));
            DirtyEntry entry;
            ParseDirtyValue(rpy->element[i + 1]->str, entry.op, entry.ts);
            auto it = dirty.find(article_id);
            if (it == dirty.end() || entry.ts >= it->second.ts) {
                dirty[article_id] = entry;
            }
        }
    }
    return dirty;
}

void SearchSync::ackDirty(uint64_t article_id, const DirtyEntry& entry)
{
    {
        std::lock_guard<std::mutex> lock(m_dirty_mutex);
        auto it = m_local_dirty.find(article_id);
        if (it != m_local_dirty.end() && it->second.ts <= entry.ts) {
            m_local_dirty.erase(it);
        }
    }

    static const char* kAckScript = "if redis.call('hget', KEYS[1], ARGV[1]) == ARGV[2] then "
                                    "return redis.call('hdel', KEYS[1], ARGV[1]) end return 0";
    chen::RedisUtil::Cmd(
        g_redis_name->getValue(), "eval %s 1 %s %llu %d:%lld", kAckScript, g_dirty_key->getValue().c_str(), (unsigned long long)article_id, entry.op,
        (long long)entry.ts);
}

bool SearchSync::buildDoc(uint64_t article_id, int64_t update_time, tagSearchDoc& doc, bool& missing)
{
    missing = false;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    auto info = ArticleMgr::GetInstance()->get(static_cast<int64_t>(article_id));
    if (!info || info->getIsDeleted()) {
        missing = true;
        return false;
    }

    doc = tagSearchDoc();
    doc.ArticleId = static_cast<uint64_t>(info->getId());
    doc.UserId = info->getUserId();
    doc.State = info->getState();
    doc.Channel = info->getChannel();
    doc.PublishTime = info->getPublishTime();
    doc.Weight = info->getWeight();
    doc.SearchUpdateTime = update_time;
    doc.Title = info->getTitle();
    doc.Content = info->getContent();

    std::vector<data::ArticleCategoryRelInfo::ptr> categories;
    if (!ArticleCategoryRelMgr::GetInstance()->listByArticleId(categories, info->getId(), true)) {
        return false;
    }
    for (auto& category : categories) {
        doc.CategoryIds.push_back(static_cast<uint64_t>(category->getCategoryId()));
    }

    std::vector<data::ArticleLabelRelInfo::ptr> labels;
    if (!ArticleLabelRelMgr::GetInstance()->listByArticleId(labels, info->getId(), true)) {
        return false;
    }
    for (auto& label : labels) {
        doc.LabelIds.push_back(static_cast<uint64_t>(label->getLabelId()));
    }
    return true;
}

bool SearchSync::sendUpsert(const tagSearchDoc& doc)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(m_client_mutex);
        client = m_client;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(g_update_timeout_ms->getValue(), static_cast<uint32_t>(SS_CMD_SEARCH_UPSERT_DOC), doc);
        if (rt != SEARCH_OK) {
            WARN(logger) << "upsert search doc rejected, article_id=" << doc.ArticleId << ", code=" << rt;
            if (rt == SEARCH_NOT_READY) {
                m_last_stats_ms.store(0);
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "upsert search doc failed, article_id=" << doc.ArticleId << ": " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::sendRemove(uint64_t article_id, int64_t update_time)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(m_client_mutex);
        client = m_client;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(
            g_update_timeout_ms->getValue(), static_cast<uint32_t>(SS_CMD_SEARCH_REMOVE_ARTICLE), article_id, update_time);
        if (rt != SEARCH_OK) {
            WARN(logger) << "remove search article rejected, article_id=" << article_id << ", code=" << rt;
            if (rt == SEARCH_NOT_READY) {
                m_last_stats_ms.store(0);
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "remove search article failed, article_id=" << article_id << ": " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::flushDirty()
{
    auto dirty = loadDirty();
    if (dirty.empty()) {
        return true;
    }

    if (!GetDB()) {
        WARN(logger) << "flush search dirty failed: no db connection";
        return false;
    }

    std::vector<std::pair<int64_t, uint64_t>> order;
    order.reserve(dirty.size());
    for (auto& [article_id, entry] : dirty) {
        order.emplace_back(entry.ts, article_id);
    }
    std::sort(order.begin(), order.end());

    int32_t limit = g_flush_batch_size->getValue();
    if (limit <= 0) {
        limit = 200;
    }

    int32_t handled = 0;
    for (auto& item : order) {
        uint64_t article_id = item.second;
        if (handled >= limit) {
            break;
        }
        if (m_stopped.load()) {
            break;
        }

        auto& entry = dirty[article_id];
        bool ok = false;
        if (entry.op == OP_REMOVE) {
            ok = sendRemove(article_id, entry.ts);
        } else {
            tagSearchDoc doc;
            bool missing = false;
            if (buildDoc(article_id, entry.ts, doc, missing)) {
                ok = sendUpsert(doc);
            } else if (missing) {
                ok = sendRemove(article_id, entry.ts);
            } else {
                WARN(logger) << "build search doc failed, retry later, article_id=" << article_id;
                return false;
            }
        }

        if (!ok) {
            return false;
        }
        ackDirty(article_id, entry);
        ++handled;
    }
    return true;
}

bool SearchSync::runSnapshot()
{
    if (!ensureConnected()) {
        return false;
    }

    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(m_client_mutex);
        client = m_client;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    uint64_t snapshot_id = (chen::TimeUtil::GetCurrentMs() << 8) ^ (static_cast<uint64_t>(::getpid()) & 0xff);
    uint64_t rpc_timeout = g_snapshot_rpc_timeout_ms->getValue();

    try {
        int64_t started_at = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());
        int32_t rt = client->callWithTimeout<int32_t>(rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_BEGIN), snapshot_id, started_at);
        if (rt != SEARCH_OK) {
            WARN(logger) << "search rebuild begin rejected, code=" << rt;
            return false;
        }

        int64_t last_id = 0;
        std::vector<tagSearchDoc> batch;
        size_t batch_bytes = 0;
        uint64_t total_docs = 0;

        auto flush_batch = [&]() -> bool {
            if (batch.empty()) {
                return true;
            }
            int32_t code = client->callWithTimeout<int32_t>(rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_DOCS), snapshot_id, batch);
            if (code != SEARCH_OK) {
                WARN(logger) << "search rebuild docs rejected, snapshot_id=" << snapshot_id << ", code=" << code;
                return false;
            }
            total_docs += batch.size();
            batch.clear();
            batch_bytes = 0;
            return true;
        };

        int32_t page_size = g_snapshot_page_size->getValue();
        if (page_size <= 0) {
            page_size = 200;
        }
        size_t batch_limit = g_snapshot_batch_bytes->getValue();
        if (batch_limit == 0) {
            batch_limit = 524288;
        }

        while (true) {
            if (m_stopped.load()) {
                WARN(logger) << "search snapshot aborted: client stopped";
                return false;
            }
            auto db = GetDB();
            if (!db) {
                ERROR(logger) << "search snapshot failed: no db connection";
                return false;
            }

            auto qb = data::ArticleInfoDao::newQuery();
            qb->where("is_deleted", "=", (int64_t)0)->where("id", ">", last_id)->orderBy("id", "ASC")->limit(page_size);

            std::vector<data::ArticleInfo::ptr> infos;
            if (data::ArticleInfoDao::QueryByBuilder(infos, qb, db) != 0) {
                ERROR(logger) << "search snapshot query article failed, last_id=" << last_id;
                return false;
            }
            if (infos.empty()) {
                break;
            }

            std::vector<int64_t> ids;
            ids.reserve(infos.size());
            for (auto& info : infos) {
                ids.push_back(info->getId());
            }

            std::map<int64_t, std::vector<uint64_t>> category_map;
            auto category_qb = data::ArticleCategoryRelInfoDao::newQuery();
            category_qb->whereIn("article_id", ids)->where("is_deleted", "=", (int64_t)0);
            std::vector<data::ArticleCategoryRelInfo::ptr> categories;
            if (data::ArticleCategoryRelInfoDao::QueryByBuilder(categories, category_qb, db) != 0) {
                ERROR(logger) << "search snapshot query category rel failed";
                return false;
            }
            for (auto& rel : categories) {
                category_map[rel->getArticleId()].push_back(static_cast<uint64_t>(rel->getCategoryId()));
            }

            std::map<int64_t, std::vector<uint64_t>> label_map;
            auto label_qb = data::ArticleLabelRelInfoDao::newQuery();
            label_qb->whereIn("article_id", ids)->where("is_deleted", "=", (int64_t)0);
            std::vector<data::ArticleLabelRelInfo::ptr> labels;
            if (data::ArticleLabelRelInfoDao::QueryByBuilder(labels, label_qb, db) != 0) {
                ERROR(logger) << "search snapshot query label rel failed";
                return false;
            }
            for (auto& rel : labels) {
                label_map[rel->getArticleId()].push_back(static_cast<uint64_t>(rel->getLabelId()));
            }

            for (auto& info : infos) {
                tagSearchDoc doc;
                doc.ArticleId = static_cast<uint64_t>(info->getId());
                doc.UserId = info->getUserId();
                doc.State = info->getState();
                doc.Channel = info->getChannel();
                doc.PublishTime = info->getPublishTime();
                doc.Weight = info->getWeight();
                // 快照文档统一使用快照开始时间：
                // - 快照开始后的增量更新（ts > started_at）会在 commit 时覆盖快照
                // - 快照开始前的旧脏数据（ts <= started_at）由其后的快照内容覆盖，应被忽略
                doc.SearchUpdateTime = started_at;
                doc.Title = info->getTitle();
                doc.Content = info->getContent();

                auto cit = category_map.find(info->getId());
                if (cit != category_map.end()) {
                    doc.CategoryIds = cit->second;
                }
                auto lit = label_map.find(info->getId());
                if (lit != label_map.end()) {
                    doc.LabelIds = lit->second;
                }

                batch_bytes +=
                    doc.Title.size() + doc.Content.size() + doc.CategoryIds.size() * sizeof(uint64_t) + doc.LabelIds.size() * sizeof(uint64_t);
                batch.push_back(std::move(doc));
                last_id = info->getId();

                if (batch_bytes >= batch_limit) {
                    if (!flush_batch()) {
                        return false;
                    }
                }
            }

            if (static_cast<int32_t>(infos.size()) < page_size) {
                break;
            }
        }

        if (!flush_batch()) {
            return false;
        }

        int64_t finished_at = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());
        int32_t rt = client->callWithTimeout<int32_t>(rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_COMMIT), snapshot_id, finished_at);
        if (rt != SEARCH_OK) {
            WARN(logger) << "search rebuild commit rejected, snapshot_id=" << snapshot_id << ", code=" << rt;
            return false;
        }

        INFO(logger) << "search snapshot done, snapshot_id=" << snapshot_id << ", docs=" << total_docs;
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "search snapshot failed: " << e.what();
        invalidateClient();
        return false;
    }
}

} // namespace blog
