#include "article/servlets/article/ArticleViewServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "article/manager/ArticleManager.hpp"

namespace blog {
namespace servlet {

ArticleViewServlet::ArticleViewServlet()
    : BlogLoginedServlet("ArticleViewServlet")
{
}

int32_t ArticleViewServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");

        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article || article->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_NOT_FOUND);
            break;
        }

        std::string cookie_id = std::to_string(uid);
        ArticleMgr::GetInstance()->incViews(article_id, cookie_id, uid);

        result->set("views", article->getViews());
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
