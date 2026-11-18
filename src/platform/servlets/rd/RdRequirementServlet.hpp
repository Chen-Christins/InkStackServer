#pragma once

#include "common/frame/Struct.hpp"
#include <json/json.h>

namespace blog {
namespace servlet {

class RdRequirementServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdRequirementServlet> ptr;
    RdRequirementServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
