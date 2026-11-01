/**
 * @file organization_repo_prs_sync_servlet.cc
 * @brief 组织仓库 PR 同步接口实现（repo_id 全量 / id 单条）
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "organization_repo_prs_sync_servlet.h"

#include <chen/iomanager/worker.h>
#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_repo_manager.h"
#include "../../manager/organization_repo_pr_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

/// 将单条同步状态映射为业务错误码（OK 不处理）
static void MapSyncPrError(OrganizationRepoPrManager::SyncPrStatus st, Result::ptr result)
{
    switch (st) {
    case OrganizationRepoPrManager::SyncPrStatus::PR_NOT_FOUND:
        result->setErrno(errcode::ORG_PR_NOT_FOUND);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::REPO_NOT_FOUND:
        result->setErrno(errcode::ORG_REPO_NOT_FOUND);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::NO_TOKEN:
        result->setErrno(errcode::ORG_REPO_TOKEN_MISSING);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::GITHUB_NOT_FOUND:
        result->setErrno(errcode::GITHUB_PR_NOT_FOUND);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::RATE_LIMITED:
        result->setErrno(errcode::GITHUB_RATE_LIMITED);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::BUSY:
        result->setErrno(errcode::ORG_PR_SYNC_IN_PROGRESS);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::HTTP_ERROR:
        result->setErrno(errcode::GITHUB_API_FAILED);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::DB_ERROR:
        result->setErrno(errcode::DB_OPERATION_FAILED);
        break;
    case OrganizationRepoPrManager::SyncPrStatus::OK:
    default:
        break;
    }
}

OrganizationRepoPrsSyncServlet::OrganizationRepoPrsSyncServlet()
    : BlogLoginedServlet("OrganizationRepoPrsSyncServlet")
{
}

int32_t OrganizationRepoPrsSyncServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        int64_t repo_id = request->getParamAs<int64_t>("repo_id", 0);
        if (!id && !repo_id) {
            result->setErrno(errcode::PARAM_MISSING, "param id or repo_id is required");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // 传 id 时定位到其所属仓库
        if (id) {
            auto pr = OrganizationRepoPrMgr::GetInstance()->get(id);
            if (!pr) {
                result->setErrno(errcode::ORG_PR_NOT_FOUND);
                break;
            }
            repo_id = pr->getRepoId();
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
                result->setErrno(errcode::ACCESS_DENIED, "仅研发主管及以上角色可同步 PR");
                break;
            }
        }

        if (id) {
            // 单条 PR 同步：同步执行，便于直接返回 GitHub 错误
            if (token.empty()) {
                result->setErrno(errcode::ORG_REPO_TOKEN_MISSING);
                break;
            }

            data::OrganizationRepoPrsInfo::ptr updated;
            auto st = OrganizationRepoPrManager::SyncPrFromGitHub(id, updated);
            if (st != OrganizationRepoPrManager::SyncPrStatus::OK) {
                MapSyncPrError(st, result);
                break;
            }

            Json::Value item;
            item["id"] = updated->getId();
            item["repo_id"] = updated->getRepoId();
            item["github_pr_id"] = updated->getGithubPrId();
            item["title"] = updated->getTitle();
            item["description"] = updated->getDescription();
            item["state"] = updated->getState();
            item["priority"] = updated->getPriority();
            item["author"] = updated->getAuthor();
            item["head_branch"] = updated->getHeadBranch();
            item["base_branch"] = updated->getBaseBranch();
            item["commit_sha"] = updated->getCommitSha().substr(0, 7);
            item["changed_files"] = updated->getChangedFiles();
            item["additions"] = updated->getAdditions();
            item["deletions"] = updated->getDeletions();
            item["reviewers"] = updated->getReviewers();
            item["review_status"] = updated->getReviewStatus();
            item["closed_at"] = updated->getClosedAt();
            item["merged_at"] = updated->getMergedAt();
            item["created_at"] = updated->getCreateTime();
            item["updated_at"] = updated->getUpdateTime();
            result->setData(item);
            break;
        }

        // 异步调度全量同步
        chen::Scheduler::GetThis()->schedule([repo_id, url, token]() {
            OrganizationRepoPrManager::SyncFromGitHub(repo_id, url, token);
        });
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
