#include "user_organization_list_servlet.h"

#include <chen/log/log.h>

#include <algorithm>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"

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

        std::vector<data::OrganizationUserRelInfo::ptr> orgs;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(orgs, uid, -1, true);

        std::sort(orgs.begin(), orgs.end(), [](const auto& a, const auto& b) {
            return a->getCreateTime() < b->getCreateTime();
        });

        Json::Value list;
        for (auto& i : orgs) {
            Json::Value item;
            auto org = OrganizationMgr::GetInstance()->get(i->getOrgId());
            item["id"] = i->getId();
            item["org_id"] = i->getOrgId();
            item["org_name"] = org->getName();
            item["org_description"] = org->getDescription();
            item["type"] = org->getType();
            item["role"] = i->getRole();
            item["join_time"] = i->getCreateTime();
            // 成员数
            int32_t status = OrganizationUserRelManager::Status::APPROVED;
            item["count"] = OrganizationUserRelMgr::GetInstance()->getMemberCount(i->getOrgId(), status, true);
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
