/**
 * @file admin_faq_edit_servlet.cc
 * @brief 管理端 - 编辑常见问题实现
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#include "admin_faq_edit_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AdminFaqEditServlet::AdminFaqEditServlet()
    : BlogLoginedServlet("AdminFaqEditServlet")
{
}

int32_t AdminFaqEditServlet::handle(
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

        std::string q = request->getParam("q");
        std::string a = request->getParam("a");
        std::string cat = request->getParam("cat");

        if (!FaqMgr::GetInstance()->update(id, q, a, cat)) {
            result->setErrno(errcode::FAQ_NOT_FOUND);
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFaqEditServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
