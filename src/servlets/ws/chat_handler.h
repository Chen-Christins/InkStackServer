/**
 * @file chat_handler.h
 * @brief 统一 WebSocket 上的私信帧处理
 * @author Christins
 * @date 2026-11-11
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/http/ws_session.h>

#include <json/json.h>

namespace blog {
namespace servlet {

/**
 * @brief 私信 WebSocket 帧处理器
 * @details 由统一 Gateway 调用，负责 send/read 两类业务帧。
 */
class ChatHandler {
public:
    /**
     * @brief 处理私信帧
     * @param uid 已认证用户 ID
     * @param root 客户端 JSON 帧
     * @param session 当前连接
     * @return 0 表示已处理，-1 表示不是私信帧或参数无效
     */
    static int32_t Handle(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session);
};

} // namespace servlet
} // namespace blog
