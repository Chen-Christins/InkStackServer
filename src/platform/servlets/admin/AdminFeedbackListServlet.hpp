/**
 * @file AdminFeedbackListServlet.hpp
 * @brief 管理端 - 反馈列表查询
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class AdminFeedbackListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AdminFeedbackListServlet> ptr;
    AdminFeedbackListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
