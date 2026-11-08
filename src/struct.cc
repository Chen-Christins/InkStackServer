#include "struct.h"

#include <cstdarg>
#include <exception>

#include <chen/log/log.h>

#include "auth/auth_service.h"
#include "auth/session_store.h"
#include "blog/data/user_info.h"
#include "error_codes.h"
#include "i18n/i18n.h"
#include "manager/error_code_manager.h"
#include "manager/user_manager.h"
#include "metrics/service_metrics.h"
#include "util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_NAME("access");

const std::string CookieKey::SESSION_KEY = "SESSIONID";
const std::string SessionDataKey::USER_ID = "user_id";
const std::string SessionDataKey::ANONYMOUS = "anonymous";

std::string GetRemoteIP(chen::http::HttpRequest::ptr request, chen::http::HttpSession::ptr session)
{
    auto rt = request->getHeader("X-Real-IP");
    if (!rt.empty()) {
        return rt;
    }
    rt = session->getRemoteAddressString();
    auto pos = rt.find(':');
    return rt.substr(0, pos);
}

Result::Result(int32_t ec, const std::string& m)
    : errno_(ec)
    , used_(chen::TimeUtil::GetCurrentUs())
    , msg_(m)
    , lang_(I18n::DefaultLanguage())
{
}

void Result::setErrno(int32_t ec)
{
    errno_ = ec;
    msg_ = ErrorCodeMgr::GetInstance()->getMessage(ec, lang_);
}

void Result::setErrno(int32_t ec, const std::string& customMsg)
{
    errno_ = ec;
    msg_ = I18n::Translate(customMsg, lang_);
}

void Result::setErrnoFmt(int32_t ec, const char* fmt, ...)
{
    errno_ = ec;
    if (!fmt) {
        msg_.clear();
        return;
    }

    const std::string localized = I18n::Translate(fmt, lang_);
    va_list ap;
    va_start(ap, fmt);
    msg_ = chen::StringUtil::Formatv(localized.c_str(), ap);
    va_end(ap);
}

void Result::setDataJson(const std::string& jsonStr)
{
    Json::Value v;
    if (chen::JsonUtil::FromString(v, jsonStr)) {
        jsondata_ = v;
    }
}

void Result::setData(Json::Value& json_data)
{
    jsondata_ = json_data;
}

void Result::setLanguage(const std::string& lang)
{
    this->lang_ = lang;
}

std::string Result::toJsonString() const
{
    Json::Value v;
    v["errno"] = errno_;
    v["msg"] = msg_;
    v["used"] = ((chen::TimeUtil::GetCurrentUs() - used_) / 1000.0);
    if (!jsondata_.isNull()) {
        v["data"] = jsondata_;
    }
    return chen::JsonUtil::ToString(v);
}

BlogServlet::BlogServlet(const std::string& name)
    : chen::http::Servlet(name)
{
}

void BlogServlet::setRoute(const std::string& route)
{
    const metrics::RouteMetrics route_metrics = metrics::GetRouteMetrics(route);
    m_requests_metric = route_metrics.requests;
    m_errors_metric = route_metrics.errors;
    m_duration_metric = route_metrics.duration;
}

int32_t BlogServlet::handle(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, chen::http::HttpSession::ptr session)
{
    Result::ptr result = std::make_shared<Result>();
    uint64_t ts = chen::TimeUtil::GetCurrentUs();
    uint64_t used = 0;
    try {
        result->setLanguage(I18n::ResolveLanguage(request));
        // response->setHeader("Access-Control-Allow-Origin", "*");
        // response->setHeader("Access-Control-Allow-Credentials", "true");

        TRACE(logger) << "path=" << request->getPath() << " - method=" << chen::http::HttpMethodToString(request->getMethod());

        if (handlePre(request, response, session, result)) {
            handle(request, response, session, result);
        } else {
            response->setBody(result->toJsonString());
        }
        used = chen::TimeUtil::GetCurrentUs() - ts;
        handlePost(request, response, session, result);
        response->setHeader("used", std::to_string((used * 1.0 / 1000)) + "ms");
    } catch (const std::exception& e) {
        used = chen::TimeUtil::GetCurrentUs() - ts;
        ERROR(logger) << "servlet exception path=" << (request ? request->getPath() : "-") << " what=" << e.what();
        result->setErrno(errcode::INTERNAL_ERROR);
        if (response) {
            response->setBody(result->toJsonString());
        }
    } catch (...) {
        used = chen::TimeUtil::GetCurrentUs() - ts;
        ERROR(logger) << "servlet unknown exception path=" << (request ? request->getPath() : "-");
        result->setErrno(errcode::INTERNAL_ERROR);
        if (response) {
            response->setBody(result->toJsonString());
        }
    }
    const metrics::RouteMetrics route_metrics = {m_requests_metric, m_errors_metric, m_duration_metric};
    metrics::RecordRequest(route_metrics, used, result->getErrno());
    return 0;
}

/// 请求是否携带服务端会话 cookie（有才需要初始化/恢复 session）
static bool HasSessionCookie(chen::http::HttpRequest::ptr request)
{
    return !request->getCookie(CookieKey::SESSION_KEY).empty();
}

bool BlogServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (allowAutoLogin() && HasSessionCookie(request)) {
        initLogin(request, response);
    }
    return true;
}

bool BlogServlet::allowAutoLogin() const
{
    return true;
}

bool BlogServlet::handlePost(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    std::string val;
    if (!response->checkGetHeaderAs<std::string>("Content-Type", val, "application/json")) {
        response->setHeader("Content-Type", val);
    }
    INFO(logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::SESSION_KEY, "-") << "\t" << getUserId(request) << "\t"
                 << result->getErrno() << "\t" << result->getMessage() << "\t" << request->getPath() << "\t"
                 << (!request->getQuery().empty() ? request->getQuery() : "-");
    return true;
}

chen::http::SessionData::ptr BlogServlet::getSessionData(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response)
{
    std::string sid = request->getCookie(CookieKey::SESSION_KEY);
    if (!sid.empty()) {
        auto data = chen::http::SessionDataMgr::GetInstance()->get(sid);
        if (data) {
            int64_t uid = data->getData<int64_t>(SessionDataKey::USER_ID);
            if (!uid) {
                // 匿名会话不参与认证
                return data;
            }
            // 已登录会话必须以 SessionStore 为准，避免撤销后内存中仍残留
            SessionInfo info;
            if (SessionStore::Get(sid, info)) {
                return data;
            }
            chen::http::SessionDataMgr::GetInstance()->del(sid);
        }

        SessionInfo info;
        if (SessionStore::Get(sid, info)) {
            AuthService::MirrorSessionData(sid, info.uid);
            auto session_data = chen::http::SessionDataMgr::GetInstance()->get(sid);
            if (session_data) {
                return session_data;
            }
        }
    }

    // 没有就创建一个匿名会话
    chen::http::SessionData::ptr data(new chen::http::SessionData(true));
    data->setData(SessionDataKey::ANONYMOUS, 1);
    chen::http::SessionDataMgr::GetInstance()->add(data);
    AuthService::SetSessionCookie(response, data->getId(), 0);
    request->setCookie(CookieKey::SESSION_KEY, data->getId());
    return data;
}

bool BlogServlet::initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response)
{
    AuthContext ctx = AuthService::Resolve(request, response, true);
    return ctx.valid();
}

chen::IDB::ptr BlogServlet::getDB()
{
    return GetDB();
}

BlogLoginedServlet::BlogLoginedServlet(const std::string& name)
    : BlogServlet(name)
{
}

bool BlogLoginedServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!HasSessionCookie(request)) {
        result->setErrno(errcode::NOT_LOGIN);
        return false;
    }
    if (!initLogin(request, response)) {
        result->setErrno(errcode::NOT_LOGIN);
        return false;
    }
    return true;
}

BlogAdminServlet::BlogAdminServlet(const std::string& name)
    : BlogLoginedServlet(name)
{
}

bool BlogAdminServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!BlogLoginedServlet::handlePre(request, response, session, result)) {
        return false;
    }

    int64_t uid = getUserId(request);
    if (!uid) {
        result->setErrno(errcode::NOT_LOGIN);
        return false;
    }
    auto user = UserMgr::GetInstance()->get(uid);
    if (!user) {
        result->setErrno(errcode::USER_NOT_FOUND);
        return false;
    }
    if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        result->setErrno(errcode::ACCOUNT_INVALID);
        return false;
    }
    if (user->getRole() != UserManager::Role::ADMIN) {
        result->setErrno(errcode::ACCESS_DENIED);
        return false;
    }
    return true;
}

int64_t BlogServlet::getUserId(chen::http::HttpRequest::ptr request)
{
    AuthContext ctx = AuthService::Resolve(request, nullptr, false);
    return ctx.valid() ? ctx.uid : 0;
}

} // namespace blog
