/**
 * @file PlatformRpcService.hpp
 * @brief 平台服务对外 RPC 方法注册
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

namespace blog {
namespace rpc {

/**
 * @brief 注册平台服务对外 RPC 方法
 * @details 契约见 protocol/xml/protocol_platform.xml，CmdID 2000~2005。
 *          平台是所有服务中唯一提供「系统设置 / 事件推送 / 设备踢下线」的一方，
 *          自身不反向依赖其它服务。
 */
void RegisterPlatformRpcMethods();

} // namespace rpc
} // namespace blog
