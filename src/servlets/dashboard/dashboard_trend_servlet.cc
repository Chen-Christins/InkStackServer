#include "dashboard_trend_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"

#include <ctime>

namespace blog {
namespace servlet {

DashboardTrendServlet::DashboardTrendServlet()
    : BlogLoginedServlet("DashboardTrendServlet")
{
}

static std::string FormatDate(int64_t ts)
{
    struct tm t;
    localtime_r(&ts, &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d-%02d", t.tm_mon + 1, t.tm_mday);
    return buf;
}

static const char* WeekdayName(int64_t ts)
{
    static const char* names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    struct tm t;
    localtime_r(&ts, &t);
    return names[t.tm_wday];
}

int32_t DashboardTrendServlet::handle(
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

        int32_t period = request->getParamAs<int32_t>("period", 7);
        if (period != 7 && period != 30 && period != 90) {
            period = 7;
        }

        int64_t now = time(0);
        int64_t today_start = now - (now % 86400);
        int64_t period_start;
        int32_t num_buckets;
        int64_t bucket_sec;

        if (period == 90) {
            num_buckets = 13;
            bucket_sec = 7 * 86400;
            period_start = today_start - (num_buckets - 1) * bucket_sec;
        } else {
            num_buckets = period;
            bucket_sec = 86400;
            period_start = today_start - (period - 1) * 86400;
        }

        std::vector<int64_t> buckets(num_buckets, 0);

        // aggregate users
        std::vector<int64_t> ids;
        UserMgr::GetInstance()->getAllIds(ids, true);
        for (auto id : ids) {
            auto u = UserMgr::GetInstance()->get(id);
            if (!u) {
                continue;
            }
            int64_t ts = u->getCreateTime();
            if (ts >= period_start) {
                int32_t idx = (ts - period_start) / bucket_sec;
                if (idx < num_buckets) {
                    buckets[idx]++;
                }
            }
        }

        // aggregate articles
        std::vector<data::ArticleInfo::ptr> articles;
        ArticleMgr::GetInstance()->listByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true);
        for (auto& a : articles) {
            int64_t ts = a->getCreateTime();
            if (ts >= period_start) {
                int32_t idx = (ts - period_start) / bucket_sec;
                if (idx < num_buckets) {
                    buckets[idx]++;
                }
            }
        }

        // aggregate comments
        std::vector<data::CommentInfo::ptr> comments;
        CommentMgr::GetInstance()->listByAdmin(comments, 1, 0x7FFFFFFFLL, 0, "", 0, -1);
        for (auto& c : comments) {
            int64_t ts = c->getCreateTime();
            if (ts >= period_start) {
                int32_t idx = (ts - period_start) / bucket_sec;
                if (idx < num_buckets) {
                    buckets[idx]++;
                }
            }
        }

        int64_t total_visits = 0;
        int64_t max_visits = 0;
        Json::Value list(Json::arrayValue);

        for (int32_t i = 0; i < num_buckets; i++) {
            int64_t bucket_ts = period_start + i * bucket_sec;
            int64_t visits = buckets[i];
            total_visits += visits;
            if (visits > max_visits) {
                max_visits = visits;
            }

            Json::Value item;
            if (period == 7) {
                item["label"] = WeekdayName(bucket_ts);
                item["date"] = FormatDate(bucket_ts);
            } else if (period == 30) {
                item["label"] = FormatDate(bucket_ts);
                item["date"] = FormatDate(bucket_ts);
            } else {
                int64_t week_end = bucket_ts + bucket_sec - 1;
                if (week_end > now) {
                    week_end = now;
                }
                item["label"] = "第" + std::to_string(i + 1) + "周";
                item["date"] = FormatDate(bucket_ts) + " ~ " + FormatDate(week_end);
            }
            item["visits"] = visits;
            list.append(item);
        }

        result->set("list", list);
        result->set("total_visits", total_visits);
        result->set("avg_visits", num_buckets > 0 ? (double)total_visits / num_buckets : 0);
        result->set("max_visits", max_visits);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
