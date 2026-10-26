/**
 * @file user_2fa_recovery_servlet.h
 * @brief 恢复码登录 — 2FA登录时的备用路径
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faRecoveryServlet : public BlogServlet {
public:
    typedef std::shared_ptr<User2faRecoveryServlet> ptr;
    User2faRecoveryServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
