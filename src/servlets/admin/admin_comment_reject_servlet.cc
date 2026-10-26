#include "admin_comment_reject_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <json/json.h>

#include <sstream>

namespace blog {
namespace servlet {

AdminCommentRejectServlet::AdminCommentRejectServlet()
    : BlogLoginedServlet("AdminCommentRejectServlet")
{
}

int32_t AdminCommentRejectServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, ids_str, "ids");

        std::vector<int64_t> ids;
        std::stringstream ss(ids_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try {
                ids.push_back(chen::TypeUtil::Atoi(token));
            } catch (...) {
            }
        }

        int64_t affected = CommentMgr::GetInstance()->batchUpdateStatus(ids, CommentManager::REJECTED);

        Json::Value idList(Json::arrayValue);
        for (auto& id : ids) {
            idList.append(std::to_string(id));
        }
        result->set("ids", idList);
        result->set("affected", affected);
        result->setErrno(errcode::SUCCESS);

        // Notify comment authors
        for (auto& cid : ids) {
            auto comment = CommentMgr::GetInstance()->get(cid);
            if (!comment) {
                continue;
            }
            int64_t author_id = comment->getUserId();
            if (author_id == uid) {
                continue;
            }

            {
                EventCommentData data = {};
                data.comment_id = cid;
                data.author_id = author_id;
                data.type = "comment_rejected";

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_COMMENT, std::move(data));
            }
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
