#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class CategoryDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<CategoryDeleteServlet> ptr;
    CategoryDeleteServlet();

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
 * @brief 管理端分类删除（可删除任意用户的分类）
 */
class CategoryAdminDeleteServlet : public CategoryDeleteServlet {
public:
    typedef std::shared_ptr<CategoryAdminDeleteServlet> ptr;

protected:
    bool isAdminRoute() const override { return true; }
};

} // namespace servlet
} // namespace blog
