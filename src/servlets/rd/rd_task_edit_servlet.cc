#include "rd_task_edit_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/task_manager.h"
#include "../../permission.h"
#include "../../util.h"
#include "rd_helper.h"

namespace blog {
namespace servlet {

RdTaskEditServlet::RdTaskEditServlet()
    : BlogLoginedServlet("RdTaskEditServlet")
{
}

int32_t RdTaskEditServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
            result->setErrno(errcode::ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = rel->getRole();

        data::TaskInfo::ptr info;
        if (id) {
            info = TaskMgr::GetInstance()->get(id);
            if (!info || info->getOrgId() != org_id) {
                result->setErrno(errcode::TASK_NOT_FOUND);
                break;
            }
            if (!permission::CanEditTask(org_role, uid, info->getAssigneeId())) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        } else {
            if (!permission::CanCreateTask(org_role)) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            info.reset(new data::TaskInfo);
            info->setOrgId(org_id);
            info->setCreatorId(uid);
            info->setCreateTime(time(0));
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");
        if (!title.empty()) {
            info->setTitle(title);
        }

        std::string desc = request->getParam("description");
        if (!desc.empty()) {
            info->setDescription(desc);
        }

        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            int32_t p = rd::StringToPriority(priority_str);
            if (p >= 0) {
                info->setPriority(p);
            }
        }

        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            int32_t s = rd::StringToTaskStatus(status_str);
            if (s >= 0) {
                info->setStatus(s);
            }
        }

        int64_t assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        if (assignee_id) {
            info->setAssigneeId(assignee_id);
        }

        int64_t requirement_id = request->getParamAs<int64_t>("requirement_id", 0);
        if (requirement_id) {
            info->setRequirementId(requirement_id);
        }

        int64_t bug_id = request->getParamAs<int64_t>("bug_id", 0);
        if (bug_id) {
            info->setBugId(bug_id);
        }

        int64_t deadline = request->getParamAs<int64_t>("deadline", 0);
        if (deadline) {
            info->setDeadline(deadline);
        }

        int64_t estimated_hours = request->getParamAs<int64_t>("estimated_hours", 0);
        if (estimated_hours) {
            info->setEstimatedHours(estimated_hours);
        }

        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        if (data::TaskInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::RD_UPDATE_FAILED);
            break;
        }
        Json::Value data;
        rd::BuildTaskJson(data, info);
        result->setData(data);

        notifyAssignee(assignee_id, info);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

void RdTaskEditServlet::notifyAssignee(int64_t assignee_id, data::TaskInfo::ptr task)
{
    if (!assignee_id) {
        return;
    }

    EventRdAssignData data = {};
    data.type = "task";
    data.assignee_id = assignee_id;
    data.item_id = task->getId();
    data.item_title = task->getTitle();
    data.creator_id = task->getCreatorId();

    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_RD_ASSIGN, std::move(data));
}

} // namespace servlet
} // namespace blog
