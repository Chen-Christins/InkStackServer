#include "auth_service.h"

#include <chen/config/config.h>
#include <chen/http/session_data.h>
#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include "../error_codes.h"
#include "../manager/session_manager.h"
#include "../manager/user_manager.h"
#include "../struct.h"
#include "../util.h"
#include "session_store.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<AuthConf>::ptr g_auth_conf = chen::Config::Lookup("auth", AuthConf(), "Auth Configuration");

static std::string NormalizeSameSite()
{
    const AuthConf& conf = g_auth_conf->getValue();
    std::string value = conf.cookie_same_site;
    if (value != "Lax" && value != "Strict" && value != "None") {
        return "Lax";
    }
    return value;
}

static bool UseSecureCookie()
{
    const AuthConf& conf = g_auth_conf->getValue();
    return conf.cookie_secure || NormalizeSameSite() == "None";
}

void AuthService::MirrorSessionData(const std::string& session_id, int64_t uid)
{
    if (session_id.empty() || !uid) {
        return;
    }

    auto manager = chen::http::SessionDataMgr::GetInstance();
    auto data = manager->get(session_id);
    if (!data) {
        data = std::make_shared<chen::http::SessionData>(false);
        data->setId(session_id);
    }
    data->setData(SessionDataKey::USER_ID, uid);
    data->setData(SessionDataKey::ANONYMOUS, 0);
    manager->add(data);
}

AuthContext AuthService::Resolve(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, bool touch)
{
    AuthContext ctx;
    if (!request) {
        return ctx;
    }

    std::string session_id = request->getCookie(CookieKey::SESSION_KEY);
    if (session_id.empty()) {
        return ctx;
    }

    // 匿名会话不参与认证，也不要清除 Cookie（公共接口会用到）。
    auto sdata = chen::http::SessionDataMgr::GetInstance()->get(session_id);
    if (sdata && sdata->getData<int64_t>(SessionDataKey::ANONYMOUS) == 1) {
        return ctx;
    }
    // 内存 SessionData 可能来自旧模块，需以 SessionStore 为准再判断匿名。
    SessionInfo info;
    bool info_loaded = false;
    if (sdata && sdata->getData<int64_t>(SessionDataKey::USER_ID) == 0) {
        if (!SessionStore::Get(session_id, info)) {
            sdata->setData(SessionDataKey::ANONYMOUS, 1);
            return ctx;
        }
        info_loaded = true;
    }

    if (!info_loaded && !SessionStore::Get(session_id, info)) {
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
        if (response) {
            ClearSessionCookie(response);
        }
        return ctx;
    }

    auto user = UserMgr::GetInstance()->get(info.uid);
    if (!user || user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        SessionStore::Destroy(session_id);
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
        if (response) {
            ClearSessionCookie(response);
        }
        return ctx;
    }

    const AuthConf& conf = g_auth_conf->getValue();

    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (touch && now - info.last_seen >= 60) {
        if (!SessionStore::Touch(session_id, info, GetSessionTTL(), conf.session_absolute_ttl)) {
            chen::http::SessionDataMgr::GetInstance()->del(session_id);
            if (response) {
                ClearSessionCookie(response);
            }
            return ctx;
        }
    }

    MirrorSessionData(session_id, info.uid);
    ctx.uid = info.uid;
    ctx.session_id = session_id;
    ctx.user = user;
    return ctx;
}

int32_t AuthService::IssueSession(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    data::UserInfo::ptr user,
    const IssueOptions& options)
{
    if (!request || !response || !user) {
        return errcode::INTERNAL_ERROR;
    }
    if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        return errcode::ACCOUNT_INVALID;
    }

    const AuthConf& conf = g_auth_conf->getValue();

    int64_t idle_ttl = GetSessionTTL();
    int64_t absolute_ttl = conf.session_absolute_ttl;

    std::string ip;
    if (session) {
        ip = GetRemoteIP(request, session);
    } else {
        ip = request->getHeader("X-Real-IP");
    }
    std::string user_agent = request->getHeader("User-Agent");
    std::string old_session_id = request->getCookie(CookieKey::SESSION_KEY);

    std::string session_id = SessionStore::Create(user->getId(), ip, user_agent, idle_ttl, absolute_ttl);
    if (session_id.empty()) {
        return errcode::REDIS_OPERATION_FAILED;
    }

    if (!old_session_id.empty() && old_session_id != session_id) {
        SessionStore::Destroy(old_session_id);
        chen::http::SessionDataMgr::GetInstance()->del(old_session_id);
    }

    MirrorSessionData(session_id, user->getId());
    int64_t cookie_max_age = absolute_ttl > 0 ? absolute_ttl : 0;
    SetSessionCookie(response, session_id, cookie_max_age);
    request->setCookie(CookieKey::SESSION_KEY, session_id);

    if (options.update_login_time) {
        user->setLoginTime(chen::TimeUtil::GetCurrentSec());
        auto db = GetDB();
        if (db && data::UserInfoDao::Update(user, db) == 0) {
            UserMgr::GetInstance()->update(user);
        } else {
            WARN(logger) << "update login time failed user=" << user->getId();
        }
    }

    INFO(logger) << "session issued user=" << user->getId() << " sid=" << SessionStore::HashId(session_id).substr(0, 12) << " ip=" << ip;
    return 0;
}

void AuthService::RevokeSession(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, const AuthContext& ctx)
{
    std::string session_id = ctx.session_id;
    if (session_id.empty() && request) {
        session_id = request->getCookie(CookieKey::SESSION_KEY);
    }

    if (!session_id.empty()) {
        SessionStore::Destroy(session_id);
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
    }

    ClearSessionCookie(response);
}

void AuthService::RevokeAllSessions(int64_t uid)
{
    if (!uid) {
        return;
    }

    SessionStore::DestroyAll(uid);
    INFO(logger) << "revoked all sessions uid=" << uid;
}

bool AuthService::ResolveWs(chen::http::HttpRequest::ptr request, int64_t& uid, data::UserInfo::ptr& user)
{
    if (!request) {
        return false;
    }

    AuthContext ctx = Resolve(request, nullptr, true);
    if (!ctx.valid()) {
        return false;
    }

    uid = ctx.uid;
    user = ctx.user;
    return true;
}

void AuthService::SetSessionCookie(chen::http::HttpResponse::ptr response, const std::string& session_id, int64_t max_age)
{
    if (!response) {
        return;
    }

    // SDK 的 setCookie 不支持 HttpOnly/SameSite，这里直接写入 Set-Cookie。
    std::string cookie = CookieKey::SESSION_KEY + "=" + session_id + "; Path=/; HttpOnly; SameSite=" + NormalizeSameSite();
    if (UseSecureCookie()) {
        cookie += "; Secure";
    }
    if (max_age > 0) {
        cookie += "; Max-Age=" + std::to_string(max_age);
    }
    response->setHeader("Set-Cookie", cookie);
}

void AuthService::ClearSessionCookie(chen::http::HttpResponse::ptr response)
{
    if (!response) {
        return;
    }

    std::string cookie = CookieKey::SESSION_KEY + "=; Path=/; HttpOnly; SameSite=" + NormalizeSameSite() + "; Max-Age=0";
    if (UseSecureCookie()) {
        cookie += "; Secure";
    }
    response->setHeader("Set-Cookie", cookie);
}

} // namespace blog
