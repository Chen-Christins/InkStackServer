#include "user_update_servlet.h"

#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include "../../auth/auth_service.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserUpdateServlet::UserUpdateServlet()
    : BlogServlet("UserUpdateServlet")
{
}

int32_t UserUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string name = request->getParam("name");
        std::string passwd = request->getParam("passwd");
        std::string bio = request->getParam("bio");
        std::string website = request->getParam("website");
        std::string github = request->getParam("github");
        std::string avatar = request->getParam("avatar");
        std::string old_passwd = request->getParam("old_passwd");

        if (name.empty() && passwd.empty() && bio.empty() && website.empty() && avatar.empty()) {
            result->setErrno(errcode::USER_NO_PARAM);
            break;
        }

        if (!passwd.empty() && passwd.length() < 6) {
            result->setErrno(errcode::USER_INVALID_PASSWORD);
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::UserInfo::ptr info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 修改密码时必须提供旧密码校验
        if (!passwd.empty()) {
            if (old_passwd.empty()) {
                result->setErrno(errcode::PARAM_MISSING, "old password required");
                break;
            }
            if (!VerifyPassword(old_passwd, info->getPasswd())) {
                result->setErrno(errcode::USER_OLD_PASSWORD_WRONG);
                break;
            }
        }

        if (!name.empty()) {
            info->setName(name);
        }
        if (!passwd.empty()) {
            std::string passwd_hash = HashPassword(passwd);
            if (passwd_hash.empty()) {
                result->setErrno(errcode::INTERNAL_ERROR);
                break;
            }
            info->setPasswd(passwd_hash);
        }
        if (!bio.empty()) {
            info->setBio(bio);
        }
        if (!website.empty()) {
            info->setWebsite(website);
        }
        if (!avatar.empty()) {
            info->setAvatar(avatar);
        }
        if (!github.empty()) {
            info->setGithub(github);
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
        result->setErrno(errcode::SUCCESS);

        if (!passwd.empty()) {
            // 密码变更后全端下线，要求重新登录
            AuthService::RevokeAllSessions(info->getId());
            AuthService::ClearSessionCookie(response);
            result->set("relogin", true);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
