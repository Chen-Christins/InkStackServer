#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class OrganizationApplyCreateServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationApplyCreateServlet> ptr;
    OrganizationApplyCreateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
