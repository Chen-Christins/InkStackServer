#include "organization_admin_lists_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationAdminListsServlet::OrganizationAdminListsServlet()
    : BlogLoginedServlet("OrganizationAdminListsServlet")
{
}

int32_t OrganizationAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t status = request->getParamAs<int32_t>("status", -1);

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

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<data::OrganizationInfo::ptr> orgs;
        uint64_t total = OrganizationMgr::GetInstance()->listByPages(orgs, offset, page_size, status, true);

        result->set("total", total);
        auto& list = result->jsondata["list"];
        for (auto& i : orgs) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["type"] = i->getType();
            item["status"] = i->getStatus();
            item["description"] = i->getDescription();
            item["create_time"] = i->getCreateTime();
            int32_t status = OrganizationUserRelManager::Status::APPROVED;
            item["count"] = OrganizationUserRelMgr::GetInstance()->getMemberCount(i->getId(), status, true);
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
