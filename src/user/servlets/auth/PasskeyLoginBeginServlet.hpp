/**
 * @file PasskeyLoginBeginServlet.hpp
 * @brief Passkey 登录第一步 — 生成 PublicKeyCredentialRequestOptions
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class PasskeyLoginBeginServlet : public BlogServlet {
public:
    typedef std::shared_ptr<PasskeyLoginBeginServlet> ptr;
    PasskeyLoginBeginServlet();

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
