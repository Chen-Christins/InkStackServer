#include "platform/servlets/notify/BroadcastCloseServlet.hpp"

#include "platform/manager/NotificationManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

BroadcastCloseServlet::BroadcastCloseServlet()
    : BlogLoginedServlet("BroadcastCloseServlet")
{
}

int32_t BroadcastCloseServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        auto uinfo = UserMgr::GetInstance()->get(uid);
        if (!uinfo || uinfo->getRole() != UserManager::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        auto info = NotificationMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::NOTIFICATION_SEND_FAILED);
            break;
        }

        info->setIsBroadcast(0);
        info->setUpdateTime(time(0));

        auto db = GetDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }
        if (data::NotificationInfoDao::Update(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        NotificationMgr::GetInstance()->update(info);

        result->setErrno(errcode::SUCCESS);
        result->set("msg", "已关闭");
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
