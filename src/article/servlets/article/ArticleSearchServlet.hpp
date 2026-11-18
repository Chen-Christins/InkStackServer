/**
 * @file ArticleSearchServlet.hpp
 * @brief 文章搜索 — 位图索引多条件过滤
 * @author Christins
 * @date 2026-06-30
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class ArticleSearchServlet : public BlogServlet {
public:
    typedef std::shared_ptr<ArticleSearchServlet> ptr;
    ArticleSearchServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
