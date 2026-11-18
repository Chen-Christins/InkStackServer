/**
 * @file UserRpcService.hpp
 * @brief 用户服务对外 RPC 方法注册
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

namespace blog {
namespace rpc {

/**
 * @brief 注册用户服务对外 RPC 方法
 * @details 契约见 protocol/xml/protocol_user.xml，CmdID 1001~1006。
 *          只暴露读接口，用户写操作不对外。
 */
void RegisterUserRpcMethods();

} // namespace rpc
} // namespace blog
