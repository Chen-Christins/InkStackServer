#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class FileDownloadServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<FileDownloadServlet> ptr;
    FileDownloadServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
    bool checkPermission(int32_t system_role);
};

} // namespace servlet
} // namespace blog
