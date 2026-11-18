/**
 * @file RdTrendServlet.hpp
 * @brief 研发平台趋势分析接口
 * @author Christins
 * @date 2026-06-25
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"
#include <ctime>
#include <json/json.h>
#include <map>
#include <string>
#include <vector>

namespace blog {
namespace servlet {

struct TrendContext {
    chen::IDB::ptr db;
    std::vector<int64_t> target_orgs;
    time_t start_date = 0;
    time_t end_date = 0;
    std::string granularity;

    // 统计中间结果
    int64_t new_req = 0, bug_total = 0, new_task = 0;
    int64_t done_req = 0, done_bug = 0, done_task = 0;
    int64_t total_completed = 0;
    int64_t review_pass_rate = 0;
    double avg_cycle_time = 0.0;
    int64_t task_delta = 0;
    double cycle_delta = 0.0;

    // 趋势序列
    std::map<std::string, Json::Value> series_map;
};

class RdTrendServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdTrendServlet> ptr;
    RdTrendServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    int64_t countBySql(const TrendContext& ctx, const std::string& table, int32_t min_status = -1);
    double avgCycleTime(const TrendContext& ctx, const std::string& table, int32_t min_status);
    void addSeriesBySql(TrendContext& ctx, const std::string& table, int32_t min_status, const std::string& field_name);
};

} // namespace servlet
} // namespace blog
