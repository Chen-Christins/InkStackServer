#pragma once

#include "blog/data/article_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>

namespace blog {

class ArticleManager {
public:
    enum Status {
        UNKNOWN = 0,
        CHECKING = 1,
        PUBLISHED = 2,
        REJECTED = 3,
        PRIVATE = 4
    };
    enum Type {
        ORIGINAL = 1,
        REPRINT = 2
    };

    void add(blog::data::ArticleInfo::ptr info);

    void del(int64_t id);

    blog::data::ArticleInfo::ptr get(int64_t id);

    bool listByUserId(std::vector<data::ArticleInfo::ptr>& infos, int64_t id, bool valid);

    int64_t listByUserIdPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t id, int32_t offset, int32_t size, bool valid, int state);

    int64_t listByLabelPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t label_id, int32_t offset, int32_t size, bool valid);

    int64_t listByCategoryPages(std::vector<data::ArticleInfo::ptr>& infos, int64_t category_id, int32_t offset, int32_t size, bool valid);

    void delVerify(int64_t id);

    void addVerify(data::ArticleInfo::ptr info);

    int64_t listVerifyPages(std::vector<data::ArticleInfo::ptr>& infos, int32_t offset, int32_t size);

    int64_t listByPages(
        std::vector<data::ArticleInfo::ptr>& infos,
        int32_t offset,
        int state,
        int category,
        int32_t role,
        int32_t days,
        int32_t size,
        bool valid);

    std::pair<data::ArticleInfo::ptr, data::ArticleInfo::ptr> nearby(int64_t id);

    std::string statusString();

    void start();

    void stop();

    /**
     * @brief 定时发布已到发布时间的文章（由服务 onTick 调用）
     * @note 从 Redis sorted set 获取到期文章，避免 DB 全表扫描
     */
    void onTick();

    /**
     * @brief 将文章加入定时发布调度（Redis sorted set）
     * @param article_id 文章 ID
     * @param publish_time 发布时间戳
     */
    void scheduleArticle(int64_t article_id, int64_t publish_time);

    /**
     * @brief 将文章从定时发布调度中移除
     * @param article_id 文章 ID
     */
    void unscheduleArticle(int64_t article_id);

    /**
     * @brief 将脏数据（浏览/点赞/收藏数）刷新到数据库
     * @note 由 onUpdateTimer 定期调用，也在 stop 时调用以避免 reload/stop 时丢失增量
     */
    void flushDirty();

    bool incViews(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    bool incPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    bool incFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    bool decPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    bool decFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    void incPraiseCount(int64_t id);

    void decPraiseCount(int64_t id);

    bool listUserFav(int64_t id, std::map<int64_t, int64_t>& articles);

    bool listUserPra(int64_t id, std::map<int64_t, int64_t>& articles);

    bool listArticleFav(int64_t id, std::map<int64_t, int64_t>& users);

    bool listArticlePra(int64_t id, std::map<int64_t, int64_t>& users);

    struct ArticleStats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t published = 0;
        int64_t rejected = 0;
        int64_t reported = 0;
    };
    ArticleStats getStats(int32_t category, int32_t role, int32_t days, const std::string& keyword);

    int64_t getTodayViews();

    int64_t getTotalViews();

    int64_t getTotalVisitors();

    /**
     * @brief 获取指定月份中有已发布文章的日期列表
     * @param[in] year 年份
     * @param[in] month 月份（1-12）
     * @param[out] days 输出：该月中有文章的日期（1-31），去重后升序排列
     */
    void getCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days);

    /**
     * @brief 清除指定时间戳对应月份的日历缓存
     * @param publishTime 文章发布时间戳（Unix timestamp）
     */
    void clearCalendarCache(int64_t user_id, int64_t publishTime);

private:
    /**
     * @brief 启动时从 DB 同步统计数到 Redis，覆盖可能存在的旧实例残留数据
     */
    void syncStatsFromDB();

    bool addViews(uint64_t id, const std::string& cookie_id);

    void addUpdate(int64_t id);

    static data::ArticleInfo::ptr parseRow(chen::ISQLData::ptr rt);

    void doArticlePublishSchedule();

private:
    /// 待更新到 DB 的文章 ID 集合锁
    std::shared_mutex m_updatesMutex;
    /// 文章计数（浏览/点赞/收藏）读改写锁，避免 Redis 缓存下的并发丢更新
    std::mutex m_counterMutex;
    /// 待更新到 DB 的文章 ID 集合
    std::set<int64_t> m_updates;
};

typedef chen::Singleton<ArticleManager> ArticleMgr;

} // namespace blog
