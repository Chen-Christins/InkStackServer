/**
 * @file user_device_list_servlet.h
 * @brief 用户登录设备列表接口
 * @author Christins
 * @date 2026-08-27
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class UserDeviceListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<UserDeviceListServlet> ptr;
    UserDeviceListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
