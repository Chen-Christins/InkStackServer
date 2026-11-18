#pragma once

#include "common/frame/Struct.hpp"

namespace blog::servlet {

class SystemSettingsServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<SystemSettingsServlet> ptr;
    SystemSettingsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace blog::servlet
