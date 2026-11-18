#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class UserCreateServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserCreateServlet> ptr;
    UserCreateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

    bool verificationEmailCode(const std::string& email, const std::string& code);
};

} // namespace servlet
} // namespace blog
