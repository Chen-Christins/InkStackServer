/**
 * @file user_2fa_disable_servlet.h
 * @brief 禁用2FA — 验证TOTP码后禁用双因素认证
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faDisableServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2faDisableServlet> ptr;
    User2faDisableServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
