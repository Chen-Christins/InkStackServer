/**
 * @file passkey_register_finish_servlet.h
 * @brief Passkey 注册第二步 — 校验并保存凭据
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class PasskeyRegisterFinishServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<PasskeyRegisterFinishServlet> ptr;
    PasskeyRegisterFinishServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
