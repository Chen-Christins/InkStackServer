#include "article/servlets/article/ArticlePraiseListServlet.hpp"

#include "article/manager/ArticleManager.hpp"
#include "article/manager/ArticlePraiseRelManager.hpp"
#include "user/manager/UserManager.hpp"

#include <json/json.h>

namespace blog {
namespace servlet {

ArticlePraiseListServlet::ArticlePraiseListServlet()
    : BlogLoginedServlet("ArticlePraiseListServlet")
{
}

int32_t ArticlePraiseListServlet::handle(
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

        int64_t article_id = 0;
        request->checkGetParamAs("article_id", article_id);

        int64_t user_id = 0;
        request->checkGetParamAs("user_id", user_id);

        uint64_t offset = 0;
        request->checkGetParamAs("offset", offset);

        uint64_t size = 20;
        request->checkGetParamAs("size", size);

        std::vector<data::ArticlePraiseRelInfo::ptr> results;

        if (article_id > 0) {
            // list users who praised this article
            ArticlePraiseRelMgr::GetInstance()->listByArticle(results, article_id, offset, size);
            Json::Value arr(Json::arrayValue);
            for (auto& rel : results) {
                Json::Value item;
                item["id"] = rel->getId();
                item["user_id"] = rel->getUserId();
                item["article_id"] = rel->getArticleId();
                item["create_time"] = rel->getCreateTime();
                auto user = UserMgr::GetInstance()->get(rel->getUserId());
                if (user && !user->getIsDeleted()) {
                    item["user_name"] = user->getName();
                    item["user_avatar"] = user->getAvatar();
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", ArticlePraiseRelMgr::GetInstance()->countByArticle(article_id));
        } else {
            // list articles praised by this user
            int64_t query_user_id = user_id > 0 ? user_id : uid;
            ArticlePraiseRelMgr::GetInstance()->listByUser(results, query_user_id, offset, size);
            Json::Value arr(Json::arrayValue);
            for (auto& rel : results) {
                Json::Value item;
                item["id"] = rel->getId();
                item["user_id"] = rel->getUserId();
                item["article_id"] = rel->getArticleId();
                item["create_time"] = rel->getCreateTime();
                auto article = ArticleMgr::GetInstance()->get(rel->getArticleId());
                if (article && !article->getIsDeleted()) {
                    item["title"] = article->getTitle();
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", ArticlePraiseRelMgr::GetInstance()->countByUser(query_user_id));
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
