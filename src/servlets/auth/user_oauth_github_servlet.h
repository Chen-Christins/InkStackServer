/**
 * @file user_oauth_github_servlet.h
 * @brief GitHub OAuth 登录入口 — 生成 state 并重定向到 GitHub 授权页
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class UserOauthGithubServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserOauthGithubServlet> ptr;
    UserOauthGithubServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
