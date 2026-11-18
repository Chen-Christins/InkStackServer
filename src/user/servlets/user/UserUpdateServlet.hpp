#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class UserUpdateServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserUpdateServlet> ptr;
    UserUpdateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
