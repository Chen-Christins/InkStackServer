/**
 * @file user_oauth_github_callback_servlet.h
 * @brief GitHub OAuth 回调 — 换取 GitHub 用户并签发本站登录态
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class UserOauthGithubCallbackServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserOauthGithubCallbackServlet> ptr;
    UserOauthGithubCallbackServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
