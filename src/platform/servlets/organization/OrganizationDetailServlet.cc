#include "platform/servlets/organization/OrganizationDetailServlet.hpp"

#include <chen/log/log.h>

#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationDetailServlet::OrganizationDetailServlet()
    : BlogLoginedServlet("OrganizationDetailServlet")
{
}

int32_t OrganizationDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        auto org = OrganizationMgr::GetInstance()->get(id);
        if (!org) {
            result->setErrno(errcode::ORG_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);
        auto rel = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(id, uid);
        if (rel) {
            result->set("user_in_org", rel->getStatus());
            result->set("user_role", rel->getRole());
        }

        result->set("id", org->getId());
        result->set("name", org->getName());
        result->set("description", org->getDescription());
        result->set("type", org->getType());
        result->set("status", org->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
