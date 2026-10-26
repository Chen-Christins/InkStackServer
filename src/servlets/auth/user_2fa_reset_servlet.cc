#include "user_2fa_reset_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"
#include "blog/data/user_recovery_code_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

User2faResetServlet::User2faResetServlet()
    : BlogLoginedServlet("User2faResetServlet")
{
}

int32_t User2faResetServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, recoveryCode, "recovery_code");

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

        if (!info->getTotpEnabled()) {
            result->setErrno(errcode::TOTP_NOT_ENABLED);
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

        // 禁用2FA
        info->setTotpEnabled(0);
        info->setTotpSecret("");
        info->setUpdateTime(time(0));

        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::TOTP_ENABLE_FAILED, "reset 2fa failed");
            ERROR(logger) << "reset 2fa: Update failed"
                          << " errstr=" << db->getErrStr() << " errno=" << db->getErrno();
            break;
        }

        // 更新缓存
        UserMgr::GetInstance()->update(info);

        // 生成新密钥供重新绑定
        std::string newSecret = TotpUtil::GenerateSecret();
        std::string otpUri = TotpUtil::GetOtpUri(newSecret, info->getAccount());

        // 将新密钥临时存入session
        auto sdata = getSessionData(request, response);
        sdata->setData("totp_pending_secret", newSecret);

        result->set("secret", newSecret);
        result->set("otp_uri", otpUri);
        INFO(logger) << "user=" << uid << " reset 2fa via recovery code";
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
