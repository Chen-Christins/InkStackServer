/**
 * @file passkey_register_begin_servlet.h
 * @brief Passkey 注册第一步 — 生成 PublicKeyCredentialCreationOptions
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class PasskeyRegisterBeginServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<PasskeyRegisterBeginServlet> ptr;
    PasskeyRegisterBeginServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
