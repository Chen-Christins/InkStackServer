#include "comment/rpc/CommentRpcService.hpp"

#include <vector>

#include "protocol_comment.h" // IWYU pragma: keep

#include "comment/manager/CommentManager.hpp"
#include "common/rpc/RpcHubClient.hpp"

namespace blog {
namespace rpc {

namespace {

/**
 * @brief 评论实体 -> 跨服务只读视图
 */
tagCommentBrief ToCommentBrief(const data::CommentInfo::ptr& comment)
{
    tagCommentBrief brief;
    brief.Id = comment->getId();
    brief.ArticleId = comment->getArticleId();
    brief.UserId = comment->getUserId();
    brief.Content = comment->getContent();
    brief.Status = comment->getStatus();
    brief.IsDeleted = comment->getIsDeleted() != 0;
    brief.CreateTime = comment->getCreateTime();
    brief.UpdateTime = comment->getUpdateTime();
    return brief;
}

} // namespace

void RegisterCommentRpcMethods()
{
    auto hub = RpcHubClientMgr::GetInstance();

    hub->registerMethod(COMMENT_CMD_COUNT_BY_ARTICLE, [](int64_t article_id) -> int64_t {
        return CommentMgr::GetInstance()->countByArticle(article_id);
    });

    hub->registerMethod(COMMENT_CMD_GET_STATS, []() -> tagCommentStats {
        auto stats = CommentMgr::GetInstance()->getStats();
        tagCommentStats out;
        out.Total = stats.total;
        out.Pending = stats.pending;
        out.Approved = stats.approved;
        out.Spam = stats.spam;
        out.Reported = stats.reported;
        return out;
    });

    hub->registerMethod(COMMENT_CMD_LIST_RECENT, [](int32_t limit) -> std::vector<tagCommentBrief> {
        std::vector<tagCommentBrief> briefs;
        if (limit <= 0) {
            return briefs;
        }
        std::vector<data::CommentInfo::ptr> comments;
        CommentMgr::GetInstance()->listByAdmin(comments, 1, limit, 0, "", 0, -1);
        briefs.reserve(comments.size());
        for (auto& comment : comments) {
            if (!comment) {
                continue;
            }
            briefs.push_back(ToCommentBrief(comment));
        }
        return briefs;
    });

    hub->registerMethod(COMMENT_CMD_GET_BRIEFS, [](std::vector<int64_t> ids) -> std::vector<tagCommentBrief> {
        std::vector<tagCommentBrief> briefs;
        briefs.reserve(ids.size());
        for (int64_t id : ids) {
            auto comment = CommentMgr::GetInstance()->get(id);
            if (!comment) {
                continue;
            }
            briefs.push_back(ToCommentBrief(comment));
        }
        return briefs;
    });
}

} // namespace rpc
} // namespace blog
