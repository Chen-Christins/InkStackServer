#include "assignment_submission_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/assignment_manager.h"
#include "../../manager/assignment_user_rel_manager.h"
#include "../../manager/resource_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentSubmissionListServlet::AssignmentSubmissionListServlet()
    : BlogLoginedServlet("AssignmentSubmissionListServlet")
{
}

int32_t AssignmentSubmissionListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, assign_id, "assign_id");

        auto assign_info = AssignmentMgr::GetInstance()->get(assign_id);
        if (!assign_info) {
            result->setErrno(errcode::ASSIGNMENT_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);
        auto rel_info = AssignmentUserRelMgr::GetInstance()->getByAssignAndUser(assign_id, uid);
        if (rel_info) {
            auto uinfo = UserMgr::GetInstance()->get(uid);
            result->set("user_id", uid);
            result->set("user_avatar", uinfo->getAvatar());
            result->set("user_name", uinfo->getName());
            result->set("submit_time", rel_info->getSubmitTime());
            result->set("status", rel_info->getStatus());

            // 获取提交的资源信息
            std::string biz_type = "assignment_submission";
            std::vector<data::ResourceInfo::ptr> resources;
            ResourceMgr::GetInstance()->getByBizUid(resources, biz_type, assign_id, uid);

            Json::Value list;
            for (auto& res : resources) {
                Json::Value item;
                item["id"] = res->getId();
                item["file_name"] = res->getName();
                item["file_path"] = res->getPath();
                item["file_size"] = res->getSize();
                list.append(item);
            }
            result->set("list", list);
            result->set("total", resources.size());
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
