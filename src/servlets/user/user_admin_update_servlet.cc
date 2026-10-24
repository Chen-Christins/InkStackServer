#include "user_admin_update_servlet.h"

#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminUpdateServlet::UserAdminUpdateServlet()
    : BlogAdminServlet("UserAdminUpdateServlet")
{
}

int32_t UserAdminUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        auto info = UserMgr::GetInstance()->get(user_id);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        std::string account = request->getParam("account");
        std::string email = request->getParam("email");
        std::string passwd = request->getParam("passwd");
        int32_t role = 0;
        int32_t state = 0;
        bool has_role = request->checkGetParamAs("role", role);
        bool has_state = request->checkGetParamAs("state", state);

        if (account.empty() && email.empty() && passwd.empty() && !has_role && !has_state) {
            result->setErrno(errcode::USER_NO_PARAM);
            break;
        }

        if (!account.empty()) {
            if (!IsValidAccount(account)) {
                result->setErrno(errcode::USER_INVALID_ACCOUNT);
                break;
            }
            auto exist = UserMgr::GetInstance()->getByAccount(account);
            if (exist && exist->getId() != user_id) {
                result->setErrno(errcode::USER_ACCOUNT_EXISTS);
                break;
            }
            info->setAccount(account);
        }
        if (!email.empty()) {
            if (!IsEmail(email)) {
                result->setErrno(errcode::USER_INVALID_EMAIL);
                break;
            }
            auto exist = UserMgr::GetInstance()->getByEmail(email);
            if (exist && exist->getId() != user_id) {
                result->setErrno(errcode::USER_EMAIL_EXISTS);
                break;
            }
            info->setEmail(email);
        }
        if (!passwd.empty()) {
            if (passwd.length() < 6) {
                result->setErrno(errcode::USER_INVALID_PASSWORD);
                break;
            }
            std::string passwd_hash = HashPassword(passwd);
            if (passwd_hash.empty()) {
                result->setErrno(errcode::INTERNAL_ERROR);
                break;
            }
            info->setPasswd(passwd_hash);
        }
        if (has_role) {
            if (role < 1 || role > 4) {
                result->setErrno(errcode::PARAM_INVALID, "invalid role");
                break;
            }
            info->setRole(role);
        }
        if (has_state) {
            if (state < 1 || state > 2) {
                result->setErrno(errcode::PARAM_INVALID, "invalid state");
                break;
            }
            info->setState(state);
        }

        info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "update user failed");
            ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        UserMgr::GetInstance()->update(info);

        // 改密或禁用账号后，立即撤销该用户全部会话
        if (!passwd.empty() || (has_state && state != UserManager::Status::ACTIVE)) {
            AuthService::RevokeAllSessions(user_id);
        }

        result->set("id", info->getId());
        result->set("account", info->getAccount());
        result->set("name", info->getName());
        result->set("email", info->getEmail());
        result->set("role", info->getRole());
        result->set("state", info->getState());
        result->set("update_time", info->getUpdateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
