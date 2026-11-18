/**
 * @file OrganizationRepoPrsDeleteServlet.cc
 * @brief 组织仓库 PR 删除接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "platform/servlets/organization/OrganizationRepoPrsDeleteServlet.hpp"

#include <chen/log/log.h>

#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationRepoManager.hpp"
#include "platform/manager/OrganizationRepoPrManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationRepoPrsDeleteServlet::OrganizationRepoPrsDeleteServlet()
    : BlogLoginedServlet("OrganizationRepoPrsDeleteServlet")
{
}

int32_t OrganizationRepoPrsDeleteServlet::handle(
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

        auto pr = OrganizationRepoPrMgr::GetInstance()->get(id);
        if (!pr) {
            result->setErrno(errcode::PARAM_INVALID, "PR 记录不存在");
            break;
        }

        auto repo = OrganizationRepoMgr::GetInstance()->get(pr->getRepoId());
        if (!repo) {
            result->setErrno(errcode::ORG_REPO_NOT_FOUND);
            break;
        }
        int64_t org_id = repo->getOrgId();

        // 权限：系统管理员或组织内研发主管及以上
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        if (system_role != UserManager::Role::ADMIN) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrganizationManager::Role::ORG_ADMIN && org_role != OrganizationManager::Role::DEV_LEAD) {
                result->setErrno(errcode::ACCESS_DENIED, "仅研发主管及以上角色可删除审查记录");
                break;
            }
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::OrganizationRepoPrsInfoDao::DeleteById(id, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "删除审查记录失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationRepoPrMgr::GetInstance()->del(id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
