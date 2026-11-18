#include "platform/servlets/organization/OrganizationStatsServlet.hpp"

#include <chen/log/log.h>

#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationStatsServlet::OrganizationStatsServlet()
    : BlogLoginedServlet("OrganizationStatsServlet")
{
}

int32_t OrganizationStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // 检查组织是否存在
        auto org = OrganizationMgr::GetInstance()->get(id);
        if (!org) {
            result->setErrno(errcode::ORG_NOT_FOUND);
            break;
        }

        auto stats = OrganizationUserRelMgr::GetInstance()->getStats(id);

        result->set("total_members", stats.total_members);
        result->set("active_members", stats.active_members);
        result->set("org_admin_count", stats.org_admin_count);
        result->set("regular_count", stats.regular_count);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
