#include "platform/servlets/rd/RdOrganizationsServlet.hpp"

#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"

namespace blog {
namespace servlet {

RdOrganizationsServlet::RdOrganizationsServlet()
    : BlogLoginedServlet("RdOrganizationsServlet")
{
}

int32_t RdOrganizationsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);

        std::vector<data::OrganizationUserRelInfo::ptr> user_orgs;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(user_orgs, uid, -1, true);

        Json::Value arr(Json::arrayValue);
        for (auto& rel : user_orgs) {
            if (rel->getRole() < OrganizationManager::Role::REPORTER) {
                continue;
            }

            auto org = OrganizationMgr::GetInstance()->get(rel->getOrgId());
            if (!org) {
                continue;
            }

            Json::Value item;
            item["org_id"] = org->getId();
            item["org_name"] = org->getName();
            item["role"] = rel->getRole();
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
