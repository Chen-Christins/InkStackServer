#include "common/rpc/RpcEndpoint.hpp"

#include <unistd.h>

#include <chen/config/config.h>

namespace blog {
namespace rpc {

namespace {

/**
 * @brief hub 地址
 * @note 单实例部署时 4 个服务与 hub 在同一台机器，直接连配置的 rpc 端口
 */
chen::ConfigVar<std::string>::ptr g_hub_address =
    chen::Config::Lookup<std::string>("rpc.hub.address", std::string("127.0.0.1:8092"), "rpc hub address");

/// 跨服务调用默认超时（毫秒）
chen::ConfigVar<uint64_t>::ptr g_call_timeout_ms =
    chen::Config::Lookup<uint64_t>("rpc.hub.call_timeout_ms", static_cast<uint64_t>(3000), "rpc call timeout in ms");

/// hub 断线重连间隔（毫秒）
chen::ConfigVar<uint64_t>::ptr g_reconnect_interval_ms =
    chen::Config::Lookup<uint64_t>("rpc.hub.reconnect_interval_ms", static_cast<uint64_t>(5000), "rpc hub reconnect interval in ms");

/// peer_id 中分配给进程号的空间
constexpr uint32_t kPeerIdPidSpace = 100000;

} // namespace

const std::string& GetHubAddress()
{
    return g_hub_address->getValue();
}

uint64_t GetCallTimeoutMs()
{
    return g_call_timeout_ms->getValue();
}

uint64_t GetReconnectIntervalMs()
{
    return g_reconnect_interval_ms->getValue();
}

uint32_t MakePeerId(uint32_t func_id)
{
    return func_id * kPeerIdPidSpace + static_cast<uint32_t>(::getpid() % kPeerIdPidSpace);
}

chen::rpc::RpcRouting RouteTo(uint32_t func_id)
{
    chen::rpc::RpcRouting routing;
    routing.method = chen::rpc::RoutingMethod::GROUPID;
    routing.func_id = func_id;
    routing.group_id = 0;
    return routing;
}

chen::rpc::RpcRouting BroadcastTo(uint32_t func_id)
{
    chen::rpc::RpcRouting routing;
    routing.method = chen::rpc::RoutingMethod::BROADCAST;
    routing.func_id = func_id;
    return routing;
}

} // namespace rpc
} // namespace blog
