/**
 * @file ArticleCalendarServlet.hpp
 * @brief 首页日历接口 - 返回指定月份中有文章的日期列表
 * @author Christins
 * @date 2026-06-14
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class ArticleCalendarServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<ArticleCalendarServlet> ptr;
    ArticleCalendarServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
