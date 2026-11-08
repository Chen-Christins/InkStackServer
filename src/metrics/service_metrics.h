/**
 * @file service_metrics.h
 * @brief 服务运行指标注册、路由埋点与 QPS 采样
 * @author Christins
 * @date 2026-11-08
 * @copyright Apache 2.0
 */
#pragma once

#include <string>
#include <vector>

#include <chen/http/http_server.h>
#include <chen/http/ws_server.h>
#include <chen/metrics/metrics.h>

namespace blog {
namespace metrics {

/**
 * @brief 单个路由的请求指标
 * @note 同时注册 QPS 指标（值为 QPS × 1000）：
 *       - blog_http_route_avg_qps_milli：启动以来平均 QPS
 *       - blog_http_route_recent_qps_milli：最近一个采样窗口 QPS
 */
struct RouteMetrics {
    chen::metrics::Counter* requests = nullptr;
    chen::metrics::Counter* errors = nullptr;
    chen::metrics::Histogram* duration = nullptr;
};

/**
 * @brief 注册服务级指标
 * @param name 服务名称
 * @param version 服务版本
 * @param http_servers HTTP 服务器列表，用于采集活跃连接数
 * @param ws_servers WebSocket 服务器列表，用于采集活跃连接数
 */
void Init(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers);

/**
 * @brief 获取指定路由的指标，首次调用时完成注册
 * @param route 注册路由模板（如 /api/v1/messages/conversations/:id）
 */
RouteMetrics GetRouteMetrics(const std::string& route);

/**
 * @brief 记录一次请求
 * @param route_metrics 路由指标
 * @param used_us 请求耗时（微秒）
 * @param err 业务错误码，0 表示成功
 */
void RecordRequest(const RouteMetrics& route_metrics, uint64_t used_us, int32_t err);

/**
 * @brief 采样最近窗口 QPS，由模块定时器周期性调用
 */
void SampleRates();

} // namespace metrics
} // namespace blog
