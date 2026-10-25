#include "user_2fa_recovery_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"
#include "blog/data/user_recovery_code_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";

User2faRecoveryServlet::User2faRecoveryServlet()
    : BlogServlet("User2faRecoveryServlet")
{
}

int32_t User2faRecoveryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, tempToken, "temp_token");
        DEFINE_AND_CHECK_STRING(result, recoveryCode, "recovery_code");

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

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        // 查找匹配且未使用的恢复码
        std::string codeHash = TotpUtil::HashRecoveryCode(recoveryCode);
        auto qb = data::UserRecoveryCodeInfoDao::newQuery();
        qb->where("user_id", "=", uid);
        qb->where("code_hash", "=", codeHash);
        qb->where("used", "=", (int64_t)0);

        std::vector<data::UserRecoveryCodeInfo::ptr> codes;
        if (data::UserRecoveryCodeInfoDao::QueryByBuilder(codes, qb, db)) {
            result->setErrno(errcode::RECOVERY_CODE_INVALID);
            break;
        }

        if (codes.empty()) {
            result->setErrno(errcode::RECOVERY_CODE_INVALID);
            break;
        }

        // 标记恢复码为已使用
        auto matchedCode = codes[0];
        matchedCode->setUsed(1);
        data::UserRecoveryCodeInfoDao::Update(matchedCode, db);

        // 完成登录
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        INFO(logger) << "2fa recovery login user=" << info->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
