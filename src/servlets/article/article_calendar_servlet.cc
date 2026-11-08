/**
 * @file article_calendar_servlet.cc
 * @brief 首页日历接口实现
 * @author Christins
 * @date 2026-06-14
 * @copyright Apache 2.0
 */
#include "article_calendar_servlet.h"

#include <chen/log/log.h>

#include "../../manager/article_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleCalendarServlet::ArticleCalendarServlet()
    : BlogLoginedServlet("ArticleCalendarServlet")
{
}

int32_t ArticleCalendarServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t user_id = getUserId(request);
        int64_t year = request->getParamAs<int64_t>("year");
        int64_t month = request->getParamAs<int64_t>("month");

        if (year <= 0 || month < 1 || month > 12) {
            result->setErrno(errcode::PARAM_INVALID, "invalid year or month");
            break;
        }

        std::vector<int32_t> days;
        ArticleMgr::GetInstance()->getCalendarDays(user_id, static_cast<int32_t>(year), static_cast<int32_t>(month), days);

        result->set("year", year);
        result->set("month", month);
        Json::Value daysArray(Json::arrayValue);
        for (auto d : days) {
            daysArray.append(d);
        }
        result->set("articleDays", daysArray);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
