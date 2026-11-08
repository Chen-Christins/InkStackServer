#include "comment_replies_servlet.h"

#include "../../manager/comment_manager.h"
#include "../../manager/comment_praise_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/util/util.h>
#include <json/json.h>

namespace blog {
namespace servlet {

CommentRepliesServlet::CommentRepliesServlet()
    : BlogServlet("CommentRepliesServlet")
{
}

int32_t CommentRepliesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, comment_id, "comment_id");

        int64_t offset = request->getParamAs<int64_t>("offset", 0);
        int64_t size = request->getParamAs<int64_t>("size", 20);

        // check parent comment exists
        auto parent = CommentMgr::GetInstance()->get(comment_id);
        if (!parent || parent->getIsDeleted()) {
            result->setErrno(errcode::COMMENT_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);

        std::vector<data::CommentInfo::ptr> replies;
        CommentMgr::GetInstance()->listReplies(replies, comment_id, offset, size);
        int64_t total = CommentMgr::GetInstance()->countReplies(comment_id);

        Json::Value list(Json::arrayValue);
        for (auto& r : replies) {
            Json::Value item;
            item["id"] = r->getId();
            item["content"] = r->getContent();
            item["user_id"] = r->getUserId();
            item["time"] = chen::TimeUtil::Time2StrSec(r->getCreateTime());

            auto user = UserMgr::GetInstance()->get(r->getUserId());
            if (user) {
                item["user"] = user->getName();
                item["avatar"] = user->getAvatar();
            } else {
                item["user"] = "";
                item["avatar"] = "";
            }

            int64_t likes = CommentPraiseRelMgr::GetInstance()->countByComment(r->getId());
            item["likes"] = likes;
            if (uid > 0) {
                item["is_liked"] = CommentPraiseRelMgr::GetInstance()->isPraising(uid, r->getId());
            } else {
                item["is_liked"] = false;
            }
            item["reply_count"] = CommentMgr::GetInstance()->countReplies(r->getId());

            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
