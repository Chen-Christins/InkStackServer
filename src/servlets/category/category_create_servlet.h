#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class CategoryCreateServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<CategoryCreateServlet> ptr;
    CategoryCreateServlet();

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
 * @brief 管理端分类创建/编辑（可为任意用户操作）
 */
class CategoryAdminCreateServlet : public CategoryCreateServlet {
public:
    typedef std::shared_ptr<CategoryAdminCreateServlet> ptr;

protected:
    bool isAdminRoute() const override { return true; }
};

} // namespace servlet
} // namespace blog
