#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class CategoryQueryServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<CategoryQueryServlet> ptr;
    CategoryQueryServlet();

protected:
    bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

    virtual bool isAdminRoute() const { return false; }
};

/**
 * @brief 管理端分类查询（默认返回全部用户分类）
 */
class CategoryAdminQueryServlet : public CategoryQueryServlet {
public:
    typedef std::shared_ptr<CategoryAdminQueryServlet> ptr;

protected:
    bool isAdminRoute() const override { return true; }
};

} // namespace servlet
} // namespace blog
