#include "user/servlets/user/UserExistsServlet.hpp"

#include <chen/log/log.h>

#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserExistsServlet::UserExistsServlet()
    : BlogServlet("UserExistsServlet")
{
}

int32_t UserExistsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, auth_id, "auth_id");
        data::UserInfo::ptr info;
        if (IsEmail(auth_id)) {
            info = UserMgr::GetInstance()->getByEmail(auth_id);
        } else if (IsValidAccount(auth_id)) {
            info = UserMgr::GetInstance()->getByAccount(auth_id);
        } else {
            result->setErrno(errcode::AUTH_CODE_INVALID);
            break;
        }
        result->setErrno(errcode::SUCCESS);
        result->set("is_exists", info ? "1" : "0");
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
