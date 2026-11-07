#include "organization_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationListServlet::OrganizationListServlet()
    : BlogLoginedServlet("OrganizationListServlet")
{
}

int32_t OrganizationListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int32_t status = request->getParamAs<int32_t>("status", -1);

        std::vector<data::OrganizationInfo::ptr> orgs;
        int64_t total = OrganizationMgr::GetInstance()->listByPages(orgs, 0, INT32_MAX, status, true);

        result->set("total", total);
        Json::Value list;
        for (auto& i : orgs) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["status"] = i->getStatus();
            item["type"] = i->getType();
            item["description"] = i->getDescription();
            // 获取成员数量
            int32_t status = OrganizationUserRelManager::Status::APPROVED;
            item["count"] = OrganizationUserRelMgr::GetInstance()->getMemberCount(i->getId(), status, true);
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
