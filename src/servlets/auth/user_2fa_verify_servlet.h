/**
 * @file user_2fa_verify_servlet.h
 * @brief 2FA登录验证 — 第二步提交TOTP码完成登录
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faVerifyServlet : public BlogServlet {
public:
    typedef std::shared_ptr<User2faVerifyServlet> ptr;
    User2faVerifyServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
