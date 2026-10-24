#include "user_refresh_token_servlet.h"

#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include "../../manager/system_settings_manager.h"
#include "../../manager/user_login_device_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserRefreshTokenServlet::UserRefreshTokenServlet()
    : BlogServlet("UserRefreshTokenServlet")
{
}

int32_t UserRefreshTokenServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string uid_str = request->getParam("uid");
        std::string token = request->getParam("token");

        int64_t uid = 0;
        if (!uid_str.empty()) {
            uid = DecryptUserId(uid_str);
        }
        if (!uid) {
            uid = getUserId(request);
        }
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::UserInfo::ptr info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (token.empty()) {
            token = request->getCookie(CookieKey::TOKEN);
        }
        if (token.empty()) {
            // 新会话体系下没有 token 需要刷新；会话有效直接返回成功
            int64_t session_uid = getUserId(request);
            if (session_uid) {
                result->set("uid", EncryptUserId(session_uid));
                result->set("session", true);
                result->setErrno(errcode::SUCCESS);
                break;
            }
            result->setErrno(errcode::PARAM_MISSING, "token is required");
            break;
        }

        // 校验 token：以设备表 token 为准
        if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, token, chen::TimeUtil::GetCurrentSec())) {
            result->setErrno(errcode::NOT_LOGIN, "token mismatch, please re-login");
            break;
        }

        int64_t now = chen::TimeUtil::GetCurrentSec();
        int32_t session_timeout = 24;
        auto sys_settings = SystemSettingsMgr::GetInstance()->get();
        if (sys_settings && sys_settings->getSessionTimeout() > 0) {
            session_timeout = sys_settings->getSessionTimeout();
        }
        int64_t token_time = now + 3600 * session_timeout;
        std::string new_token = UserManager::generateToken();

        if (UserLoginDeviceMgr::GetInstance()->updateToken(token, new_token, token_time)) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        response->setCookie(CookieKey::USER_ID, EncryptUserId(info->getId()), token_time, "/");
        response->setCookie(CookieKey::TOKEN, new_token, token_time, "/");
        response->setCookie(CookieKey::TOKEN_TIME, std::to_string(token_time), token_time, "/");

        result->set("uid", EncryptUserId(info->getId()));
        result->set("token", new_token);
        result->set("token_time", token_time);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
