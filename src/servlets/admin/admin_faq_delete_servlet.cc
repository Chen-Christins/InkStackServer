/**
 * @file admin_faq_delete_servlet.cc
 * @brief 管理端 - 删除常见问题实现
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#include "admin_faq_delete_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AdminFaqDeleteServlet::AdminFaqDeleteServlet()
    : BlogLoginedServlet("AdminFaqDeleteServlet")
{
}

int32_t AdminFaqDeleteServlet::handle(
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

        if (!FaqMgr::GetInstance()->remove(id)) {
            result->setErrno(errcode::FAQ_NOT_FOUND);
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFaqDeleteServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
