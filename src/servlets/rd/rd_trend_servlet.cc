#include "rd_trend_servlet.h"

#include <chen/db/query_builder.h>
#include <chen/log/log.h>

#include <cmath>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "blog/data/organization_repo_prs_info.h"
#include "blog/data/task_info.h"
#include "rd_helper.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

RdTrendServlet::RdTrendServlet()
    : BlogLoginedServlet("RdTrendServlet")
{
}

// ==================== 查询辅助函数 ====================

/// 为 QueryBuilder 添加 is_deleted + org 过滤条件
static void addBaseConds(chen::QueryBuilder::ptr qb, const TrendContext& ctx)
{
    qb->where("is_deleted", "=", (int64_t)0);
    if (!ctx.target_orgs.empty()) {
        qb->whereIn("org_id", ctx.target_orgs);
    }
}

/// 为 QueryBuilder 添加标准时间范围条件
static void addTimeCond(chen::QueryBuilder::ptr qb, time_t start, time_t end)
{
    qb->whereSQL("create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    qb->whereSQL("create_time < FROM_UNIXTIME(?)", (int64_t)end);
}

/// 为 QueryBuilder 添加 PR 表的时间范围条件
static void addPRTimeCond(chen::QueryBuilder::ptr qb, time_t start, time_t end)
{
    qb->whereSQL("prs.create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    qb->whereSQL("prs.create_time < FROM_UNIXTIME(?)", (int64_t)end);
}

int64_t RdTrendServlet::countBySql(const TrendContext& ctx, const std::string& table, int32_t min_status)
{
    auto qb = chen::QueryBuilder::Create(table);
    addBaseConds(qb, ctx);
    if (min_status >= 0) {
        qb->where("status", ">=", (int64_t)min_status);
    }
    addTimeCond(qb, ctx.start_date, ctx.end_date);
    int64_t total = 0;
    qb->executeCount(total, ctx.db);
    return total;
}

double RdTrendServlet::avgCycleTime(const TrendContext& ctx, const std::string& table, int32_t min_status)
{
    auto qb = chen::QueryBuilder::Create(table);
    qb->select("AVG(TIMESTAMPDIFF(SECOND, create_time, update_time)) / 86400.0");
    addBaseConds(qb, ctx);
    qb->where("status", ">=", (int64_t)min_status);
    addTimeCond(qb, ctx.start_date, ctx.end_date);

    double result = 0.0;
    qb->queryScalarDouble(result, ctx.db);
    return result;
}

void RdTrendServlet::addSeriesBySql(TrendContext& ctx, const std::string& table, int32_t min_status, const std::string& field_name)
{
    auto qb = chen::QueryBuilder::Create(table);
    qb->select("DATE_FORMAT(create_time, '%Y-%m-%d') as d, COUNT(*) as cnt");
    addBaseConds(qb, ctx);
    if (min_status >= 0) {
        qb->where("status", ">=", (int64_t)min_status);
    }
    addTimeCond(qb, ctx.start_date, ctx.end_date);
    qb->groupBy("d");
    qb->orderBy("d", "ASC");

    std::vector<std::pair<std::string, int64_t>> rows;
    if (qb->queryPairs<std::string, int64_t>(rows, ctx.db) == 0) {
        for (auto& [d, cnt] : rows) {
            std::string key = d;
            if (ctx.granularity == "week") {
                key = rd::FormatGroupKey(rd::ParseDate(d), "week");
            } else if (ctx.granularity == "month") {
                key = d.substr(0, 7);
            }
            auto it = ctx.series_map.find(key);
            if (it != ctx.series_map.end()) {
                it->second[field_name] = it->second[field_name].asInt64() + cnt;
            }
        }
    }
}

// ==================== 核心处理 ====================

int32_t RdTrendServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    TrendContext ctx;
    do {
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
        int64_t period_days = request->getParamAs<int64_t>("period_days", 30);
        std::string start_date_str = request->getParam("start_date");
        std::string end_date_str = request->getParam("end_date");
        ctx.granularity = request->getParam("granularity", "day");

        if (ctx.granularity != "day" && ctx.granularity != "week" && ctx.granularity != "month") {
            ctx.granularity = "day";
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
        int32_t system_role = user->getRole();
        bool is_admin = (system_role == UserManager::Role::ADMIN);

        time_t now = time(0);
        if (!end_date_str.empty()) {
            ctx.end_date = rd::ParseDate(end_date_str) + 86400;
        } else {
            // 左闭右开：结束点取明天 00:00，保证 series 包含今天
            ctx.end_date = chen::TimeUtil::GetDayStart(now) + 86400;
        }
        if (!start_date_str.empty()) {
            ctx.start_date = rd::ParseDate(start_date_str);
        } else {
            ctx.start_date = ctx.end_date - period_days * 86400;
        }

        if (ctx.start_date >= ctx.end_date) {
            result->setErrno(errcode::PARAM_INVALID, "日期范围不合法");
            break;
        }

        // 确定统计范围
        if (org_id) {
            ctx.target_orgs.push_back(org_id);
        } else if (!is_admin) {
            std::vector<data::OrganizationUserRelInfo::ptr> user_orgs;
            OrganizationUserRelMgr::GetInstance()->getOrgByUserId(user_orgs, uid, OrganizationUserRelManager::Status::APPROVED, true);
            for (auto& rel : user_orgs) {
                ctx.target_orgs.push_back(rel->getOrgId());
            }
            if (ctx.target_orgs.empty()) {
                result->setErrno(errcode::ACCESS_DENIED, "无权访问任何组织");
                break;
            }
        }
        // 管理员不传 org_id → target_orgs 为空 → 查所有组织

        // 权限校验
        if (org_id && !is_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED || rel->getRole() < OrganizationManager::Role::REPORTER) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        }

        ctx.db = getDB();
        if (!ctx.db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        // ====== 汇总统计 ======
        ctx.new_req = countBySql(ctx, "requirement");
        ctx.done_req = countBySql(ctx, "requirement", 3);
        ctx.bug_total = countBySql(ctx, "bug");
        ctx.done_bug = countBySql(ctx, "bug", 2);
        ctx.new_task = countBySql(ctx, "task");
        ctx.done_task = countBySql(ctx, "task", 2);
        ctx.total_completed = ctx.done_req + ctx.done_bug + ctx.done_task;

        // avg_review_pass_rate
        {
            auto buildBaseQB = [&]() {
                auto qb = data::OrganizationRepoPrsInfoDao::newQuery("prs");
                qb->join("INNER", "organization_repos repos", "prs.repo_id = repos.id");
                qb->where("prs.review_status", "!=", std::string(""));
                qb->where("prs.review_status", "!=", std::string("pending"));
                if (!ctx.target_orgs.empty()) {
                    qb->whereIn("repos.org_id", ctx.target_orgs);
                }
                addPRTimeCond(qb, ctx.start_date, ctx.end_date);
                return qb;
            };

            auto totalQB = buildBaseQB();
            int64_t total_reviewed = 0;
            totalQB->executeCount(total_reviewed, ctx.db);

            if (total_reviewed > 0) {
                auto approvedQB = buildBaseQB();
                approvedQB->where("prs.review_status", "=", std::string("approved"));
                int64_t approved = 0;
                approvedQB->executeCount(approved, ctx.db);
                ctx.review_pass_rate = approved * 100 / total_reviewed;
            }
        }

        // avg_cycle_time
        {
            double req_cycle = avgCycleTime(ctx, "requirement", 3);
            double bug_cycle = avgCycleTime(ctx, "bug", 2);
            double task_cycle = avgCycleTime(ctx, "task", 2);
            if (ctx.total_completed > 0) {
                ctx.avg_cycle_time = (req_cycle * ctx.done_req + bug_cycle * ctx.done_bug + task_cycle * ctx.done_task) / ctx.total_completed;
            }
        }

        // task_delta
        {
            auto qbEnd = data::TaskInfoDao::newQuery();
            addBaseConds(qbEnd, ctx);
            qbEnd->whereSQL("create_time < FROM_UNIXTIME(?)", (int64_t)ctx.end_date);
            int64_t tasks_at_end = 0;
            qbEnd->executeCount(tasks_at_end, ctx.db);

            auto qbStart = data::TaskInfoDao::newQuery();
            addBaseConds(qbStart, ctx);
            qbStart->whereSQL("create_time < FROM_UNIXTIME(?)", (int64_t)ctx.start_date);
            int64_t tasks_at_start = 0;
            qbStart->executeCount(tasks_at_start, ctx.db);

            ctx.task_delta = tasks_at_end - tasks_at_start;
        }

        // cycle_delta
        {
            time_t prev_end = ctx.start_date;
            time_t prev_start = ctx.start_date - (ctx.end_date - ctx.start_date);
            if (prev_start > 0) {
                TrendContext prev_ctx;
                prev_ctx.db = ctx.db;
                prev_ctx.target_orgs = ctx.target_orgs;
                prev_ctx.start_date = prev_start;
                prev_ctx.end_date = prev_end;

                double prev_req = avgCycleTime(prev_ctx, "requirement", 3);
                double prev_bug = avgCycleTime(prev_ctx, "bug", 2);
                double prev_task = avgCycleTime(prev_ctx, "task", 2);
                if (ctx.total_completed > 0) {
                    double prev_total = (prev_req * ctx.done_req + prev_bug * ctx.done_bug + prev_task * ctx.done_task) / ctx.total_completed;
                    ctx.cycle_delta = ctx.avg_cycle_time - prev_total;
                }
            }
        }

        // ====== 趋势序列 ======
        Json::Value series_arr(Json::arrayValue);
        {
            std::vector<time_t> date_points;
            time_t cursor = ctx.start_date;
            while (cursor < ctx.end_date) {
                date_points.push_back(cursor);
                if (ctx.granularity == "day") {
                    cursor += 86400;
                } else if (ctx.granularity == "week") {
                    cursor += 7 * 86400;
                } else {
                    struct tm tm;
                    localtime_r(&cursor, &tm);
                    tm.tm_mon += 1;
                    cursor = mktime(&tm);
                }
            }

            for (auto& t : date_points) {
                std::string key = rd::FormatGroupKey(t, ctx.granularity);
                if (ctx.series_map.find(key) == ctx.series_map.end()) {
                    Json::Value pt;
                    pt["date"] = key;
                    pt["requirements"] = 0;
                    pt["bugs"] = 0;
                    pt["tasks"] = 0;
                    pt["completed"] = 0;
                    pt["reopened"] = 0;
                    pt["review_pass_rate"] = 0;
                    pt["cycle_time"] = 0;
                    ctx.series_map[key] = pt;
                }
            }

            addSeriesBySql(ctx, "requirement", -1, "requirements");
            addSeriesBySql(ctx, "bug", -1, "bugs");
            addSeriesBySql(ctx, "task", -1, "tasks");
            addSeriesBySql(ctx, "requirement", 3, "completed");
            addSeriesBySql(ctx, "bug", 2, "completed");
            addSeriesBySql(ctx, "task", 2, "completed");

            for (auto& t : date_points) {
                std::string key = rd::FormatGroupKey(t, ctx.granularity);
                auto it = ctx.series_map.find(key);
                if (it != ctx.series_map.end()) {
                    series_arr.append(it->second);
                }
            }
        }

        // ====== Summary ======
        Json::Value summary;
        summary["completed_total"] = ctx.total_completed;
        summary["bug_total"] = ctx.bug_total;
        summary["avg_review_pass_rate"] = ctx.review_pass_rate;
        summary["avg_cycle_time"] = std::round(ctx.avg_cycle_time * 10.0) / 10.0;
        summary["task_delta"] = ctx.task_delta;
        summary["cycle_delta"] = std::round(ctx.cycle_delta * 10.0) / 10.0;

        result->set("summary", summary);
        result->set("series", series_arr);

        // ====== 工作分布 ======
        int64_t total_items = ctx.new_req + ctx.bug_total + ctx.new_task;
        Json::Value dist;
        {
            auto qb = data::OrganizationRepoPrsInfoDao::newQuery("prs");
            qb->join("INNER", "organization_repos repos", "prs.repo_id = repos.id");
            if (!ctx.target_orgs.empty()) {
                qb->whereIn("repos.org_id", ctx.target_orgs);
            }
            addPRTimeCond(qb, ctx.start_date, ctx.end_date);

            int64_t pr_count = 0;
            qb->executeCount(pr_count, ctx.db);

            int64_t dist_total = total_items + pr_count;
            if (dist_total > 0) {
                dist["requirement_delivery"] = std::round(ctx.new_req * 100.0 / dist_total);
                dist["bug_fix"] = std::round(ctx.bug_total * 100.0 / dist_total);
                dist["rd_task"] = std::round(ctx.new_task * 100.0 / dist_total);
                dist["code_review"] = std::round(pr_count * 100.0 / dist_total);
            } else {
                dist["requirement_delivery"] = 0;
                dist["bug_fix"] = 0;
                dist["rd_task"] = 0;
                dist["code_review"] = 0;
            }
        }
        result->set("work_distribution", dist);

        // ====== 团队健康度 ======
        Json::Value health;
        health["throughput"] = std::min((int64_t)100, ctx.total_completed);
        if (total_items > 0) {
            health["bug_pressure"] = 100 - std::min(ctx.bug_total * 100 / total_items, (int64_t)80);
        } else {
            health["bug_pressure"] = 100;
        }
        health["review_efficiency"] = ctx.review_pass_rate;
        health["rework_risk"] = 0;
        result->set("team_health", health);

        // ====== 洞察文案 ======
        result->set("insights", Json::Value(Json::arrayValue));
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
