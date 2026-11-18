#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class ChunkUploadServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<ChunkUploadServlet> ptr;
    ChunkUploadServlet();

protected:
    virtual int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

    int32_t handleInit(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    int32_t handleComplete(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    int32_t handleCancel(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    int32_t handleUpload(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    int32_t handleStatus(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    void parseBizInfo(const std::string& biz_info, std::string& biz_type, std::string& biz_id);

    bool dumpToResource(const std::string& biz_type, int64_t biz_id, const std::string& path, const std::string& hash_key, int64_t uid, size_t size);
};

} // namespace servlet
} // namespace blog
