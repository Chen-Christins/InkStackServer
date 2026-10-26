#include "user_2fa_enable_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

User2faEnableServlet::User2faEnableServlet()
    : BlogLoginedServlet("User2faEnableServlet")
{
}

int32_t User2faEnableServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (info->getTotpEnabled()) {
            result->setErrno(errcode::TOTP_ALREADY_ENABLED);
            break;
        }

        // 生成新密钥（不立即写入DB，等用户confirm后才正式启用）
        std::string secret = TotpUtil::GenerateSecret();
        std::string otpUri = TotpUtil::GetOtpUri(secret, info->getAccount());

        // 将密钥临时存入session，等待confirm步骤确认
        auto sdata = getSessionData(request, response);
        sdata->setData("totp_pending_secret", secret);

        result->set("secret", secret);
        result->set("otp_uri", otpUri);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
