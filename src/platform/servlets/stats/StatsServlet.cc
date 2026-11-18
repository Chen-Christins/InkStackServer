#include "platform/servlets/stats/StatsServlet.hpp"

#include "article/manager/ArticleManager.hpp"
#include "platform/manager/NotificationManager.hpp"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

StatsServlet::StatsServlet()
    : BlogServlet("StatsServlet")
{
}

int32_t StatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t online_users = NotificationMgr::GetInstance()->getOnlineCount();

        int64_t today_visits = ArticleMgr::GetInstance()->getTodayViews();
        int64_t total_visits = ArticleMgr::GetInstance()->getTotalViews();
        int64_t total_visitors = ArticleMgr::GetInstance()->getTotalVisitors();

        result->set("online_users", online_users);
        result->set("today_visits", today_visits);
        result->set("total_visits", total_visits);
        result->set("total_visitors", total_visitors);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
