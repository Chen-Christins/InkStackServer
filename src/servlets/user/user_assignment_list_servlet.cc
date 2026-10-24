#include "user_assignment_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/assignment_manager.h"
#include "../../manager/assignment_organization_rel_manager.h"
#include "../../manager/organization_user_rel_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAssignmentListServlet::UserAssignmentListServlet()
    : BlogLoginedServlet("UserAssignmentListServlet")
{
}

int32_t UserAssignmentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        // 获取当前用户
        auto uid = getUserId(request);

        // 当前用户的所有组织关系
        std::vector<data::OrganizationUserRelInfo::ptr> user_org_rels;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(user_org_rels, uid, -1, true);

        if (user_org_rels.empty()) {
            result->set("total", 0);
            result->set("list", Json::Value(Json::arrayValue));
            break;
        }

        // 获取所有作业-组织关系
        auto& list = result->jsondata["list"];
        for (auto& org_rel : user_org_rels) {
            // 该组织下的所有作业关系
            std::vector<data::AssignmentOrganizationRelInfo::ptr> assignment_org_rels;
            AssignmentOrganizationRelMgr::GetInstance()->getByPages(assignment_org_rels, org_rel->getOrgId(), 0, INT32_MAX, -1, true);
            for (auto& assign_org_rel : assignment_org_rels) {
                // 把作业信息加入结果集中
                int64_t assign_id = assign_org_rel->getAssignmentId();
                auto assign_info = AssignmentMgr::GetInstance()->get(assign_id);
                // 只返回激活状态的作业
                if (!assign_info || assign_info->getStatus() == AssignmentManager::Status::DRAFT || assign_info->getIsDeleted()) {
                    continue;
                }
                Json::Value item;
                item["id"] = assign_info->getId();
                item["name"] = assign_info->getName();
                item["subject_name"] = assign_info->getSubjectName();
                item["end_time"] = assign_info->getDeadline();
                item["status"] = assign_info->getStatus(); // 1 - 正常 2 - 关闭
                item["priority"] = assign_info->getPriority();
                item["create_time"] = assign_info->getCreateTime();
                item["description"] = assign_info->getDescription();
                item["file_type"] = assign_info->getFileType();
                item["file_size"] = assign_info->getMaxSize();
                list.append(item);
            }
        }
        result->set("total", list.size());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
