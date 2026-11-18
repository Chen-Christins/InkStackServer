/**
 * @file RpcEndpoint.hpp
 * @brief 服务间 RPC 的功能类型、实例身份与路由构造
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>

#include <chen/rpc/rpc_client.h>

namespace blog {
namespace rpc {

/**
 * @brief 功能类型，与 protocol/xml 下的契约一一对应
 */
enum FuncId : uint32_t {
    FUNC_USER = 1,     ///< 用户服务
    FUNC_ARTICLE = 2,  ///< 文章服务
    FUNC_COMMENT = 3,  ///< 评论服务
    FUNC_PLATFORM = 4, ///< 平台服务
};

/**
 * @brief 获取 hub 地址
 * @return 形如 127.0.0.1:8092，来自配置 rpc.hub.address
 */
const std::string& GetHubAddress();

/**
 * @brief 获取跨服务调用超时
 * @return 毫秒，来自配置 rpc.hub.call_timeout_ms，默认 3000
 */
uint64_t GetCallTimeoutMs();

/**
 * @brief 获取 hub 断线重连间隔
 * @return 毫秒，来自配置 rpc.hub.reconnect_interval_ms，默认 5000
 */
uint64_t GetReconnectIntervalMs();

/**
 * @brief 生成本进程唯一的 peer_id
 * @param func_id 功能类型
 * @return peer_id
 * @note 热重载期间新旧 worker 并存，peer_id 必须每进程唯一，
 *       否则 hub 的 RpcHubRegistry 会按 peer_id 冲突拒绝注册
 */
uint32_t MakePeerId(uint32_t func_id);

/**
 * @brief 构造按功能类型路由的描述
 * @param func_id 功能类型
 * @return GROUPID 路由（单实例场景 group_id 固定为 0）
 */
chen::rpc::RpcRouting RouteTo(uint32_t func_id);

/**
 * @brief 构造广播路由描述
 * @param func_id 功能类型
 * @return BROADCAST 路由（单向，不等回包）
 */
chen::rpc::RpcRouting BroadcastTo(uint32_t func_id);

} // namespace rpc
} // namespace blog
