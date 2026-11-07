#include "comment_create_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/util/util.h>
#include <json/json.h>

namespace blog {
namespace servlet {

CommentCreateServlet::CommentCreateServlet()
    : BlogLoginedServlet("CommentCreateServlet")
{
}

int32_t CommentCreateServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");
        DEFINE_AND_CHECK_STRING(result, content, "content");

        int64_t parent_id = 0;
        request->checkGetParamAs("parent_id", parent_id);

        // check article exists
        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article || article->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_NOT_FOUND);
            break;
        }

        // if replying, check parent comment exists
        if (parent_id > 0) {
            auto parent = CommentMgr::GetInstance()->get(parent_id);
            if (!parent || parent->getIsDeleted()) {
                result->setErrno(errcode::COMMENT_PARENT_NOT_FOUND);
                break;
            }
        }

        std::string ip = session->getRemoteAddressString();
        std::string user_agent = request->getHeader("User-Agent");

        auto info = CommentMgr::GetInstance()->create(article_id, uid, content, parent_id, ip, user_agent);
        if (!info) {
            result->setErrno(errcode::COMMENT_CREATE_FAILED);
            break;
        }

        // build response
        Json::Value item;
        item["id"] = info->getId();
        item["content"] = info->getContent();
        item["user_id"] = info->getUserId();
        item["time"] = chen::TimeUtil::Time2StrSec(info->getCreateTime());

        auto user = UserMgr::GetInstance()->get(uid);
        if (user) {
            item["user"] = user->getName();
            item["avatar"] = user->getAvatar();
        } else {
            item["user"] = "";
            item["avatar"] = "";
        }

        item["likes"] = 0;
        item["is_liked"] = false;
        item["replies"] = Json::Value(Json::arrayValue);
        item["reply_count"] = 0;

        result->setData(item);
        result->setErrno(errcode::SUCCESS);

        // Notify article author and parent comment author asynchronously
        {
            auto commenter_info = UserMgr::GetInstance()->get(uid);

            EventCommentCreatedData data = {};
            data.commenter_id = uid;
            data.commenter_name = commenter_info ? commenter_info->getName() : "someone";
            data.article_id = article_id;
            data.article_title = article->getTitle();
            data.author_id = article->getUserId();
            data.parent_comment_id = parent_id;
            if (parent_id > 0) {
                auto parent = CommentMgr::GetInstance()->get(parent_id);
                if (parent) {
                    data.parent_comment_author_id = parent->getUserId();
                }
            }

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_COMMENT_CREATED, std::move(data));
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
