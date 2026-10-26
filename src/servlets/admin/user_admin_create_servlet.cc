#include "user_admin_create_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminCreateServlet::UserAdminCreateServlet()
    : BlogAdminServlet("UserAdminCreateServlet")
{
}

int32_t UserAdminCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, account, "account");
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_TYPE(result, int32_t, role, "role");
        DEFINE_AND_CHECK_TYPE(result, int32_t, state, "state");

        if (account.empty() || passwd.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "account and password required");
            break;
        }
        if (passwd.length() < 6) {
            result->setErrno(errcode::USER_INVALID_PASSWORD);
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
        if (role < 1 || role > 4) {
            result->setErrno(errcode::PARAM_INVALID, "invalid role");
            break;
        }
        if (state < 1 || state > 2) {
            result->setErrno(errcode::PARAM_INVALID, "invalid state");
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }
        std::string passwd_hash = HashPassword(passwd);
        if (passwd_hash.empty()) {
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        chen::ITransaction::ptr trans = db->openTransaction();
        data::UserInfo::ptr info(new data::UserInfo);
        info->setAccount(account);
        info->setEmail(email);
        info->setPasswd(passwd_hash);
        info->setRole(role);
        info->setState(state);
        info->setName(account);

        if (data::UserInfoDao::Insert(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->add(info);

        result->set("id", info->getId());
        result->set("account", info->getAccount());
        result->set("name", info->getName());
        result->set("email", info->getEmail());
        result->set("role", info->getRole());
        result->set("state", info->getState());
        result->set("create_time", info->getCreateTime());
        INFO(logger) << info->toJsonString();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
