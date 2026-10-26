#include "article_admin_lists_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

ArticleAdminListsServlet::ArticleAdminListsServlet()
    : BlogLoginedServlet("ArticleAdminListsServlet")
{
}

int32_t ArticleAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t state = request->getParamAs<int32_t>("state", 0);
        int64_t category = request->getParamAs<int64_t>("category_id", 0);
        int32_t rrole = request->getParamAs<int32_t>("role", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<data::ArticleInfo::ptr> articles;
        uint64_t total = ArticleMgr::GetInstance()->listByPages(articles, offset, state, category, rrole, days, page_size, true);

        result->set("total", total);
        auto& list = result->jsondata["list"];
        for (const auto& atc : articles) {
            const auto& user = UserMgr::GetInstance()->get(atc->getUserId());
            Json::Value item;
            item["id"] = atc->getId();
            item["user_id"] = atc->getUserId();
            if (user) {
                item["author"] = user->getName();
                item["email"] = user->getEmail();
                item["author_role"] = user->getRole();
            }
            item["title"] = atc->getTitle();
            item["state"] = atc->getState();
            item["views"] = atc->getViews();
            item["praise"] = atc->getPraise();
            item["favorites"] = atc->getFavorites();
            item["publish_time"] = atc->getPublishTime();
            item["summary"] = atc->getContent().substr(0, 100);
            item["comment_count"] = CommentMgr::GetInstance()->countByArticle(atc->getId());
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
}