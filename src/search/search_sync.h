/**
 * @file search_sync.h
 * @brief 搜索索引同步客户端
 * @author Christins
 * @date 2026-11-19
 * @copyright Apache 2.0
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>

#include <chen/rpc/rpc_client.h>
#include <chen/timer/timer.h>
#include <chen/util/singleton.h>

#include "protocol_ss_search.h" // IWYU pragma: keep

namespace blog {

/**
 * @brief 搜索索引同步客户端
 * @details 负责连接 SearchServer hub、查询、增量文档推送、失败重试与全量快照。
 *          待同步文章 ID 同时写入 Redis hash（持久化 outbox）和进程内 map，
 *          SearchServer 不可用时不会丢失变更。
 */
class SearchSync {
public:
    enum Op : int32_t {
        OP_UPSERT = 1,
        OP_REMOVE = 2
    };

    SearchSync() = default;

    /**
     * @brief 初始化连接与定时任务
     */
    bool init();

    /**
     * @brief 停止客户端
     */
    void stop();

    /**
     * @brief 标记文章需要同步
     * @param article_id 文章 ID
     * @param op OP_UPSERT / OP_REMOVE
     */
    void markDirty(uint64_t article_id, int32_t op);

    /**
     * @brief 查询文章
     * @return false 表示 RPC 不可用或服务端返回错误
     */
    bool query(const tagSearchQuery& query, tagSearchResult& result);

private:
    struct DirtyEntry {
        int32_t op = 0;
        int64_t ts = 0;
    };

    void wake();

    void syncLoop();

    bool ensureConnected();

    void invalidateClient();

    bool checkStats(tagSearchStats& stats);

    bool runSnapshot();

    bool flushDirty();

    bool buildDoc(uint64_t article_id, int64_t update_time, tagSearchDoc& doc, bool& missing);

    bool sendUpsert(const tagSearchDoc& doc);

    bool sendRemove(uint64_t article_id, int64_t update_time);

    std::map<uint64_t, DirtyEntry> loadDirty();

    void cacheDirty(uint64_t article_id, const DirtyEntry& entry);

    void ackDirty(uint64_t article_id, const DirtyEntry& entry);

private:
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_stopped{false};
    std::atomic<bool> m_sync_scheduled{false};
    std::atomic<bool> m_syncing{false};
    std::atomic<uint64_t> m_last_stats_ms{0};
    std::atomic<uint64_t> m_last_connect_ms{0};

    std::mutex m_client_mutex;
    chen::rpc::RpcClient::ptr m_client;

    std::mutex m_dirty_mutex;
    std::map<uint64_t, DirtyEntry> m_local_dirty;

    chen::Timer::ptr m_timer;
};

typedef chen::LeakySingleton<SearchSync> SearchSyncMgr;

} // namespace blog
