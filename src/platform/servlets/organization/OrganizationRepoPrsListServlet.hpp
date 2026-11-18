/**
 * @file OrganizationRepoPrsListServlet.hpp
 * @brief 组织仓库 PR 列表接口
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class OrganizationRepoPrsListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationRepoPrsListServlet> ptr;
    OrganizationRepoPrsListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
