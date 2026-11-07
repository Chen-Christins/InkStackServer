#include "organization_repos_sync_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_repo_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposSyncServlet::OrganizationReposSyncServlet()
    : BlogLoginedServlet("OrganizationReposSyncServlet")
{
}

int32_t OrganizationReposSyncServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, repo_id, "repo_id");

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
        std::string url = repo->getUrl();
        std::string token = repo->getToken();

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
                result->setErrno(errcode::ACCESS_DENIED, "仅研发主管及以上角色可触发仓库同步");
                break;
            }
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        // 设置状态为 syncing 后立即返回，异步调 GitHub API
        repo->setSyncStatus("syncing");
        repo->setUpdateTime(time(0));
        if (data::OrganizationReposInfoDao::Update(repo, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "同步失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationRepoMgr::GetInstance()->add(repo);

        // 异步调度 GitHub 同步
        chen::Scheduler::GetThis()->schedule([repo_id, url, token]() {
            OrganizationRepoManager::SyncFromGitHub(repo_id, url, token);
        });
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
