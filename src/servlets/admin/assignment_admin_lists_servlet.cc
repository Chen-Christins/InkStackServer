#include "assignment_admin_lists_servlet.h"

#include "../../manager/assignment_manager.h"
#include "../../manager/assignment_organization_rel_manager.h"
#include "../../manager/organization_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentAdminListsServlet::AssignmentAdminListsServlet()
    : BlogLoginedServlet("AssignmentAdminListsServlet")
{
}

int32_t AssignmentAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t state = request->getParamAs<int32_t>("state", -1);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<data::AssignmentInfo::ptr> infos;
        uint64_t total = AssignmentMgr::GetInstance()->listByPages(infos, offset, page_size, state, true);

        result->set("total", total);
        auto& list = result->jsondata["list"];
        for (const auto& i : infos) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["subject_name"] = i->getSubjectName();
            item["end_time"] = i->getDeadline();
            item["status"] = i->getStatus();
            item["priority"] = i->getPriority();
            item["create_time"] = i->getCreateTime();
            item["description"] = i->getDescription();
            item["file_type"] = i->getFileType();
            item["file_size"] = i->getMaxSize();

            // organization info (负责人)
            std::vector<data::AssignmentOrganizationRelInfo::ptr> org_rels;
            AssignmentOrganizationRelMgr::GetInstance()->getByAssignmentId(org_rels, i->getId());
            if (!org_rels.empty() && org_rels[0]) {
                auto org = OrganizationMgr::GetInstance()->get(org_rels[0]->getOrganizationId());
                if (org) {
                    item["organization_name"] = org->getName();
                }
                item["assigned_by"] = org_rels[0]->getAssignedBy();
            }

            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
