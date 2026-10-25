#include "passkey_list_servlet.h"

#include <vector>

#include "../../manager/passkey_manager.h"

namespace blog {
namespace servlet {

PasskeyListServlet::PasskeyListServlet()
    : BlogLoginedServlet("PasskeyListServlet")
{
}

int32_t PasskeyListServlet::handle(
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

        std::vector<data::UserPasskeyInfo::ptr> passkeys;
        if (PasskeyMgr::GetInstance()->listByUser(passkeys, uid) < 0) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(errcode::SUCCESS);
        result->set("total", passkeys.size());
        auto& list = result->jsondata["list"];
        for (const auto& passkey : passkeys) {
            if (!passkey) {
                continue;
            }
            Json::Value item;
            item["id"] = passkey->getId();
            item["name"] = passkey->getName();
            item["alg"] = passkey->getAlg();
            item["aaguid"] = passkey->getAaguid();
            item["transports"] = passkey->getTransports();
            item["backup_eligible"] = passkey->getBackupEligible();
            item["backup_state"] = passkey->getBackupState();
            item["created_at"] = passkey->getCreatedAt();
            item["last_used_at"] = passkey->getLastUsedAt();
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
