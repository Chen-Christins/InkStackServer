#include "user_oauth_github_callback_servlet.h"

#include <map>
#include <regex>
#include <unistd.h>

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/http/http_connection.h>
#include <chen/log/log.h>
#include <chen/util/json_util.h>
#include <chen/util/string_util.h>
#include <chen/util/time_util.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../manager/user_oauth_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

static chen::ConfigVar<std::string>::ptr g_github_client_id =
    chen::Config::Lookup("oauth.github.client_id", std::string(""), "GitHub OAuth client id");

static chen::ConfigVar<std::string>::ptr g_github_client_secret =
    chen::Config::Lookup("oauth.github.client_secret", std::string(""), "GitHub OAuth client secret");

static chen::ConfigVar<std::string>::ptr g_github_callback_url =
    chen::Config::Lookup("oauth.github.callback_url", std::string(""), "GitHub OAuth callback url");

static chen::ConfigVar<std::string>::ptr g_frontend_base = chen::Config::Lookup("oauth.github.frontend_base", std::string(""), "Frontend base url");

/// Redis state key 前缀（与入口 servlet 保持一致）
static const char* kOAuthStatePrefix = "oauth:github:state:";
/// GitHub 平台标识
static const char* kProviderGithub = "github";
/// GitHub OAuth 接口
static const char* kGithubTokenUrl = "https://github.com/login/oauth/access_token";
static const char* kGithubUserUrl = "https://api.github.com/user";
static const char* kGithubEmailsUrl = "https://api.github.com/user/emails";
static const char* kGithubUserAgent = "TechHaven/1.0";

struct GithubProfile {
    std::string open_id;
    std::string login;
    std::string name;
    std::string email;
    std::string avatar;
};

UserOauthGithubCallbackServlet::UserOauthGithubCallbackServlet()
    : BlogServlet("UserOauthGithubCallbackServlet")
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

/// GitHub 请求超时（网络抖动较大，放宽到 30s）
static const uint64_t kGithubTimeoutMs = 30000;
/// GitHub 请求最大尝试次数
static const int kGithubMaxAttempts = 3;

/// 向 GitHub 发起请求，仅对连接失败/超时做重试（退避 0.3s/0.6s）
static chen::http::HttpResult::ptr DoGithubRequestWithRetry(
    bool is_post,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body)
{
    chen::http::HttpResult::ptr result;
    for (int attempt = 0; attempt < kGithubMaxAttempts; attempt++) {
        result = is_post ? chen::http::HttpConnection::DoPost(url, kGithubTimeoutMs, headers, body)
                         : chen::http::HttpConnection::DoGet(url, kGithubTimeoutMs, headers, body);

        bool transient =
            (!result || result->result == (int)chen::http::HttpResult::Error::CONNECT_FAIL
             || result->result == (int)chen::http::HttpResult::Error::TIMEOUT);
        if (!transient) {
            return result;
        }
        ERROR(logger) << "github request transient failure url=" << url << " attempt=" << (attempt + 1)
                      << " result=" << (result ? result->result : -1);
        if (attempt + 1 < kGithubMaxAttempts) {
            usleep(300000 * (attempt + 1));
        }
    }
    return result;
}

/// 用授权码换取 access_token
static bool ExchangeAccessToken(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& code,
    const std::string& callback_url,
    std::string& access_token)
{
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/json";
    headers["User-Agent"] = kGithubUserAgent;

    std::string body = "client_id=" + chen::StringUtil::UrlEncode(client_id, false)
        + "&client_secret=" + chen::StringUtil::UrlEncode(client_secret, false) + "&code=" + chen::StringUtil::UrlEncode(code, false)
        + "&redirect_uri=" + chen::StringUtil::UrlEncode(callback_url, false);

    auto result = DoGithubRequestWithRetry(true, kGithubTokenUrl, headers, body);
    if (!result || result->result != 0 || !result->response || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "ExchangeAccessToken: http failed result=" << (result ? result->result : -1)
                      << " status=" << (result && result->response ? (int)result->response->getStatus() : 0)
                      << " body=" << (result && result->response ? result->response->getBody() : "");
        return false;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, result->response->getBody())) {
        ERROR(logger) << "ExchangeAccessToken: parse json failed";
        return false;
    }
    if (!json.isMember("access_token") || !json["access_token"].isString()) {
        ERROR(logger) << "ExchangeAccessToken: no access_token, body=" << result->response->getBody();
        return false;
    }
    access_token = json["access_token"].asString();
    return !access_token.empty();
}

/// 读取 GitHub 用户信息（id/login/name/email/avatar）
static bool FetchGithubProfile(const std::string& access_token, GithubProfile& profile)
{
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github+json";
    headers["User-Agent"] = kGithubUserAgent;
    headers["Authorization"] = "Bearer " + access_token;

    auto result = DoGithubRequestWithRetry(false, kGithubUserUrl, headers, "");
    if (!result || result->result != 0 || !result->response || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "FetchGithubProfile: http failed result=" << (result ? result->result : -1)
                      << " status=" << (result && result->response ? (int)result->response->getStatus() : 0);
        return false;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, result->response->getBody())) {
        ERROR(logger) << "FetchGithubProfile: parse json failed";
        return false;
    }

    if (json["id"].isInt64()) {
        profile.open_id = std::to_string(json["id"].asInt64());
    } else if (json["id"].isUInt64()) {
        profile.open_id = std::to_string(json["id"].asUInt64());
    } else if (json["id"].isInt()) {
        profile.open_id = std::to_string(json["id"].asInt());
    } else if (json["id"].isUInt()) {
        profile.open_id = std::to_string(json["id"].asUInt());
    } else if (json["id"].isString()) {
        profile.open_id = json["id"].asString();
    }
    if (profile.open_id.empty()) {
        ERROR(logger) << "FetchGithubProfile: missing id";
        return false;
    }
    if (json["login"].isString()) {
        profile.login = json["login"].asString();
    }
    if (json["name"].isString()) {
        profile.name = json["name"].asString();
    }
    if (json["email"].isString()) {
        profile.email = json["email"].asString();
    }
    if (json["avatar_url"].isString()) {
        profile.avatar = json["avatar_url"].asString();
    }
    return true;
}

/// 拉取 GitHub 邮箱列表，返回 primary + verified 的邮箱
static std::string FetchPrimaryEmail(const std::string& access_token)
{
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github+json";
    headers["User-Agent"] = kGithubUserAgent;
    headers["Authorization"] = "Bearer " + access_token;

    auto result = DoGithubRequestWithRetry(false, kGithubEmailsUrl, headers, "");
    if (!result || result->result != 0 || !result->response || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "FetchPrimaryEmail: http failed";
        return "";
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, result->response->getBody()) || !json.isArray()) {
        ERROR(logger) << "FetchPrimaryEmail: parse json failed";
        return "";
    }
    std::string fallback;
    for (const auto& item : json) {
        if (!item["email"].isString()) {
            continue;
        }
        std::string email = item["email"].asString();
        if (email.empty()) {
            continue;
        }
        bool verified = item["verified"].asBool();
        bool primary = item["primary"].asBool();
        if (primary && verified) {
            return email;
        }
        if (verified && fallback.empty()) {
            fallback = email;
        }
    }
    return fallback;
}

/// 生成唯一且符合站点规则的账号名
static std::string BuildAccount(const std::string& open_id, const std::string& login)
{
    if (IsValidAccount(login) && !UserMgr::GetInstance()->getByAccount(login)) {
        return login;
    }
    std::string base = "gh_" + open_id;
    if (!IsValidAccount(base)) {
        base = "gh" + open_id;
    }
    if (base.size() > 16) {
        base = base.substr(0, 16);
    }
    if (!IsValidAccount(base)) {
        base = "ghuser";
    }
    std::string candidate = base;
    int suffix = 1;
    while (UserMgr::GetInstance()->getByAccount(candidate)) {
        std::string s = std::to_string(suffix++);
        std::string prefix = base;
        if (prefix.size() + s.size() > 16) {
            prefix = prefix.substr(0, 16 - s.size());
        }
        candidate = prefix + s;
    }
    return candidate;
}

/// 生成唯一用户名
static std::string BuildName(const std::string& open_id, const std::string& uname, const std::string& login)
{
    std::string base = !uname.empty() ? uname : login;
    base = chen::StringUtil::Trim(base);
    if (base.empty()) {
        base = "github_" + open_id;
    }
    if (!UserMgr::GetInstance()->getByName(base)) {
        return base;
    }
    std::string candidate = base + "_" + open_id;
    int suffix = 1;
    while (UserMgr::GetInstance()->getByName(candidate)) {
        candidate = base + "_" + open_id + "_" + std::to_string(suffix++);
    }
    return candidate;
}

/// 重定向到前端登录页并携带错误码
static void RedirectError(chen::http::HttpResponse::ptr response, const std::string& frontend_base, int32_t code)
{
    if (code == 0) {
        code = errcode::GITHUB_OAUTH_FAILED;
    }
    response->setRedirect(frontend_base + "/auth?oauth_error=" + std::to_string(code));
}

int32_t UserOauthGithubCallbackServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    std::string client_id = g_github_client_id->getValue();
    std::string client_secret = g_github_client_secret->getValue();
    std::string callback_url = g_github_callback_url->getValue();
    std::string frontend_base = g_frontend_base->getValue();

    if (client_id.empty() || client_secret.empty() || callback_url.empty() || frontend_base.empty()) {
        result->setErrno(errcode::GITHUB_OAUTH_FAILED, "GitHub OAuth 未配置");
        response->setBody(result->toJsonString());
        return 0;
    }
    while (frontend_base.size() > 1 && frontend_base.back() == '/') {
        frontend_base.pop_back();
    }

    std::string code = request->getParam("code");
    std::string state = request->getParam("state");
    std::string redirect = "/index";
    int32_t error = 0;
    bool success = false;

    do {
        if (code.empty() || state.empty()) {
            error = errcode::GITHUB_STATE_INVALID;
            break;
        }

        // 校验并一次性消费 state
        std::string stateKey = std::string(kOAuthStatePrefix) + state;
        auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", stateKey.c_str());
        if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
            error = errcode::GITHUB_STATE_INVALID;
            break;
        }
        redirect = NormalizeRedirect(std::string(rpy->str, rpy->len));
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", stateKey.c_str());

        // 换取 GitHub 身份
        std::string access_token;
        if (!ExchangeAccessToken(client_id, client_secret, code, callback_url, access_token)) {
            error = errcode::GITHUB_OAUTH_FAILED;
            break;
        }
        GithubProfile profile;
        if (!FetchGithubProfile(access_token, profile)) {
            error = errcode::GITHUB_OAUTH_FAILED;
            break;
        }
        if (profile.email.empty()) {
            profile.email = FetchPrimaryEmail(access_token);
        }

        // 查询或创建本地用户
        data::UserInfo::ptr user;
        auto binding = UserOAuthMgr::GetInstance()->getByOpenId(kProviderGithub, profile.open_id);
        if (binding) {
            user = UserMgr::GetInstance()->get(binding->getUserId());
            if (!user) {
                error = errcode::USER_NOT_FOUND;
                break;
            }
        }

        if (!user && !profile.email.empty()) {
            user = UserMgr::GetInstance()->getByEmail(profile.email);
            if (user) {
                UserOAuthMgr::GetInstance()->bind(user->getId(), kProviderGithub, profile.open_id, profile.login);
            }
        }

        if (!user) {
            auto db = getDB();
            if (!db) {
                error = errcode::DB_OPERATION_FAILED;
                break;
            }
            std::string email = profile.email;
            if (email.empty()) {
                email = profile.open_id + "+" + profile.login + "@users.noreply.github.com";
            }
            user = UserMgr::GetInstance()->getByEmail(email);
            if (!user) {
                user = std::make_shared<data::UserInfo>();
                user->setAccount(BuildAccount(profile.open_id, profile.login));
                user->setName(BuildName(profile.open_id, profile.name, profile.login));
                user->setEmail(email);
                user->setAvatar(profile.avatar);
                if (!profile.login.empty()) {
                    user->setGithub("https://github.com/" + profile.login);
                }
                // GitHub 用户无本地密码，写入不可用的随机值
                user->setPasswd(HashPassword(UserManager::generateToken()));
                user->setRole(UserManager::Role::USER);
                user->setState(UserManager::Status::ACTIVE);
                user->setCreateTime(chen::TimeUtil::GetCurrentSec());
                user->setUpdateTime(chen::TimeUtil::GetCurrentSec());
                if (data::UserInfoDao::Insert(user, db)) {
                    ERROR(logger) << "callback: insert user failed"
                                  << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
                    error = errcode::DB_OPERATION_FAILED;
                    break;
                }
                UserMgr::GetInstance()->add(user);
            }
            UserOAuthMgr::GetInstance()->bind(user->getId(), kProviderGithub, profile.open_id, profile.login);
        }

        if (!user) {
            error = errcode::USER_NOT_FOUND;
            break;
        }
        if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
            error = errcode::ACCOUNT_INVALID;
            break;
        }

        // 签发登录态（OAuth 用户跳过 2FA）
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, user, options);
        if (ret != 0) {
            error = ret;
            break;
        }
        INFO(logger) << "github oauth login user=" << user->getId() << " login=" << profile.login;

        success = true;
    } while (0);

    if (!success) {
        RedirectError(response, frontend_base, error);
        return 0;
    }

    std::string sep = redirect.find('?') == std::string::npos ? "?" : "&";
    response->setRedirect(frontend_base + redirect + sep + "oauth=1");
    return 0;
}

} // namespace servlet
} // namespace blog
