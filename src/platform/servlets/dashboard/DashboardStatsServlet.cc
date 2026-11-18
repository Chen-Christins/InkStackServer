#include "platform/servlets/dashboard/DashboardStatsServlet.hpp"

#include "article/manager/ArticleManager.hpp"
#include "comment/manager/CommentManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

DashboardStatsServlet::DashboardStatsServlet()
    : BlogLoginedServlet("DashboardStatsServlet")
{
}

int32_t DashboardStatsServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        int64_t now = time(0);
        int64_t today_start = now - (now % 86400);
        int64_t yesterday_start = today_start - 86400;
        int64_t week_ago_start = today_start - 7 * 86400;
        int64_t two_weeks_ago_start = today_start - 14 * 86400;

        auto pct_change = [](int64_t cur, int64_t prev) -> int64_t {
            if (prev == 0) {
                return cur > 0 ? 100 : 0;
            }
            return (cur - prev) * 100 / prev;
        };

        // --- users ---
        std::vector<int64_t> ids;
        UserMgr::GetInstance()->getAllIds(ids, true);
        int64_t total_users = ids.size();

        int64_t this_week_users = 0, last_week_users = 0;
        int64_t today_users = 0, yesterday_users = 0;
        for (auto id : ids) {
            auto u = UserMgr::GetInstance()->get(id);
            if (!u) {
                continue;
            }
            auto ct = u->getCreateTime();
            if (ct >= week_ago_start) {
                this_week_users++;
            } else if (ct >= two_weeks_ago_start) {
                last_week_users++;
            }
            if (ct >= today_start) {
                today_users++;
            } else if (ct >= yesterday_start) {
                yesterday_users++;
            }
        }

        // --- articles ---
        auto astats = ArticleMgr::GetInstance()->getStats(0, -1, 0, "");
        int64_t total_articles = astats.total;

        std::vector<data::ArticleInfo::ptr> articles;
        ArticleMgr::GetInstance()->listByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true);

        int64_t this_week_articles = 0, last_week_articles = 0;
        int64_t today_articles = 0, yesterday_articles = 0;
        for (auto& a : articles) {
            auto ct = a->getCreateTime();
            if (ct >= week_ago_start) {
                this_week_articles++;
            } else if (ct >= two_weeks_ago_start) {
                last_week_articles++;
            }
            if (ct >= today_start) {
                today_articles++;
            } else if (ct >= yesterday_start) {
                yesterday_articles++;
            }
        }

        // --- comments ---
        auto cstats = CommentMgr::GetInstance()->getStats();
        int64_t total_comments = cstats.total;

        std::vector<data::CommentInfo::ptr> comments;
        CommentMgr::GetInstance()->listByAdmin(comments, 1, 0x7FFFFFFFLL, 0, "", 0, -1);
        int64_t this_week_comments = 0, last_week_comments = 0;
        int64_t today_comments = 0, yesterday_comments = 0;
        for (auto& c : comments) {
            auto ct = c->getCreateTime();
            if (ct >= week_ago_start) {
                this_week_comments++;
            } else if (ct >= two_weeks_ago_start) {
                last_week_comments++;
            }
            if (ct >= today_start) {
                today_comments++;
            } else if (ct >= yesterday_start) {
                yesterday_comments++;
            }
        }

        // today_visits = today's activity (same definition as trend endpoint)
        int64_t today_visits = today_users + today_articles + today_comments;
        int64_t yesterday_visits = yesterday_users + yesterday_articles + yesterday_comments;

        result->set("total_users", total_users);
        result->set("total_users_change", pct_change(this_week_users, last_week_users));
        result->set("total_articles", total_articles);
        result->set("total_articles_change", pct_change(this_week_articles, last_week_articles));
        result->set("total_comments", total_comments);
        result->set("total_comments_change", pct_change(this_week_comments, last_week_comments));
        result->set("today_visits", today_visits);
        result->set("today_visits_change", pct_change(today_visits, yesterday_visits));
        result->set("new_users_today", today_users);
        result->set("new_users_today_change", pct_change(today_users, yesterday_users));

        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
