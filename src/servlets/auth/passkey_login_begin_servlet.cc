#include "passkey_login_begin_servlet.h"

#include <vector>

#include "../../auth/auth_service.h"
#include "../../auth/webauthn/webauthn.h"
#include "../../manager/passkey_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

PasskeyLoginBeginServlet::PasskeyLoginBeginServlet()
    : BlogServlet("PasskeyLoginBeginServlet")
{
}

int32_t PasskeyLoginBeginServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        AuthContext ctx = AuthService::Resolve(request, response, false);
        if (ctx.valid()) {
            result->setErrno(errcode::USER_ALREADY_LOGIN);
            break;
        }

        int64_t uid = 0;
        std::vector<std::string> credential_ids;
        std::string auth_id = request->getParam("auth_id");
        if (!auth_id.empty()) {
            data::UserInfo::ptr user;
            if (IsEmail(auth_id)) {
                user = UserMgr::GetInstance()->getByEmail(auth_id);
            } else if (IsValidAccount(auth_id)) {
                user = UserMgr::GetInstance()->getByAccount(auth_id);
            } else {
                result->setErrno(errcode::USER_INVALID_ACCOUNT);
                break;
            }

            if (!user) {
                result->setErrno(errcode::USER_NOT_FOUND);
                break;
            }
            if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
                result->setErrno(errcode::ACCOUNT_INVALID);
                break;
            }

            std::vector<data::UserPasskeyInfo::ptr> passkeys;
            if (PasskeyMgr::GetInstance()->listByUser(passkeys, user->getId()) < 0) {
                result->setErrno(errcode::DB_OPERATION_FAILED);
                break;
            }
            if (passkeys.empty()) {
                result->setErrno(errcode::PASSKEY_NOT_FOUND);
                break;
            }

            uid = user->getId();
            for (const auto& passkey : passkeys) {
                if (passkey && !passkey->getCredentialId().empty()) {
                    credential_ids.push_back(passkey->getCredentialId());
                }
            }
            if (credential_ids.empty()) {
                result->setErrno(errcode::PASSKEY_NOT_FOUND);
                break;
            }
        }

        Json::Value options;
        int32_t ret = webauthn::WebAuthnService::CreateAuthenticationOptions(uid, credential_ids, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->jsondata = options;
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
