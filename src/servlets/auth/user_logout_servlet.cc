#include "user_logout_servlet.h"

#include "../../auth/auth_service.h"

namespace blog {
namespace servlet {

UserLogoutServlet::UserLogoutServlet()
    : BlogServlet("UserLogoutServlet")
{
}

int32_t UserLogoutServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        // 登出是幂等操作：无论会话是否还在，都清理当前 Cookie 与旧 token
        AuthContext ctx;
        AuthService::RevokeSession(request, response, ctx);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
