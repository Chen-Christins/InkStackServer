#include "user/servlets/user/UserOrganizationListServlet.hpp"

#include <chen/log/log.h>

#include "user/client/PlatformClient.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserOrganizationListServlet::UserOrganizationListServlet()
    : BlogLoginedServlet("UserOrganizationListServlet")
{
}

int32_t UserOrganizationListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);

        auto orgs = client::PlatformClientMgr::GetInstance()->listUserOrgs(uid);

        Json::Value list;
        for (auto& org : orgs) {
            Json::Value item;
            item["id"] = org.RelId;
            item["org_id"] = org.OrgId;
            item["org_name"] = org.OrgName;
            item["org_description"] = org.OrgDescription;
            item["type"] = org.Type;
            item["role"] = org.Role;
            item["join_time"] = org.JoinTime;
            item["count"] = org.MemberCount;
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
