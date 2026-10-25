/**
 * @file auth_service.h
 * @brief 认证服务 — 统一处理会话签发、解析、撤销与旧 token 兼容
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <sstream>
#include <string>

#include <chen/http/http.h>
#include <chen/http/http_session.h>
#include <chen/config/config.h>

#include "auth_context.h"

struct AuthConf {
    bool cookie_secure = false;
    std::string cookie_same_site = "Lax";
    // 会话绝对有效期（秒），<=0 表示不限制
    int64_t session_absolute_ttl = 604800;
    // 迁移期兼容 WebSocket 的旧 token query 认证
    bool ws_legacy_token = false;

    bool operator==(const AuthConf& oth) const
    {
        return cookie_secure == oth.cookie_secure && cookie_same_site == oth.cookie_same_site && session_absolute_ttl == oth.session_absolute_ttl
            && ws_legacy_token == oth.ws_legacy_token;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, AuthConf> {
public:
    AuthConf operator()(const std::string& value)
    {
        AuthConf conf;
        YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["cookie_secure"]) {
            conf.cookie_secure = LexicalCast<std::string, bool>()(node["cookie_secure"].Scalar());
        }
        if (node["cookie_same_site"]) {
            conf.cookie_same_site = node["cookie_same_site"].as<std::string>();
        }
        if (node["session_absolute_ttl"]) {
            conf.session_absolute_ttl = node["session_absolute_ttl"].as<int64_t>();
        }
        if (node["ws_legacy_token"]) {
            conf.ws_legacy_token = LexicalCast<std::string, bool>()(node["ws_legacy_token"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<AuthConf, std::string> {
public:
    std::string operator()(const AuthConf& cfg)
    {
        YAML::Node node = {};
        node["cookie_secure"] = cfg.cookie_secure;
        node["cookie_same_site"] = cfg.cookie_same_site;
        node["session_absolute_ttl"] = cfg.session_absolute_ttl;
        node["ws_legacy_token"] = cfg.ws_legacy_token;

        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

namespace blog {

/**
 * @brief 会话签发选项
 */
struct IssueOptions {
    /// 是否更新用户最近登录时间
    bool update_login_time = true;
};

class AuthService {
public:
    /**
     * @brief 解析当前请求的登录态
     * @param request HTTP 请求
     * @param response 用于清理失效 Cookie，可为空
     * @param touch 是否刷新会话活跃时间
     */
    static AuthContext Resolve(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response = nullptr, bool touch = true);

    /**
     * @brief 用旧版 S_UID / S_TOKEN Cookie 换取新的会话 Cookie
     * @return 是否成功升级
     */
    static bool UpgradeLegacySession(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session);

    /**
     * @brief 登录成功后签发会话
     * @return 0 成功，否则返回业务错误码
     */
    static int32_t IssueSession(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        data::UserInfo::ptr user,
        const IssueOptions& options = IssueOptions());

    /**
     * @brief 撤销当前会话（登出）
     */
    static void RevokeSession(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, const AuthContext& ctx);

    /**
     * @brief 撤销用户全部会话（改密、封禁、删除等场景）
     */
    static void RevokeAllSessions(int64_t uid);

    /**
     * @brief WebSocket 握手认证：优先 Cookie 会话，兼容旧 query token
     */
    static bool ResolveWs(chen::http::HttpRequest::ptr request, int64_t& uid, data::UserInfo::ptr& user);

    /**
     * @brief 写入 SESSIONID Cookie（HttpOnly / SameSite）
     */
    static void SetSessionCookie(chen::http::HttpResponse::ptr response, const std::string& session_id, int64_t max_age);

    /**
     * @brief 清除 SESSIONID Cookie
     */
    static void ClearSessionCookie(chen::http::HttpResponse::ptr response);

    /**
     * @brief 将会话映射到框架 SessionData，兼容 2FA 临时密钥等旧逻辑
     */
    static void MirrorSessionData(const std::string& session_id, int64_t uid);
};

} // namespace blog
