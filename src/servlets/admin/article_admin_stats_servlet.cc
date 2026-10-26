#include "article_admin_stats_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

ArticleAdminStatsServlet::ArticleAdminStatsServlet()
    : BlogLoginedServlet("ArticleAdminStatsServlet")
{
}

int32_t ArticleAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int32_t category = request->getParamAs<int32_t>("category_id", 0);
        int32_t role = request->getParamAs<int32_t>("role", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);
        std::string keyword = request->getParam("keyword");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t user_role = current_user->getRole();
        if (user_role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto stats = ArticleMgr::GetInstance()->getStats(category, role, days, keyword);

        result->setErrno(errcode::SUCCESS);
        result->set("total_articles", stats.total);
        result->set("pending_articles", stats.pending);
        result->set("published_articles", stats.published);
        result->set("rejected_articles", stats.rejected);
        result->set("reported_articles", stats.reported);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
