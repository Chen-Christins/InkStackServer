#include "article/servlets/article/ArticleIsPraisingServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "article/manager/ArticlePraiseRelManager.hpp"

namespace blog {
namespace servlet {

ArticleIsPraisingServlet::ArticleIsPraisingServlet()
    : BlogLoginedServlet("ArticleIsPraisingServlet")
{
}

int32_t ArticleIsPraisingServlet::handle(
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

        bool praising = ArticlePraiseRelMgr::GetInstance()->isPraising(uid, article_id);

        result->setErrno(errcode::SUCCESS);
        result->set("is_praising", praising);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
