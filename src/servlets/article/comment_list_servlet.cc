#include "comment_list_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/comment_praise_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/util/util.h>
#include <json/json.h>

namespace blog {
namespace servlet {

CommentListServlet::CommentListServlet()
    : BlogServlet("CommentListServlet")
{
}

static Json::Value buildCommentItem(
    data::CommentInfo::ptr c,
    const std::unordered_map<int64_t, std::vector<data::CommentInfo::ptr>>& children,
    int64_t uid)
{
    Json::Value item;
    item["id"] = c->getId();
    item["content"] = c->getContent();
    item["user_id"] = c->getUserId();
    item["time"] = chen::TimeUtil::Time2StrSec(c->getCreateTime());

    auto user = UserMgr::GetInstance()->get(c->getUserId());
    if (user) {
        item["user"] = user->getName();
        item["avatar"] = user->getAvatar();
    } else {
        item["user"] = "";
        item["avatar"] = "";
    }

    int64_t likes = CommentPraiseRelMgr::GetInstance()->countByComment(c->getId());
    item["likes"] = likes;
    if (uid > 0) {
        item["is_liked"] = CommentPraiseRelMgr::GetInstance()->isPraising(uid, c->getId());
    } else {
        item["is_liked"] = false;
    }

    // recursively build nested replies
    Json::Value repliesArr(Json::arrayValue);
    auto it = children.find(c->getId());
    if (it != children.end()) {
        for (auto& child : it->second) {
            repliesArr.append(buildCommentItem(child, children, uid));
        }
    }
    item["replies"] = repliesArr;
    item["reply_count"] = it != children.end() ? it->second.size() : 0;

    return item;
}

int32_t CommentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");

        // check article exists
        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article || article->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);

        // fetch all approved comments for this article
        std::vector<data::CommentInfo::ptr> allComments;
        CommentMgr::GetInstance()->listAllByArticle(allComments, article_id);

        // build parent_id -> [children] map
        std::unordered_map<int64_t, std::vector<data::CommentInfo::ptr>> children;
        for (auto& c : allComments) {
            children[c->getParentId()].push_back(c);
        }

        // sort each group by create_time ascending (oldest first)
        for (auto& [pid, vec] : children) {
            std::sort(vec.begin(), vec.end(), [](auto& a, auto& b) {
                return a->getCreateTime() < b->getCreateTime();
            });
        }

        // build top-level list (parent_id == 0)
        Json::Value list(Json::arrayValue);
        auto it = children.find(0);
        if (it != children.end()) {
            for (auto& c : it->second) {
                list.append(buildCommentItem(c, children, uid));
            }
        }

        result->set("total", it != children.end() ? it->second.size() : 0);
        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
