/**
 * @file passkey_delete_servlet.h
 * @brief 删除当前用户的 Passkey
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class PasskeyDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<PasskeyDeleteServlet> ptr;
    PasskeyDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
