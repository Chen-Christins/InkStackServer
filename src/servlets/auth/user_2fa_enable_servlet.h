/**
 * @file user_2fa_enable_servlet.h
 * @brief 启用2FA — 生成TOTP密钥和OTP URI
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faEnableServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2faEnableServlet> ptr;
    User2faEnableServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
