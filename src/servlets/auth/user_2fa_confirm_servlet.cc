#include "user_2fa_confirm_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"
#include "blog/data/user_recovery_code_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

User2faConfirmServlet::User2faConfirmServlet()
    : BlogLoginedServlet("User2faConfirmServlet")
{
}

int32_t User2faConfirmServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, code, "code");

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

        // 从session获取待确认的密钥
        auto sdata = getSessionData(request, response);
        std::string secret = sdata->getData<std::string>("totp_pending_secret");
        if (secret.empty()) {
            result->setErrno(errcode::TOTP_ENABLE_FAILED, "please call /api/v1/user/2fa/enable first");
            break;
        }

        // 验证TOTP码
        if (!TotpUtil::VerifyCode(secret, code, 1)) {
            result->setErrno(errcode::TOTP_INVALID_CODE);
            break;
        }

        // 验证通过，写入DB
        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        info->setTotpSecret(secret);
        info->setTotpEnabled(1);
        info->setUpdateTime(time(0));

        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::TOTP_ENABLE_FAILED);
            ERROR(logger) << "confirm 2fa: Update failed"
                          << " errstr=" << db->getErrStr() << " errno=" << db->getErrno();
            break;
        }
        UserMgr::GetInstance()->update(info);

        // 清除session中的待确认密钥
        sdata->setData("totp_pending_secret", std::string(""));

        // 生成恢复码
        std::vector<std::string> hashedCodes;
        std::vector<std::string> recoveryCodes = TotpUtil::GenerateRecoveryCodes(10, hashedCodes);

        // 将恢复码哈希存入DB
        auto trans = db->openTransaction();
        for (auto& hash : hashedCodes) {
            auto rc = std::make_shared<data::UserRecoveryCodeInfo>();
            rc->setUserId(uid);
            rc->setCodeHash(hash);
            rc->setUsed(0);
            if (data::UserRecoveryCodeInfoDao::Insert(rc, db)) {
                ERROR(logger) << "confirm 2fa: insert recovery code failed"
                              << " errstr=" << db->getErrStr();
            }
        }
        if (!trans->commit()) {
            ERROR(logger) << "confirm 2fa: commit recovery codes failed";
        }

        // 更新缓存
        UserMgr::GetInstance()->update(info);

        // 返回恢复码（仅此一次展示）
        Json::Value codesArr;
        for (std::string& c : recoveryCodes) {
            codesArr.append(c);
        }
        result->set("recovery_codes", codesArr);
        INFO(logger) << "user=" << uid << " enabled 2fa";
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
