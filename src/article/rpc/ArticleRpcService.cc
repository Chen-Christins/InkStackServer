#include "article/rpc/ArticleRpcService.hpp"

#include <vector>

#include "protocol_article.h" // IWYU pragma: keep

#include "article/manager/ArticleManager.hpp"
#include "common/rpc/RpcHubClient.hpp"

namespace blog {
namespace rpc {

namespace {

/**
 * @brief 文章实体 -> 跨服务只读视图
 */
tagArticleBrief ToArticleBrief(const data::ArticleInfo::ptr& article)
{
    tagArticleBrief brief;
    brief.Id = article->getId();
    brief.UserId = article->getUserId();
    brief.Title = article->getTitle();
    brief.State = article->getState();
    brief.IsDeleted = article->getIsDeleted() != 0;
    brief.Views = article->getViews();
    brief.Praise = article->getPraise();
    brief.Favorites = article->getFavorites();
    brief.PublishTime = article->getPublishTime();
    brief.CreateTime = article->getCreateTime();
    brief.UpdateTime = article->getUpdateTime();
    return brief;
}

} // namespace

void RegisterArticleRpcMethods()
{
    auto hub = RpcHubClientMgr::GetInstance();

    hub->registerMethod(ARTICLE_CMD_GET_BRIEF, [](int64_t id) -> tagArticleBrief {
        auto article = ArticleMgr::GetInstance()->get(id);
        if (!article) {
            return {};
        }
        return ToArticleBrief(article);
    });

    hub->registerMethod(ARTICLE_CMD_GET_BRIEFS, [](std::vector<int64_t> ids) -> std::vector<tagArticleBrief> {
        std::vector<tagArticleBrief> briefs;
        briefs.reserve(ids.size());
        for (int64_t id : ids) {
            auto article = ArticleMgr::GetInstance()->get(id);
            if (!article) {
                continue;
            }
            briefs.push_back(ToArticleBrief(article));
        }
        return briefs;
    });

    hub->registerMethod(ARTICLE_CMD_GET_STATS, [](int32_t category, int32_t role, int32_t days, std::string keyword) -> tagArticleStats {
        auto stats = ArticleMgr::GetInstance()->getStats(category, role, days, keyword);
        tagArticleStats out;
        out.Total = stats.total;
        out.Pending = stats.pending;
        out.Published = stats.published;
        out.Rejected = stats.rejected;
        out.Reported = stats.reported;
        return out;
    });

    hub->registerMethod(ARTICLE_CMD_GET_VIEW_STATS, []() -> tagArticleViewStats {
        tagArticleViewStats out;
        out.TodayViews = ArticleMgr::GetInstance()->getTodayViews();
        out.TotalViews = ArticleMgr::GetInstance()->getTotalViews();
        out.TotalVisitors = ArticleMgr::GetInstance()->getTotalVisitors();
        return out;
    });

    hub->registerMethod(ARTICLE_CMD_LIST_RECENT, [](int32_t limit) -> std::vector<tagArticleBrief> {
        std::vector<tagArticleBrief> briefs;
        if (limit <= 0) {
            return briefs;
        }
        std::vector<data::ArticleInfo::ptr> articles;
        ArticleMgr::GetInstance()->listByPages(articles, 0, 0, 0, -1, 0, limit, true);
        briefs.reserve(articles.size());
        for (auto& article : articles) {
            if (!article) {
                continue;
            }
            briefs.push_back(ToArticleBrief(article));
        }
        return briefs;
    });

    hub->registerMethod(ARTICLE_CMD_COUNT_BY_USER, [](int64_t uid, bool include_deleted) -> int64_t {
        std::vector<data::ArticleInfo::ptr> articles;
        return ArticleMgr::GetInstance()->listByUserIdPages(articles, uid, 0, 1, !include_deleted, 0);
    });
}

} // namespace rpc
} // namespace blog
