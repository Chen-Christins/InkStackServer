/**
 * @file passkey_list_servlet.h
 * @brief 查询当前用户的 Passkey 列表
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class PasskeyListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<PasskeyListServlet> ptr;
    PasskeyListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
