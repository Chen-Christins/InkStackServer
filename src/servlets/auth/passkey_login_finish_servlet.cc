#include "passkey_login_finish_servlet.h"

#include <chen/util/json_util.h>
#include <chen/util/time_util.h>

#include "../../auth/auth_service.h"
#include "../../auth/webauthn/webauthn.h"
#include "../../manager/passkey_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

PasskeyLoginFinishServlet::PasskeyLoginFinishServlet()
    : BlogServlet("PasskeyLoginFinishServlet")
{
}

int32_t PasskeyLoginFinishServlet::handle(
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

        Json::Value credential;
        if (!chen::JsonUtil::FromString(credential, request->getBody()) || !credential.isObject()) {
            result->setErrno(errcode::PARAM_INVALID);
            break;
        }

        std::string credential_id;
        if (!webauthn::WebAuthnService::ExtractCredentialId(credential, credential_id)) {
            result->setErrno(errcode::WEBAUTHN_INVALID_CREDENTIAL);
            break;
        }

        auto passkey = PasskeyMgr::GetInstance()->getByCredentialId(credential_id);
        if (!passkey) {
            result->setErrno(errcode::PASSKEY_NOT_FOUND);
            break;
        }

        auto user = UserMgr::GetInstance()->get(passkey->getUserId());
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        webauthn::AuthenticationResult auth_result;
        int32_t ret = webauthn::WebAuthnService::VerifyAuthentication(
            passkey->getUserId(), passkey->getPublicKey(), passkey->getSignCount(), credential, auth_result);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        if (auth_result.credential_id != passkey->getCredentialId()) {
            result->setErrno(errcode::WEBAUTHN_INVALID_CREDENTIAL);
            break;
        }

        passkey->setSignCount(auth_result.sign_count);
        passkey->setLastUsedAt(chen::TimeUtil::GetCurrentSec());
        passkey->setBackupState(auth_result.backup_state);
        if (PasskeyMgr::GetInstance()->update(passkey) != 0) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        // Passkey 本身已具备持有因子 + 本地用户验证，登录时不再叠加 TOTP。
        IssueOptions options;
        ret = AuthService::IssueSession(request, response, session, user, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->set("id", user->getId());
        result->set("account", user->getAccount());
        result->set("name", user->getName());
        result->set("avatar", user->getAvatar());
        result->set("role", user->getRole());
        result->set("user_verified", auth_result.user_verified);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
