#include "comment/servlets/article/CommentPraiseServlet.hpp"

#include "common/event/EventDefine.hpp"
#include "comment/manager/CommentManager.hpp"
#include "comment/manager/CommentPraiseRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

#include <json/json.h>

namespace blog {
namespace servlet {

CommentPraiseServlet::CommentPraiseServlet()
    : BlogLoginedServlet("CommentPraiseServlet")
{
}

int32_t CommentPraiseServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, comment_id, "comment_id");

        // check comment exists
        auto comment = CommentMgr::GetInstance()->get(comment_id);
        if (!comment || comment->getIsDeleted()) {
            result->setErrno(errcode::COMMENT_NOT_FOUND);
            break;
        }

        bool already_praising = CommentPraiseRelMgr::GetInstance()->isPraising(uid, comment_id);

        if (already_praising) {
            // unlike
            if (!CommentPraiseRelMgr::GetInstance()->unpraise(uid, comment_id)) {
                result->setErrno(errcode::COMMENT_UNPRAISE_FAILED);
                break;
            }
            result->set("is_praising", false);
        } else {
            // like
            auto info = CommentPraiseRelMgr::GetInstance()->praise(uid, comment_id);
            if (!info) {
                result->setErrno(errcode::COMMENT_PRAISE_FAILED);
                break;
            }
            result->set("is_praising", true);

            // Notify comment author (not self-praise)
            if (comment->getUserId() != uid) {
                auto liker_info = UserMgr::GetInstance()->get(uid);

                EventCommentPraiseData data = {};
                data.comment_author_id = comment->getUserId();
                data.liker_id = uid;
                data.liker_name = liker_info ? liker_info->getName() : "someone";
                data.article_id = comment->getArticleId();
                data.comment_id = comment_id;

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_COMMENT_PRAISE, std::move(data));
            }
        }

        int64_t praise_count = CommentPraiseRelMgr::GetInstance()->countByComment(comment_id);
        result->set("praise_count", praise_count);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
