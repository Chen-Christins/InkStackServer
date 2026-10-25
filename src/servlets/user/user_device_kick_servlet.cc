#include "user_device_kick_servlet.h"

#include "../../auth/session_store.h"
#include "../../manager/user_login_device_manager.h"
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

        // 新会话体系：device_id 实际是 SessionStore::HashId(SESSIONID)
        if (SessionStore::DestroyForUser(uid, device_id)) {
            result->setErrno(errcode::SUCCESS);
            break;
        }

        // 迁移期兼容旧设备 token
        auto device = UserLoginDeviceMgr::GetInstance()->getActiveByDevice(device_id);
        if (!device) {
            result->setErrno(errcode::DEVICE_NOT_FOUND);
            break;
        }
        if (device->getUserId() != uid) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        if (UserLoginDeviceMgr::GetInstance()->kick(device)) {
            result->setErrno(errcode::DEVICE_KICK_FAILED);
            break;
        }
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
