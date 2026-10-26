#include "admin_feedback_delete_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackDeleteServlet::AdminFeedbackDeleteServlet()
    : BlogLoginedServlet("AdminFeedbackDeleteServlet")
{
}

int32_t AdminFeedbackDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
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

        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, id_str, "id");

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(errcode::PARAM_INVALID, "invalid id");
            break;
        }

        if (!FeedbackMgr::GetInstance()->remove(id)) {
            result->setErrno(errcode::FEEDBACK_NOT_FOUND);
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackDeleteServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
