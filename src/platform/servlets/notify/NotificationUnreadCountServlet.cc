#include "platform/servlets/notify/NotificationUnreadCountServlet.hpp"

#include "platform/manager/NotificationManager.hpp"

namespace blog {
namespace servlet {

NotificationUnreadCountServlet::NotificationUnreadCountServlet()
    : BlogLoginedServlet("NotificationUnreadCountServlet")
{
}

int32_t NotificationUnreadCountServlet::handle(
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

        int64_t count = NotificationMgr::GetInstance()->unreadCount(uid);

        result->setErrno(errcode::SUCCESS);
        result->set("count", count);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
} // namespace servlet
} // namespace blog
