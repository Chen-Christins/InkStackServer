/**
 * @file BroadcastListServlet.hpp
 * @brief 广播列表 — 前端轮询获取活跃广播
 * @author Christins
 * @date 2026-07-01
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class BroadcastListServlet : public BlogServlet {
public:
    BroadcastListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
