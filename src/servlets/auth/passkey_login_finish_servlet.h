/**
 * @file passkey_login_finish_servlet.h
 * @brief Passkey 登录第二步 — 校验断言并签发会话
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class PasskeyLoginFinishServlet : public BlogServlet {
public:
    typedef std::shared_ptr<PasskeyLoginFinishServlet> ptr;
    PasskeyLoginFinishServlet();

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
