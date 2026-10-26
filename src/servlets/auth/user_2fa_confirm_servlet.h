/**
 * @file user_2fa_confirm_servlet.h
 * @brief 确认2FA — 用户输入TOTP验证码完成绑定
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faConfirmServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2faConfirmServlet> ptr;
    User2faConfirmServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
