#include "platform/servlets/rd/RdOrganizationMembersServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "platform/servlets/rd/RdHelper.hpp"

namespace blog {
namespace servlet {

RdOrganizationMembersServlet::RdOrganizationMembersServlet()
    : BlogLoginedServlet("RdOrganizationMembersServlet")
{
}

int32_t RdOrganizationMembersServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        int64_t uid = getUserId(request);
        auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
            result->setErrno(errcode::ORG_NOT_MEMBER);
            break;
        }

        std::vector<data::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelMgr::GetInstance()->getByPages(members, org_id, 0, INT32_MAX, OrganizationUserRelManager::Status::APPROVED, true);

        Json::Value arr(Json::arrayValue);
        for (auto& m : members) {
            if (m->getRole() < OrganizationManager::Role::REPORTER) {
                continue;
            }
            Json::Value item;
            item["user_id"] = m->getUserId();
            item["name"] = rd::GetUserName(m->getUserId());
            item["role"] = rd::OrgRoleToFrontend(m->getRole());
            item["avatar"] = rd::GetUserAvatar(m->getUserId());
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
