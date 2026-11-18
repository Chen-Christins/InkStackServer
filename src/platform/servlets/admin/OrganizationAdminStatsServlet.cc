#include "platform/servlets/admin/OrganizationAdminStatsServlet.hpp"

#include "platform/manager/OrganizationManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

OrganizationAdminStatsServlet::OrganizationAdminStatsServlet()
    : BlogLoginedServlet("OrganizationAdminStatsServlet")
{
}

int32_t OrganizationAdminStatsServlet::handle(
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
        if (!current_user || current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto stats = OrganizationMgr::GetInstance()->getStats();

        result->setErrno(errcode::SUCCESS);
        result->set("total_organizations", stats.total);
        result->set("active_organizations", stats.active);
        result->set("inactive_organizations", stats.inactive);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
