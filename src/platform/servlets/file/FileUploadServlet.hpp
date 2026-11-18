#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class FileUploadServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<FileUploadServlet> ptr;
    FileUploadServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
    bool dumpToResource(const std::string& biz_type, int64_t biz_id, const std::string& path, const std::string& hash_key, int64_t uid, size_t size);
};

} // namespace servlet
} // namespace blog
