#include "passkey_register_begin_servlet.h"

#include "../../auth/webauthn/webauthn.h"
#include "../../manager/passkey_manager.h"
#include "../../manager/user_manager.h"
#include "../../util/ua_parser.h"

namespace blog {
namespace servlet {

PasskeyRegisterBeginServlet::PasskeyRegisterBeginServlet()
    : BlogLoginedServlet("PasskeyRegisterBeginServlet")
{
}

int32_t PasskeyRegisterBeginServlet::handle(
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

        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        std::string passkey_name = request->getParam("name");
        if (passkey_name.empty()) {
            passkey_name = ParseDeviceName(request->getHeader("User-Agent"));
        }
        if (passkey_name.empty()) {
            passkey_name = "Passkey";
        }
        if (passkey_name.size() > 64) {
            result->setErrno(errcode::PARAM_INVALID, "passkey name too long");
            break;
        }

        std::vector<data::UserPasskeyInfo::ptr> passkeys;
        if (PasskeyMgr::GetInstance()->listByUser(passkeys, uid) < 0) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        std::vector<std::string> exclude_ids;
        exclude_ids.reserve(passkeys.size());
        for (const auto& passkey : passkeys) {
            if (passkey) {
                exclude_ids.push_back(passkey->getCredentialId());
            }
        }

        Json::Value options;
        int32_t ret =
            webauthn::WebAuthnService::CreateRegistrationOptions(uid, user->getAccount(), user->getName(), passkey_name, exclude_ids, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->jsondata = options;
        result->set("name", passkey_name);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
