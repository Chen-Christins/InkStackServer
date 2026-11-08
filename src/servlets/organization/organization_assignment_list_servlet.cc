#include "organization_assignment_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/assignment_manager.h"
#include "../../manager/assignment_organization_rel_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../permission.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationAssignmentListServlet::OrganizationAssignmentListServlet()
    : BlogLoginedServlet("OrganizationAssignmentListServlet")
{
}

int32_t OrganizationAssignmentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        // -1 all, 0 draft, 1 active, 2 closed(inactive)
        int32_t status = request->getParamAs<int32_t>("status", -1);

        // check operator permission
        auto uid = getUserId(request);
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
        if (system_role != UserManager::Role::ADMIN) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel) {
                result->setErrno(errcode::ORG_NOT_MEMBER);
                break;
            }

            int32_t org_role = rel->getRole();
            if (!permission::CanManageMembers(system_role, org_role)) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        }

        uint64_t offset = (page_num - 1) * page_size;

        // get assignments in organization
        std::vector<data::AssignmentOrganizationRelInfo::ptr> assign_rels;
        int64_t total = AssignmentOrganizationRelMgr::GetInstance()->getByPages(assign_rels, org_id, offset, page_size, status, true);

        Json::Value list;
        for (auto& i : assign_rels) {
            Json::Value item;
            auto assign = AssignmentMgr::GetInstance()->get(i->getAssignmentId());
            if (assign->getIsDeleted()) {
                continue;
            }
            auto assign_org_rel = AssignmentOrganizationRelMgr::GetInstance()->getByOrgAndAssign(org_id, i->getAssignmentId());
            item["id"] = i->getId();
            item["assign_id"] = i->getAssignmentId();
            item["assigned_by"] = assign_org_rel->getAssignedBy();
            item["name"] = assign->getName();
            item["subject_name"] = assign->getSubjectName();
            item["end_time"] = assign->getDeadline();
            item["max_size"] = assign->getMaxSize();
            item["status"] = assign->getStatus();
            item["priority"] = assign->getPriority();
            item["file_type"] = assign->getFileType();
            item["description"] = assign->getDescription();
            list.append(item);
        }
        result->set("list", list);
        result->set("total", std::min(total, static_cast<int64_t>(list.size())));
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
