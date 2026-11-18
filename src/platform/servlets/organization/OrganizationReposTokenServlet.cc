#include "platform/servlets/organization/OrganizationReposTokenServlet.hpp"

#include <chen/log/log.h>

#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationRepoManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposTokenServlet::OrganizationReposTokenServlet()
    : BlogLoginedServlet("OrganizationReposTokenServlet")
{
}

int32_t OrganizationReposTokenServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, repo_id, "repo_id");
        DEFINE_AND_CHECK_STRING(result, token, "token");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // 检查仓库是否存在
        auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
        if (!repo) {
            result->setErrno(errcode::ORG_REPO_NOT_FOUND);
            break;
        }
        int64_t org_id = repo->getOrgId();

        // 权限检查：系统管理员或组织内研发主管及以上
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
                result->setErrno(errcode::ACCESS_DENIED, "仅研发主管及以上角色可管理仓库 Token");
                break;
            }
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        repo->setToken(token);
        repo->setUpdateTime(time(0));
        if (data::OrganizationReposInfoDao::Update(repo, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "保存 Token 失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }
        OrganizationRepoMgr::GetInstance()->add(repo);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
