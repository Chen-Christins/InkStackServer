#include "user_oauth_github_servlet.h"

#include <regex>

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>
#include <chen/util/string_util.h>

#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

static chen::ConfigVar<std::string>::ptr g_github_client_id =
    chen::Config::Lookup("oauth.github.client_id", std::string(""), "GitHub OAuth client id");

static chen::ConfigVar<std::string>::ptr g_github_callback_url =
    chen::Config::Lookup("oauth.github.callback_url", std::string(""), "GitHub OAuth callback url");

/// state 有效期（秒）
static const int kOAuthStateTTL = 600;
/// Redis state key 前缀
static const char* kOAuthStatePrefix = "oauth:github:state:";

UserOauthGithubServlet::UserOauthGithubServlet()
    : BlogServlet("UserOauthGithubServlet")
{
}

/// 仅允许站内相对路径作为登录后回跳地址，防止开放重定向
static std::string NormalizeRedirect(const std::string& raw)
{
    static const std::regex pattern(R"(^/[A-Za-z0-9_\-./?=&%]*$)");
    if (!std::regex_match(raw, pattern)) {
        return "/index";
    }
    if (raw.rfind("//", 0) == 0) {
        return "/index";
    }
    return raw;
}

int32_t UserOauthGithubServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string client_id = g_github_client_id->getValue();
        std::string callback_url = g_github_callback_url->getValue();
        if (client_id.empty() || callback_url.empty()) {
            result->setErrno(errcode::GITHUB_OAUTH_FAILED, "GitHub OAuth 未配置");
            break;
        }

        std::string redirect = NormalizeRedirect(request->getParam("redirect", "/index"));

        std::string state = UserManager::generateToken();
        std::string redisKey = std::string(kOAuthStatePrefix) + state;
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", redisKey.c_str(), kOAuthStateTTL, redirect.c_str());

        std::string authorize_url = "https://github.com/login/oauth/authorize?client_id=" + client_id
            + "&redirect_uri=" + chen::StringUtil::UrlEncode(callback_url, false)
            + "&scope=" + chen::StringUtil::UrlEncode("read:user user:email", false) + "&state=" + state;
        response->setRedirect(authorize_url);
    } while (0);
    if (result->errno_ != 0) {
        response->setBody(result->toJsonString());
    }
    return 0;
}

} // namespace servlet
} // namespace blog
