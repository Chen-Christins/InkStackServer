/**
 * @file admin_feedback_delete_servlet.h
 * @brief 管理端 - 删除反馈
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class AdminFeedbackDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AdminFeedbackDeleteServlet> ptr;
    AdminFeedbackDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
