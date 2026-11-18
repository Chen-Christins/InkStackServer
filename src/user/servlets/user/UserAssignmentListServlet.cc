#include "user/servlets/user/UserAssignmentListServlet.hpp"

#include <chen/log/log.h>

#include "user/client/PlatformClient.hpp"

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
        int64_t uid = getUserId(request);

        auto assignments = client::PlatformClientMgr::GetInstance()->listUserAssignments(uid);

        Json::Value list;
        for (auto& assignment : assignments) {
            Json::Value item;
            item["id"] = assignment.Id;
            item["name"] = assignment.Name;
            item["subject_name"] = assignment.SubjectName;
            item["end_time"] = assignment.Deadline;
            item["status"] = assignment.Status; // 1 - 正常 2 - 关闭
            item["priority"] = assignment.Priority;
            item["create_time"] = assignment.CreateTime;
            item["description"] = assignment.Description;
            item["file_type"] = assignment.FileType;
            item["file_size"] = assignment.MaxSize;
            list.append(item);
        }
        result->set("list", list);
        result->set("total", list.size());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
