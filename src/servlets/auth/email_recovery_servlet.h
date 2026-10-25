/**
 * @file email_recovery_servlet.h
 * @brief 邮箱验证码恢复登录 — 校验验证码并签发会话
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class EmailRecoveryServlet : public BlogServlet {
public:
    typedef std::shared_ptr<EmailRecoveryServlet> ptr;
    EmailRecoveryServlet();

protected:
    bool allowAutoLogin() const override { return false; }

    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
