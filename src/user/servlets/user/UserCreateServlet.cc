#include "user/servlets/user/UserCreateServlet.hpp"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "common/Util.hpp"
#include "user/client/PlatformClient.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

UserCreateServlet::UserCreateServlet()
    : BlogServlet("UserCreateServlet")
{
}

int32_t UserCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, account, "account");
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_STRING(result, auth_code, "auth_code");

        if (account.empty() || passwd.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "account and password required");
            break;
        }

        if (blog::UserMgr::GetInstance()->getByAccount(account)) {
            result->setErrno(errcode::USER_ACCOUNT_EXISTS);
            break;
        }
        if (blog::UserMgr::GetInstance()->getByEmail(email)) {
            result->setErrno(errcode::USER_EMAIL_EXISTS);
            break;
        }
        if (!IsEmail(email)) {
            result->setErrno(errcode::USER_INVALID_EMAIL);
            break;
        }
        if (!IsValidAccount(account)) {
            result->setErrno(errcode::USER_INVALID_ACCOUNT);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        // 根据系统设置决定是否需要校验邮箱验证码；设置不可用时按需要校验处理（安全优先）
        tagSystemSettings sys_settings;
        const bool has_settings = client::PlatformClientMgr::GetInstance()->tryGetSettings(sys_settings);
        const bool require_verification = has_settings ? sys_settings.RequireEmailVerification != 0 : true;
        if (require_verification) {
            if (!verificationEmailCode(email, auth_code)) {
                result->setErrno(errcode::AUTH_CODE_INVALID);
                break;
            }
        }

        std::string passwd_hash = HashPassword(passwd);
        if (passwd_hash.empty()) {
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        // 开启事务
        chen::ITransaction::ptr trans = db->openTransaction();
        data::UserInfo::ptr info(new data::UserInfo);
        info->setAccount(account);
        info->setEmail(email);
        info->setPasswd(passwd_hash);
        info->setState(UserManager::Status::ACTIVE);
        info->setName(account);

        if (data::UserInfoDao::Insert(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->add(info);
        INFO(logger) << info->toJsonString();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool UserCreateServlet::verificationEmailCode(const std::string& email, const std::string& code)
{
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "GET email:verify:1:%s", email.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING) {
        return false;
    }
    if (code != rpy->str) {
        return false;
    }
    // 一次性使用，校验通过后删除
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "DEL email:verify:1:%s", email.c_str());
    return true;
}

} // namespace servlet
} // namespace blog
