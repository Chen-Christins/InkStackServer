/**
 * @file AdminFaqEditServlet.hpp
 * @brief 管理端 - 编辑常见问题
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class AdminFaqEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AdminFaqEditServlet> ptr;
    AdminFaqEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
