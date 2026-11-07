#include "rd_requirement_edit_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/requirement_manager.h"
#include "../../permission.h"
#include "../../util.h"
#include "rd_helper.h"

namespace blog {
namespace servlet {

RdRequirementEditServlet::RdRequirementEditServlet()
    : BlogLoginedServlet("RdRequirementEditServlet")
{
}

int32_t RdRequirementEditServlet::handle(
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

        data::RequirementInfo::ptr info;

        if (id) {
            info = RequirementMgr::GetInstance()->get(id);
            if (!info || info->getOrgId() != org_id) {
                result->setErrno(errcode::REQUIREMENT_NOT_FOUND);
                break;
            }
            if (!permission::CanEditRequirement(org_role)) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        } else {
            if (!permission::CanCreateRequirement(org_role)) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            info.reset(new data::RequirementInfo);
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
            int32_t s = rd::StringToRequirementStatus(status_str);
            if (s >= 0) {
                info->setStatus(s);
            }
        }

        int64_t assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        if (assignee_id) {
            info->setAssigneeId(assignee_id);
        }

        int64_t deadline = request->getParamAs<int64_t>("deadline", 0);
        if (deadline) {
            info->setDeadline(deadline);
        }

        std::string iteration = request->getParam("iteration");
        if (!iteration.empty()) {
            info->setIteration(iteration);
        }

        std::string category = request->getParam("category");
        if (!category.empty()) {
            info->setCategory(category);
        }

        std::string source = request->getParam("source");
        if (!source.empty()) {
            info->setSource(source);
        }

        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::RequirementInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::RD_UPDATE_FAILED);
            break;
        }

        Json::Value data;
        rd::BuildRequirementJson(data, info);
        result->setData(data);

        notifyAssignee(assignee_id, info);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

void RdRequirementEditServlet::notifyAssignee(int64_t assignee_id, data::RequirementInfo::ptr requirement)
{
    if (!assignee_id) {
        return;
    }

    EventRdAssignData data = {};
    data.type = "requirement";
    data.assignee_id = assignee_id;
    data.item_id = requirement->getId();
    data.item_title = requirement->getTitle();
    data.creator_id = requirement->getCreatorId();

    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_RD_ASSIGN, std::move(data));
}

} // namespace servlet
} // namespace blog
