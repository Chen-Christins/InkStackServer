/**
 * @file OrganizationRepoPrsDeleteServlet.hpp
 * @brief 组织仓库 PR 删除接口
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class OrganizationRepoPrsDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationRepoPrsDeleteServlet> ptr;
    OrganizationRepoPrsDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
