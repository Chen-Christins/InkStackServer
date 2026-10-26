/**
 * @file admin_feedback_convert_servlet.h
 * @brief 管理端 - 反馈转换为常见问题/需求/缺陷
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class AdminFeedbackConvertServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AdminFeedbackConvertServlet> ptr;
    AdminFeedbackConvertServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
