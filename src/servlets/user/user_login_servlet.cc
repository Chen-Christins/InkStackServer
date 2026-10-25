#include "user_login_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";
/// 2FA 临时凭证 TTL（秒）
static const int k2faPendingTTL = 300;

UserLoginServlet::UserLoginServlet()
    : BlogServlet("UserLoginServlet")
{
}

int32_t UserLoginServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, auth_id, "auth_id");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");

        AuthContext ctx = AuthService::Resolve(request, response, false);
        if (ctx.valid()) {
            result->setErrno(errcode::USER_ALREADY_LOGIN);
            break;
        }

        data::UserInfo::ptr info;
        if (IsEmail(auth_id)) {
            info = UserMgr::GetInstance()->getByEmail(auth_id);
        } else if (IsValidAccount(auth_id)) {
            info = UserMgr::GetInstance()->getByAccount(auth_id);
        } else {
            result->setErrno(errcode::USER_INVALID_ACCOUNT);
            break;
        }

        if (!info) {
            result->setErrno(errcode::AUTH_CODE_INVALID);
            break;
        }
        if (!VerifyPassword(passwd, info->getPasswd())) {
            result->setErrno(errcode::USER_PASSWORD_WRONG);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 历史 MD5 哈希在登录成功后透明升级为 bcrypt；失败不影响本次登录，下次登录重试
        if (IsLegacyPasswordHash(info->getPasswd())) {
            std::string passwd_hash = HashPassword(passwd);
            auto upgrade_db = getDB();
            if (!passwd_hash.empty() && upgrade_db) {
                std::string old_hash = info->getPasswd();
                info->setPasswd(passwd_hash);
                if (data::UserInfoDao::Update(info, upgrade_db) == 0) {
                    UserMgr::GetInstance()->update(info);
                } else {
                    info->setPasswd(old_hash);
                    ERROR(logger) << "upgrade password hash failed user=" << info->getId();
                }
            }
        }

        // 2FA: 密码验证通过后，如果启用了TOTP，返回临时凭证等待第二步验证
        if (info->getTotpEnabled()) {
            std::string tempToken = UserManager::generateToken();
            std::string redisKey = std::string(k2faPendingPrefix) + tempToken;
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), k2faPendingTTL, (long long)info->getId());
            result->set("require_2fa", true);
            result->set("temp_token", tempToken);
            INFO(logger) << "login user=" << info->getId() << " require 2fa verification";
            break;
        }

        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        INFO(logger) << "login user=" << info->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
