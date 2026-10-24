#include "passkey_register_finish_servlet.h"

#include <chen/util/json_util.h>
#include <chen/util/time_util.h>

#include "../../auth/webauthn/webauthn.h"
#include "../../manager/passkey_manager.h"

namespace blog {
namespace servlet {

PasskeyRegisterFinishServlet::PasskeyRegisterFinishServlet()
    : BlogLoginedServlet("PasskeyRegisterFinishServlet")
{
}

int32_t PasskeyRegisterFinishServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        Json::Value credential;
        if (!chen::JsonUtil::FromString(credential, request->getBody()) || !credential.isObject()) {
            result->setErrno(errcode::PARAM_INVALID);
            break;
        }

        webauthn::RegistrationResult registration;
        std::string passkey_name;
        int32_t ret = webauthn::WebAuthnService::VerifyRegistration(uid, credential, registration, passkey_name);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        std::string body_name;
        if (credential.isMember("name") && credential["name"].isString()) {
            body_name = credential["name"].asString();
        }
        if (!body_name.empty()) {
            passkey_name = body_name;
        }
        if (passkey_name.empty()) {
            passkey_name = "Passkey";
        }
        if (passkey_name.size() > 64) {
            result->setErrno(errcode::PARAM_INVALID, "passkey name too long");
            break;
        }

        if (PasskeyMgr::GetInstance()->getByCredentialId(registration.credential_id)) {
            result->setErrno(errcode::PASSKEY_ALREADY_EXISTS);
            break;
        }

        auto info = std::make_shared<data::UserPasskeyInfo>();
        info->setUserId(uid);
        info->setCredentialId(registration.credential_id);
        info->setPublicKey(registration.public_key);
        info->setAlg(registration.alg);
        info->setSignCount(registration.sign_count);
        info->setTransports(registration.transports);
        info->setAaguid(registration.aaguid);
        info->setName(passkey_name);
        info->setBackupEligible(registration.backup_eligible);
        info->setBackupState(registration.backup_state);
        info->setIsDeleted(0);
        info->setCreatedAt(chen::TimeUtil::GetCurrentSec());
        info->setLastUsedAt(chen::TimeUtil::GetCurrentSec());

        if (PasskeyMgr::GetInstance()->add(info) != 0) {
            result->setErrno(errcode::PASSKEY_REGISTER_FAILED);
            break;
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("credential_id", info->getCredentialId());
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
