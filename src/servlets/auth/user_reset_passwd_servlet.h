#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class UserResetPasswdServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserResetPasswdServlet> ptr;
    UserResetPasswdServlet();

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
