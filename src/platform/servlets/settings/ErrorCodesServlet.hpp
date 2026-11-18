#pragma once

#include "common/frame/Struct.hpp"

namespace blog::servlet {

/**
 * @brief 错误码下发接口
 * GET /api/v1/error-codes?lang=zh-CN
 * 返回指定语言的错误码映射表，前端缓存后用于 errno → 消息的转换
 */
class ErrorCodesServlet : public BlogServlet {
public:
    typedef std::shared_ptr<ErrorCodesServlet> ptr;
    ErrorCodesServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace blog::servlet
