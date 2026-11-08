#include "service_metrics.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <chen/fiber/fiber.h>
#include <chen/util/time_util.h>

#include <unistd.h>

#include "../manager/notification_manager.h"
#include "../util.h"

namespace blog {
namespace metrics {

namespace {

/**
 * @brief 路由指标运行态
 * @details 由 s_routes 持有，生命周期与进程一致，GaugeFunc 回调持有其裸指针
 */
struct RouteState {
    RouteMetrics metrics;
    uint64_t start_ms = 0;
    uint64_t sampled_at_ms = 0;
    uint64_t sampled_requests = 0;
    std::atomic<int64_t> recent_qps_milli{0};
};

std::mutex s_mutex;
std::unordered_map<std::string, std::unique_ptr<RouteState>> s_routes;
std::vector<chen::http::HttpServer::ptr> s_http_servers;
std::vector<chen::http::WSServer::ptr> s_ws_servers;
uint64_t s_start_ms = 0;
bool s_inited = false;

} // namespace

void Init(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_inited) {
        return;
    }
    s_inited = true;
    s_start_ms = chen::TimeUtil::GetCurrentMs();
    s_http_servers = http_servers;
    s_ws_servers = ws_servers;

    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return;
    }

    if (auto* info =
            registry->getGauge("blog_info", "Blog service information", {{"name", name}, {"version", version}, {"pid", std::to_string(getpid())}})) {
        info->set(1);
    }

    registry->getGaugeFunc("blog_process_uptime_seconds", "Blog process uptime in seconds", {}, []() -> int64_t {
        return static_cast<int64_t>((chen::TimeUtil::GetCurrentMs() - s_start_ms) / 1000);
    });

    registry->getGaugeFunc("blog_process_resident_memory_kb", "Blog process resident memory in KB", {}, []() -> int64_t {
        return GetProcessMemoryInfo().rss_kb;
    });

    registry->getGaugeFunc("blog_process_virtual_memory_kb", "Blog process virtual memory in KB", {}, []() -> int64_t {
        return GetProcessMemoryInfo().vm_kb;
    });

    registry->getGaugeFunc("blog_process_threads", "Blog process thread count", {}, []() -> int64_t {
        return GetProcessMemoryInfo().threads;
    });

    registry->getGaugeFunc("blog_system_memory_total_kb", "System total memory in KB", {}, []() -> int64_t {
        return GetSystemMemoryInfo().total_kb;
    });

    registry->getGaugeFunc("blog_system_memory_available_kb", "System available memory in KB", {}, []() -> int64_t {
        return GetSystemMemoryInfo().available_kb;
    });

    registry->getGaugeFunc("blog_fibers", "Current fiber count", {}, []() -> int64_t {
        return chen::Fiber::TotalFibers();
    });

    registry->getGaugeFunc("blog_http_active_connections", "Active HTTP connections", {}, []() -> int64_t {
        int64_t total = 0;
        for (const auto& server : s_http_servers) {
            if (server) {
                total += server->getActiveClientCount();
            }
        }
        return total;
    });

    registry->getGaugeFunc("blog_ws_active_connections", "Active WebSocket connections", {}, []() -> int64_t {
        int64_t total = 0;
        for (const auto& server : s_ws_servers) {
            if (server) {
                total += server->getActiveClientCount();
            }
        }
        return total;
    });

    registry->getGaugeFunc("blog_online_users", "Online notification users", {}, []() -> int64_t {
        auto* manager = NotificationMgr::GetInstance();
        if (!manager) {
            return 0;
        }
        return manager->getOnlineCount();
    });
}

RouteMetrics GetRouteMetrics(const std::string& route)
{
    if (route.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_routes.find(route);
    if (it != s_routes.end()) {
        return it->second->metrics;
    }

    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return {};
    }

    const chen::metrics::Labels labels = {{"route", route}};

    auto state = std::make_unique<RouteState>();
    state->start_ms = chen::TimeUtil::GetCurrentMs();
    state->sampled_at_ms = state->start_ms;
    state->metrics.requests = registry->getCounter("blog_http_requests_total", "Blog HTTP requests total", labels);
    state->metrics.errors = registry->getCounter("blog_http_errors_total", "Blog HTTP error responses total", labels);
    state->metrics.duration = registry->getHistogram("blog_http_request_duration_seconds", "Blog HTTP request duration in seconds", labels);

    RouteState* raw = state.get();
    registry->getGaugeFunc("blog_http_route_avg_qps_milli", "Blog HTTP route average QPS since start multiplied by 1000", labels, [raw]() -> int64_t {
        if (!raw->metrics.requests) {
            return 0;
        }
        const uint64_t now = chen::TimeUtil::GetCurrentMs();
        const uint64_t elapsed_ms = now > raw->start_ms ? now - raw->start_ms : 0;
        if (!elapsed_ms) {
            return 0;
        }
        const double qps = static_cast<double>(raw->metrics.requests->value()) * 1000.0 / static_cast<double>(elapsed_ms);
        return static_cast<int64_t>(qps * 1000.0);
    });

    registry->getGaugeFunc(
        "blog_http_route_recent_qps_milli", "Blog HTTP route QPS in the latest sampling window multiplied by 1000", labels, [raw]() -> int64_t {
            return raw->recent_qps_milli.load(std::memory_order_relaxed);
        });

    const RouteMetrics result = state->metrics;
    s_routes.emplace(route, std::move(state));
    return result;
}

void RecordRequest(const RouteMetrics& route_metrics, uint64_t used_us, int32_t err)
{
    if (route_metrics.requests) {
        route_metrics.requests->inc();
    }
    if (err != 0 && route_metrics.errors) {
        route_metrics.errors->inc();
    }
    if (route_metrics.duration) {
        route_metrics.duration->observe(static_cast<double>(used_us) / 1000000.0);
    }
}

void SampleRates()
{
    const uint64_t now = chen::TimeUtil::GetCurrentMs();

    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& item : s_routes) {
        auto& state = item.second;
        if (!state->metrics.requests) {
            continue;
        }
        const uint64_t total = state->metrics.requests->value();
        const uint64_t elapsed_ms = now > state->sampled_at_ms ? now - state->sampled_at_ms : 0;
        if (elapsed_ms > 0) {
            const uint64_t delta = total >= state->sampled_requests ? total - state->sampled_requests : total;
            const double qps = static_cast<double>(delta) * 1000.0 / static_cast<double>(elapsed_ms);
            state->recent_qps_milli.store(qps * 1000.0, std::memory_order_relaxed);
        }
        state->sampled_requests = total;
        state->sampled_at_ms = now;
    }
}

} // namespace metrics
} // namespace blog
