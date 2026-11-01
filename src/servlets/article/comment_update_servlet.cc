#include "comment_update_servlet.h"

#include "../../manager/comment_manager.h"
#include "../../manager/comment_praise_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <json/json.h>

namespace blog {
namespace servlet {

CommentUpdateServlet::CommentUpdateServlet()
    : BlogLoginedServlet("CommentUpdateServlet")
{
}

int32_t CommentUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_STRING(result, content, "content");

        auto comment = CommentMgr::GetInstance()->get(id);
        if (!comment || comment->getIsDeleted()) {
            result->setErrno(errcode::COMMENT_NOT_FOUND);
            break;
        }

        // only the comment author can edit
        if (comment->getUserId() != uid) {
            result->setErrno(errcode::COMMENT_PERMISSION_DENIED);
            break;
        }

        if (!CommentMgr::GetInstance()->update(id, content)) {
            result->setErrno(errcode::COMMENT_UPDATE_FAILED);
            break;
        }

        // build response (same structure as create)
        Json::Value item;
        item["id"] = comment->getId();
        item["content"] = comment->getContent();
        item["user_id"] = comment->getUserId();
        item["time"] = chen::TimeUtil::Time2StrSec(comment->getCreateTime());

        auto user = UserMgr::GetInstance()->get(uid);
        if (user) {
            item["user"] = user->getName();
            item["avatar"] = user->getAvatar();
        } else {
            item["user"] = "";
            item["avatar"] = "";
        }

        int64_t likes = CommentPraiseRelMgr::GetInstance()->countByComment(id);
        item["likes"] = likes;
        item["is_liked"] = CommentPraiseRelMgr::GetInstance()->isPraising(uid, id);
        item["replies"] = Json::Value(Json::arrayValue);
        item["reply_count"] = CommentMgr::GetInstance()->countReplies(id);

        result->setData(item);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
