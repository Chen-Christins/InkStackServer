#include "user_2fa_disable_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"
#include "blog/data/user_recovery_code_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

User2faDisableServlet::User2faDisableServlet()
    : BlogLoginedServlet("User2faDisableServlet")
{
}

int32_t User2faDisableServlet::handle(
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

        if (!info->getTotpEnabled()) {
            result->setErrno(errcode::TOTP_NOT_ENABLED);
            break;
        }

        // 验证TOTP码（必须验证通过才能禁用）
        if (!TotpUtil::VerifyCode(info->getTotpSecret(), code, 1)) {
            result->setErrno(errcode::TOTP_INVALID_CODE);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        info->setTotpEnabled(0);
        info->setTotpSecret("");
        info->setUpdateTime(time(0));

        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::TOTP_ENABLE_FAILED, "disable 2fa failed");
            ERROR(logger) << "disable 2fa: Update failed"
                          << " errstr=" << db->getErrStr() << " errno=" << db->getErrno();
            break;
        }
        UserMgr::GetInstance()->update(info);

        // 删除该用户所有恢复码
        auto qb = data::UserRecoveryCodeInfoDao::newQuery();
        qb->where("user_id", "=", uid);
        std::vector<data::UserRecoveryCodeInfo::ptr> codes;
        data::UserRecoveryCodeInfoDao::QueryByBuilder(codes, qb, db);
        for (auto& rc : codes) {
            data::UserRecoveryCodeInfoDao::DeleteById(rc->getId(), db);
        }

        // 更新缓存
        UserMgr::GetInstance()->update(info);

        INFO(logger) << "user=" << uid << " disabled 2fa";
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
