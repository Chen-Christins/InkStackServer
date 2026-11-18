#include "platform/servlets/assignment/AssignmentDetailServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "platform/manager/AssignmentManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentDetailServlet::AssignmentDetailServlet()
    : BlogLoginedServlet("AssignmentDetailServlet")
{
}

int32_t AssignmentDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        auto assignment = AssignmentMgr::GetInstance()->get(id);

        if (!assignment || assignment->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        result->set("id", assignment->getId());
        result->set("name", assignment->getName());
        result->set("status", assignment->getStatus()); // 1 - 正常 2 - 关闭
        result->set("priority", assignment->getPriority());
        result->set("subject_name", assignment->getSubjectName());
        result->set("description", assignment->getDescription());
        result->set("end_time", assignment->getDeadline());
        result->set("file_size", assignment->getMaxSize());
        result->set("file_type", assignment->getFileType());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
