#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class ResourceListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<ResourceListServlet> ptr;
    ResourceListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
    bool checkPermession(int32_t system_role);
};

} // namespace servlet
} // namespace blog
