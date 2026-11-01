#include "rd_task_servlet.h"

#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/task_manager.h"
#include "../../manager/user_manager.h"
#include "../../permission.h"
#include "rd_helper.h"

namespace blog {
namespace servlet {

RdTaskServlet::RdTaskServlet()
    : BlogLoginedServlet("RdTaskServlet")
{
}

int32_t RdTaskServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
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

        if (id) {
            auto info = TaskMgr::GetInstance()->get(id);
            if (!info || info->getIsDeleted()) {
                result->setErrno(errcode::TASK_NOT_FOUND);
                break;
            }
            int64_t info_org_id = info->getOrgId();
            if (!is_platform_admin) {
                auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(info_org_id, uid);
                if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                    result->setErrno(errcode::ACCESS_DENIED);
                    break;
                }
                bool is_related = (uid == info->getCreatorId() || uid == info->getAssigneeId());
                if (!permission::CanViewTask(rel->getRole(), is_related)) {
                    result->setErrno(errcode::ACCESS_DENIED);
                    break;
                }
            }
            Json::Value data;
            rd::BuildTaskJson(data, info);
            result->set("data", data);
            break;
        }

        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
        uint64_t offset, size;
        rd::ParsePagination(page, page_size, offset, size);

        std::string search = request->getParam("search");
        std::string status_str = request->getParam("status");
        std::string priority_str = request->getParam("priority");
        int64_t filter_assignee_id = request->getParamAs<int64_t>("assignee_id", 0);

        std::vector<data::TaskInfo::ptr> all;
        if (is_platform_admin && org_id) {
            TaskMgr::GetInstance()->listByOrg(all, org_id, 0, INT32_MAX, -1, true);
        } else if (!is_platform_admin) {
            std::vector<data::OrganizationUserRelInfo::ptr> user_orgs;
            OrganizationUserRelMgr::GetInstance()->getOrgByUserId(user_orgs, uid, OrganizationUserRelManager::Status::APPROVED, true);
            for (auto& rel : user_orgs) {
                if (org_id && rel->getOrgId() != org_id) {
                    continue;
                }
                std::vector<data::TaskInfo::ptr> org_tasks;
                TaskMgr::GetInstance()->listByOrg(org_tasks, rel->getOrgId(), 0, INT32_MAX, -1, true);
                for (auto& task : org_tasks) {
                    bool is_related = (uid == task->getCreatorId() || uid == task->getAssigneeId());
                    if (permission::CanViewTask(rel->getRole(), is_related)) {
                        all.push_back(task);
                    }
                }
            }
        } else {
            TaskMgr::GetInstance()->listByPages(all, 0, INT32_MAX, -1, true);
        }

        std::vector<data::TaskInfo::ptr> filtered;
        for (auto& info : all) {
            if (!status_str.empty() && rd::StringToTaskStatus(status_str) != info->getStatus()) {
                continue;
            }
            if (!priority_str.empty() && rd::StringToPriority(priority_str) != info->getPriority()) {
                continue;
            }
            if (filter_assignee_id && info->getAssigneeId() != filter_assignee_id) {
                continue;
            }
            if (!search.empty()) {
                std::string title = info->getTitle();
                if (title.find(search) == std::string::npos) {
                    continue;
                }
            }
            filtered.push_back(info);
        }

        uint64_t total = filtered.size();
        Json::Value arr(Json::arrayValue);
        for (uint64_t i = offset; i < filtered.size() && arr.size() < size; ++i) {
            Json::Value item;
            rd::BuildTaskJson(item, filtered[i]);
            arr.append(item);
        }
        result->set("total", total);
        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
