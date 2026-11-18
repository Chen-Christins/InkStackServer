/**
 * @file ArticleRpcService.hpp
 * @brief 文章服务对外 RPC 方法注册
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

namespace blog {
namespace rpc {

/**
 * @brief 注册文章服务对外 RPC 方法
 * @details 契约见 protocol/xml/protocol_article.xml，CmdID 3001~3006。
 *          只暴露读接口，发布/审核/删除不对外。
 */
void RegisterArticleRpcMethods();

} // namespace rpc
} // namespace blog
