#include "platform/servlets/rd/RdBugDetailServlet.hpp"

#include "platform/manager/BugManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Permission.hpp"
#include "platform/servlets/rd/RdHelper.hpp"

namespace blog {
namespace servlet {

RdBugDetailServlet::RdBugDetailServlet()
    : BlogLoginedServlet("RdBugDetailServlet")
{
}

int32_t RdBugDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        if (!id) {
            result->setErrno(errcode::PARAM_MISSING, "id is required");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserManager::Role::ADMIN);

        auto info = BugMgr::GetInstance()->get(id);
        if (!info || info->getIsDeleted()) {
            result->setErrno(errcode::BUG_NOT_FOUND);
            break;
        }

        int64_t info_org_id = info->getOrgId();
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(info_org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            if (!permission::CanViewBug(rel->getRole(), uid, info->getCreatorId())) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        }

        Json::Value data;
        rd::BuildBugJson(data, info);
        result->set("data", data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
