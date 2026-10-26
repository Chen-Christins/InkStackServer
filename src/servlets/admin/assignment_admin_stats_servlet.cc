#include "assignment_admin_stats_servlet.h"

#include "../../manager/assignment_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

AssignmentAdminStatsServlet::AssignmentAdminStatsServlet()
    : BlogLoginedServlet("AssignmentAdminStatsServlet")
{
}

int32_t AssignmentAdminStatsServlet::handle(
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
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto stats = AssignmentMgr::GetInstance()->getStats();

        result->setErrno(errcode::SUCCESS);
        result->set("total_assignments", stats.total);
        result->set("active_assignments", stats.active);
        result->set("closed_assignments", stats.closed);
        result->set("draft_assignments", stats.draft);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
