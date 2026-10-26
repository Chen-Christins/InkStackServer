#include "user_reset_passwd_servlet.h"

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

UserResetPasswdServlet::UserResetPasswdServlet()
    : BlogServlet("UserResetPasswdServlet")
{
}

int32_t UserResetPasswdServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_STRING(result, auth_code, "auth_code");

        if (passwd.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "password required");
            break;
        }

        if (!IsEmail(email)) {
            result->setErrno(errcode::USER_INVALID_EMAIL);
            break;
        }
        if (!blog::UserMgr::GetInstance()->getByEmail(email)) {
            result->setErrno(errcode::USER_EMAIL_NOT_REGISTER);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        if (!verificationEmailCode(email, auth_code)) {
            result->setErrno(errcode::AUTH_CODE_INVALID);
            break;
        }

        std::string passwd_hash = HashPassword(passwd);
        if (passwd_hash.empty()) {
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        chen::ITransaction::ptr trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }
        data::UserInfo::ptr info = UserMgr::GetInstance()->getByEmail(email);
        if (!info) {
            result->setErrno(errcode::USER_EMAIL_NOT_REGISTER);
            break;
        }
        info->setPasswd(passwd_hash);

        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->update(info);
        AuthService::RevokeAllSessions(info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool UserResetPasswdServlet::verificationEmailCode(const std::string& email, const std::string& code)
{
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "GET email:verify:3:%s", email.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING) {
        return false;
    }
    if (code != rpy->str) {
        return false;
    }
    // 一次性使用，校验通过后删除
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "DEL email:verify:3:%s", email.c_str());
    return true;
}

} // namespace servlet
} // namespace blog
