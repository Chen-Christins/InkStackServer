#include "user_device_kick_servlet.h"

#include "../../auth/session_store.h"
#include "../../util.h"

namespace blog {
namespace servlet {

UserDeviceKickServlet::UserDeviceKickServlet()
    : BlogLoginedServlet("UserDeviceKickServlet")
{
}

int32_t UserDeviceKickServlet::handle(
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

        DEFINE_AND_CHECK_STRING(result, device_id, "device_id");

        // device_id 是 SessionStore::HashId(SESSIONID)
        if (!SessionStore::DestroyForUser(uid, device_id)) {
            result->setErrno(errcode::DEVICE_NOT_FOUND);
            break;
        }
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
