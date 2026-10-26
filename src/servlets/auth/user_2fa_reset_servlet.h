/**
 * @file user_2fa_reset_servlet.h
 * @brief 重置2FA — 使用恢复码重置密钥（手机丢失场景）
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class User2faResetServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2faResetServlet> ptr;
    User2faResetServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
