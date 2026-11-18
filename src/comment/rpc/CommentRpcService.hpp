/**
 * @file CommentRpcService.hpp
 * @brief 评论服务对外 RPC 方法注册
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

namespace blog {
namespace rpc {

/**
 * @brief 注册评论服务对外 RPC 方法
 * @details 契约见 protocol/xml/protocol_comment.xml，CmdID 4001~4004。
 *          只暴露读接口，发表/审核/删除不对外。
 */
void RegisterCommentRpcMethods();

} // namespace rpc
} // namespace blog
