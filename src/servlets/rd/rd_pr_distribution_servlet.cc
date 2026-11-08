#include "rd_pr_distribution_servlet.h"

#include <chen/db/query_builder.h>
#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "blog/data/organization_repo_prs_info.h"
#include "rd_helper.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

/// 为 PR 查询添加仓库关联、组织和时间范围条件
static void AddPrBaseConds(chen::QueryBuilder::ptr qb, const std::vector<int64_t>& orgs, time_t start, time_t end)
{
    qb->join("INNER", "organization_repos repos", "prs.repo_id = repos.id");
    if (!orgs.empty()) {
        qb->whereIn("repos.org_id", orgs);
    }
    qb->whereSQL("prs.create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    qb->whereSQL("prs.create_time < FROM_UNIXTIME(?)", (int64_t)end);
}

/// 按天聚合 PR 创建数，写入指定字段：total=桶内创建数，merged=其中已合入数
static bool AddPrSeries(
    chen::IDB::ptr db,
    const std::vector<int64_t>& orgs,
    time_t start,
    time_t end,
    const std::string& granularity,
    bool merged_only,
    const std::string& field,
    std::map<std::string, Json::Value>& series)
{
    auto qb = data::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        ERROR(logger) << "pr-distribution newQuery failed field=" << field;
        return false;
    }
    qb->select("DATE_FORMAT(prs.create_time, '%Y-%m-%d') as d, COUNT(*) as cnt");
    AddPrBaseConds(qb, orgs, start, end);
    if (merged_only) {
        // 未合入时 merged_at 可能被写成 0（1970-01-01），这里排除零值，避免把所有 PR 都算成已合入
        qb->whereSQL("(prs.state = 'merged' OR prs.merged_at > ?)", std::string("1970-01-02 00:00:00"));
    }
    qb->groupBy("d");
    qb->orderBy("d", "ASC");

    std::vector<std::pair<std::string, int64_t>> rows;
    if (qb->queryPairs<std::string, int64_t>(rows, db) != 0) {
        ERROR(logger) << "pr-distribution query failed field=" << field;
        return false;
    }

    for (auto& [d, cnt] : rows) {
        std::string key = d;
        if (granularity == "week") {
            key = rd::FormatGroupKey(rd::ParseDate(d), "week");
        } else if (granularity == "month") {
            key = d.substr(0, 7);
        }
        auto it = series.find(key);
        if (it != series.end()) {
            it->second[field] = it->second[field].asInt64() + cnt;
        }
    }
    return true;
}

RdPrDistributionServlet::RdPrDistributionServlet()
    : BlogLoginedServlet("RdPrDistributionServlet")
{
}

int32_t RdPrDistributionServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string range = request->getParam("range");
        std::string granularity = request->getParam("granularity");
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        if (range.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "range is required");
            break;
        }
        if (granularity.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "granularity is required");
            break;
        }
        if (org_id < 0) {
            result->setErrno(errcode::PARAM_INVALID, "invalid org_id");
            break;
        }

        bool is_15d = (range == "15d");
        if (!is_15d && range != "3m") {
            result->setErrno(errcode::PARAM_INVALID, "range must be 15d or 3m");
            break;
        }
        if ((is_15d && granularity != "day") || (!is_15d && granularity != "week")) {
            result->setErrno(errcode::PARAM_INVALID, "granularity does not match range");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        bool is_admin = (user->getRole() == UserManager::Role::ADMIN);

        std::vector<int64_t> target_orgs;
        if (org_id > 0) {
            auto org = OrganizationMgr::GetInstance()->get(org_id);
            if (!org || org->getIsDeleted()) {
                result->setErrno(errcode::ORG_NOT_FOUND);
                break;
            }
            if (!is_admin) {
                auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
                if (!rel || rel->getIsDeleted() || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED
                    || rel->getRole() < OrganizationManager::Role::REPORTER) {
                    result->setErrno(errcode::ACCESS_DENIED);
                    break;
                }
            }
            target_orgs.push_back(org_id);
        } else if (!is_admin) {
            std::vector<data::OrganizationUserRelInfo::ptr> rels;
            OrganizationUserRelMgr::GetInstance()->getOrgByUserId(rels, uid, OrganizationUserRelManager::Status::APPROVED, true);
            for (auto& rel : rels) {
                if (rel && !rel->getIsDeleted() && rel->getRole() >= OrganizationManager::Role::REPORTER) {
                    target_orgs.push_back(rel->getOrgId());
                }
            }
            if (target_orgs.empty()) {
                result->setErrno(errcode::ACCESS_DENIED, "no accessible organization");
                break;
            }
        }
        // 管理员不传 org_id 时 target_orgs 为空，表示统计全部组织

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        time_t now = time(0);
        time_t today_start = chen::TimeUtil::GetDayStart(now);
        time_t start = 0;
        time_t end = chen::TimeUtil::GetDayStart(today_start + 86400);

        if (is_15d) {
            start = chen::TimeUtil::GetDayStart(today_start - 14 * 86400);
        } else {
            time_t current_monday = chen::TimeUtil::GetWeekStart(today_start);
            start = chen::TimeUtil::GetWeekStart(current_monday - 12 * 7 * 86400);
        }

        std::vector<time_t> date_points;
        time_t cursor = start;
        while (cursor < end) {
            date_points.push_back(cursor);
            if (is_15d) {
                cursor = chen::TimeUtil::GetDayStart(cursor + 86400);
            } else {
                cursor = chen::TimeUtil::GetWeekStart(cursor + 7 * 86400);
            }
        }

        std::map<std::string, Json::Value> series;
        for (auto t : date_points) {
            std::string key = rd::FormatGroupKey(t, granularity);
            Json::Value point;
            point["date"] = key;
            if (is_15d) {
                point["end_date"] = key;
            } else {
                time_t week_end = chen::TimeUtil::GetWeekEnd(t);
                if (week_end > today_start) {
                    week_end = today_start;
                }
                point["end_date"] = rd::FormatDate(week_end);
            }
            point["total"] = 0;
            point["merged"] = 0;
            series[key] = point;
        }

        if (!AddPrSeries(db, target_orgs, start, end, granularity, false, "total", series)
            || !AddPrSeries(db, target_orgs, start, end, granularity, true, "merged", series)) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        Json::Value points(Json::arrayValue);
        int64_t total_sum = 0;
        int64_t merged_sum = 0;
        for (auto t : date_points) {
            std::string key = rd::FormatGroupKey(t, granularity);
            auto it = series.find(key);
            if (it == series.end()) {
                continue;
            }
            total_sum += it->second["total"].asInt64();
            merged_sum += it->second["merged"].asInt64();
            points.append(it->second);
        }

        double merge_rate = total_sum > 0 ? (double)merged_sum * 100.0 / (double)total_sum : 0.0;
        merge_rate = std::round(merge_rate * 10.0) / 10.0;

        result->set("range", range);
        result->set("granularity", granularity);

        Json::Value summary;
        summary["total"] = total_sum;
        summary["merged"] = merged_sum;
        summary["merge_rate"] = merge_rate;
        result->set("summary", summary);

        result->set("points", points);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
