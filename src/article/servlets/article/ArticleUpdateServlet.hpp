#pragma once

#include "common/frame/Struct.hpp"
#include "blog/data/article_info.h"

namespace blog {
namespace servlet {

class ArticleUpdateServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<ArticleUpdateServlet> ptr;
    ArticleUpdateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    int32_t updateOptionalFields(chen::http::HttpRequest::ptr request, data::ArticleInfo::ptr info, Result::ptr result);
    int32_t syncCategoryRels(chen::http::HttpRequest::ptr request, int64_t id, chen::IDB::ptr db, time_t now, Result::ptr result);
    int32_t syncLabelRels(chen::http::HttpRequest::ptr request, int64_t id, chen::IDB::ptr db, time_t now, Result::ptr result);
};

} // namespace servlet
} // namespace blog
