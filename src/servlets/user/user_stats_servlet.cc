#include "user_stats_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"

#include <chen/db/query_builder.h>

#include <set>

namespace blog {
namespace servlet {

UserStatsServlet::UserStatsServlet()
    : BlogLoginedServlet("UserStatsServlet")
{
}

int32_t UserStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = request->getParamAs<int64_t>("user_id", 0);
        if (!uid) {
            uid = getUserId(request);
        }
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        std::vector<data::ArticleInfo::ptr> articles;
        ArticleMgr::GetInstance()->listByUserId(articles, uid, true);

        int64_t total_articles = 0;
        int64_t published_articles = 0;
        int64_t private_articles = 0;
        int64_t total_views = 0;
        int64_t total_likes = 0;
        std::set<int64_t> unique_labels;
        std::vector<int64_t> published_ids;

        for (auto& a : articles) {
            if (a->getIsDeleted())
                continue;
            total_articles++;

            int32_t state = a->getState();
            if (state == ArticleManager::PUBLISHED) {
                published_articles++;
                published_ids.push_back(a->getId());
            } else if (state == ArticleManager::PRIVATE) {
                private_articles++;
            }

            total_views += a->getViews();
            total_likes += a->getPraise();

            std::vector<data::ArticleLabelRelInfo::ptr> labels;
            ArticleLabelRelMgr::GetInstance()->listByArticleId(labels, a->getId(), true);
            for (auto& l : labels) {
                unique_labels.insert(l->getLabelId());
            }
        }

        // total_organizations
        std::vector<data::OrganizationUserRelInfo::ptr> orgs;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(orgs, uid, OrganizationUserRelManager::APPROVED, true);
        int64_t total_organizations = orgs.size();

        result->setErrno(errcode::SUCCESS);
        result->set("total_articles", total_articles);
        result->set("published_articles", published_articles);
        result->set("private_articles", private_articles);
        result->set("total_views", total_views);
        result->set("total_likes", total_likes);
        // total_comments
        int64_t total_comments = 0;
        if (!published_ids.empty()) {
            auto db = GetDB();
            if (db) {
                auto qb = data::CommentInfoDao::newQuery();
                qb->whereIn("article_id", published_ids);
                qb->where("is_deleted", "=", (int64_t)0);
                qb->executeCount(total_comments, db);
            }
        }
        result->set("total_comments", total_comments);
        result->set("total_tags", (int64_t)unique_labels.size());
        result->set("total_organizations", total_organizations);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
} // namespace servlet
} // namespace blog
