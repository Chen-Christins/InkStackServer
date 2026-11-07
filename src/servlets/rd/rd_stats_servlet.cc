#include "rd_stats_servlet.h"

#include "../../manager/bug_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/requirement_manager.h"
#include "../../manager/task_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

RdStatsServlet::RdStatsServlet()
    : BlogLoginedServlet("RdStatsServlet")
{
}

int32_t RdStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
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
        time_t now = time(0);

        std::vector<data::RequirementInfo::ptr> reqs;
        std::vector<data::BugInfo::ptr> bugs;
        std::vector<data::TaskInfo::ptr> tasks;

        if (is_platform_admin) {
            if (org_id) {
                RequirementMgr::GetInstance()->listByOrg(reqs, org_id, 0, INT32_MAX, -1, true);
                BugMgr::GetInstance()->listByOrg(bugs, org_id, 0, INT32_MAX, -1, true);
                TaskMgr::GetInstance()->listByOrg(tasks, org_id, 0, INT32_MAX, -1, true);
            } else {
                RequirementMgr::GetInstance()->listByPages(reqs, 0, INT32_MAX, -1, true);
                BugMgr::GetInstance()->listByPages(bugs, 0, INT32_MAX, -1, true);
                TaskMgr::GetInstance()->listByPages(tasks, 0, INT32_MAX, -1, true);
            }
        } else {
            std::vector<data::OrganizationUserRelInfo::ptr> user_orgs;
            OrganizationUserRelMgr::GetInstance()->getOrgByUserId(user_orgs, uid, OrganizationUserRelManager::Status::APPROVED, true);
            for (auto& rel : user_orgs) {
                if (org_id && rel->getOrgId() != org_id) {
                    continue;
                }
                std::vector<data::RequirementInfo::ptr> oReqs;
                RequirementMgr::GetInstance()->listByOrg(oReqs, rel->getOrgId(), 0, INT32_MAX, -1, true);
                reqs.insert(reqs.end(), oReqs.begin(), oReqs.end());
                std::vector<data::BugInfo::ptr> oBugs;
                BugMgr::GetInstance()->listByOrg(oBugs, rel->getOrgId(), 0, INT32_MAX, -1, true);
                bugs.insert(bugs.end(), oBugs.begin(), oBugs.end());
                std::vector<data::TaskInfo::ptr> oTasks;
                TaskMgr::GetInstance()->listByOrg(oTasks, rel->getOrgId(), 0, INT32_MAX, -1, true);
                tasks.insert(tasks.end(), oTasks.begin(), oTasks.end());
            }
        }

        int total_reqs = 0, open_reqs = 0;
        for (auto& info : reqs) {
            total_reqs++;
            int st = info->getStatus();
            if (st != RequirementManager::STATUS_DONE && st != RequirementManager::STATUS_CLOSED) {
                open_reqs++;
            }
        }

        int total_bugs = 0, unresolved_bugs = 0;
        for (auto& info : bugs) {
            total_bugs++;
            int st = info->getStatus();
            if (st != BugManager::STATUS_FIXED && st != BugManager::STATUS_CLOSED) {
                unresolved_bugs++;
            }
        }

        int total_tasks = 0, overdue_tasks = 0;
        for (auto& info : tasks) {
            total_tasks++;
            int st = info->getStatus();
            if (st != TaskManager::STATUS_DONE && st != TaskManager::STATUS_CLOSED && info->getDeadline() > 0 && info->getDeadline() < now) {
                overdue_tasks++;
            }
        }

        result->set("total_requirements", total_reqs);
        result->set("open_requirements", open_reqs);
        result->set("total_bugs", total_bugs);
        result->set("unresolved_bugs", unresolved_bugs);
        result->set("total_tasks", total_tasks);
        result->set("overdue_tasks", overdue_tasks);

    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
