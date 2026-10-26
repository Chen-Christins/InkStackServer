#include "user_2fa_verify_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";

User2faVerifyServlet::User2faVerifyServlet()
    : BlogServlet("User2faVerifyServlet")
{
}

int32_t User2faVerifyServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, tempToken, "temp_token");
        DEFINE_AND_CHECK_STRING(result, code, "code");

        // 从Redis获取临时凭证对应的用户ID
        std::string key = std::string(k2faPendingPrefix) + tempToken;
        auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", key.c_str());
        int64_t uid = 0;
        if (rpy && rpy->type == REDIS_REPLY_STRING && rpy->str) {
            try {
                uid = chen::TypeUtil::Atoi(std::string(rpy->str, rpy->len));
            } catch (...) {
                uid = 0;
            }
        }
        if (!uid) {
            result->setErrno(errcode::TEMP_TOKEN_INVALID);
            break;
        }

        // 删除临时凭证（一次性）
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.c_str());

        auto info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 验证TOTP码
        if (!TotpUtil::VerifyCode(info->getTotpSecret(), code, 1)) {
            result->setErrno(errcode::TOTP_INVALID_CODE);
            break;
        }

        // TOTP验证通过，签发会话
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        INFO(logger) << "2fa login user=" << info->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
