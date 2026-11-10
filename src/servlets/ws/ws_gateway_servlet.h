/**
 * @file ws_gateway_servlet.h
 * @brief 统一 WebSocket 入口（/ws/v1/session）
 * @author Christins
 * @date 2026-11-11
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/http/ws_servlet.h>

namespace blog {
namespace servlet {

/**
 * @brief 统一 WebSocket Gateway
 * @details 一个设备一条连接，复用 notification/presence/chat/system 四类通道。
 */
class WSGatewayServlet : public chen::http::WSServlet {
public:
    typedef std::shared_ptr<WSGatewayServlet> ptr;

    WSGatewayServlet();

    int32_t onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;

    int32_t onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;

    int32_t handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session) override;
};

} // namespace servlet
} // namespace blog
