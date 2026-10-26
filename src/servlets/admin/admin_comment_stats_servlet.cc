#include "admin_comment_stats_servlet.h"

#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

AdminCommentStatsServlet::AdminCommentStatsServlet()
    : BlogLoginedServlet("AdminCommentStatsServlet")
{
}

int32_t AdminCommentStatsServlet::handle(
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

        auto stats = CommentMgr::GetInstance()->getStats();

        result->set("total_comments", stats.total);
        result->set("pending_comments", stats.pending);
        result->set("approved_comments", stats.approved);
        result->set("spam_comments", stats.spam);
        result->set("reported_comments", stats.reported);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
