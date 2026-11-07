/**
 * @file struct.h
 * @brief 博客使用到的数据结构
 * @author Christins
 * @date 2025-05-11
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>
#include <string>

#include <chen/db/db.h>
#include <chen/http/servlet.h>
#include <chen/http/session_data.h>
#include <json/json.h>

#include "error_codes.h" // IWYU pragma: keep

namespace blog {

/// 获取客户端真实 IP（优先 X-Real-IP 头，否则取会话远端地址）
std::string GetRemoteIP(chen::http::HttpRequest::ptr request, chen::http::HttpSession::ptr session);

struct Result {
    typedef std::shared_ptr<Result> ptr;

    Result(int32_t ec = 0, const std::string& msg = "ok");

    template <class T>
    void set(const std::string& key, const T& v)
    {
        jsondata_[key] = v;
    }

    template <class T>
    void append(const std::string& key, const T& v)
    {
        jsondata_[key].append(v);
    }

    void set(const std::string& key, const char* v) { jsondata_[key] = v; }
    void set(const std::string& key, const std::string& v) { jsondata_[key] = v; }

    void setErrno(int32_t ec);

    void setErrno(int32_t ec, const std::string& customMsg);
    int32_t getErrno() const { return errno_; }

    const std::string& getMessage() const { return msg_; }

    void setErrnoFmt(int32_t ec, const char* fmt, ...);

    void setDataJson(const std::string& jsonStr);

    void setData(Json::Value& json_data);

    void setLanguage(const std::string& lang);

    std::string toJsonString() const;

private:
    int32_t errno_;
    int64_t used_;
    std::string msg_;
    std::string lang_;
    Json::Value jsondata_;
};

struct CookieKey {
    static const std::string SESSION_KEY;
};

struct SessionDataKey {
    static const std::string USER_ID;
    static const std::string ANONYMOUS;
};

class BlogServlet : public chen::http::Servlet {
public:
    BlogServlet(const std::string& name);

    int32_t handle(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, chen::http::HttpSession::ptr session) override;

    int64_t getUserId(chen::http::HttpRequest::ptr request);

protected:
    virtual bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    virtual int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) = 0;

    virtual bool handlePost(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

protected:
    chen::http::SessionData::ptr getSessionData(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response);

    bool initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response);

    /**
     * @brief 是否允许公开接口自动解析登录态（登录接口可覆盖为 false）
     */
    virtual bool allowAutoLogin() const;

protected:
    chen::IDB::ptr getDB();
};

class BlogLoginedServlet : public BlogServlet {
public:
    BlogLoginedServlet(const std::string& name);

protected:
    bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

/**
 * @brief 管理员专用 Servlet
 * @note 在登录校验基础上要求当前账号状态正常且角色为管理员
 */
class BlogAdminServlet : public BlogLoginedServlet {
public:
    BlogAdminServlet(const std::string& name);

protected:
    bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace blog
