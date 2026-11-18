/**
 * @file UserDeviceKickServlet.hpp
 * @brief 下线指定登录设备接口
 * @author Christins
 * @date 2026-08-27
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class UserDeviceKickServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<UserDeviceKickServlet> ptr;
    UserDeviceKickServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
