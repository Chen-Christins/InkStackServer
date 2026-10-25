#include "category_query_servlet.h"

#include "../../manager/article_category_rel_manager.h"
#include "../../manager/article_manager.h"
#include "../../manager/category_manager.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

CategoryQueryServlet::CategoryQueryServlet()
    : BlogAdminServlet("CategoryQueryServlet")
{
}

int32_t CategoryQueryServlet::handle(
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

        std::vector<blog::data::CategoryInfo::ptr> infos;
        CategoryMgr::GetInstance()->listAll(infos, true);

        result->set("total", infos.size());
        auto& list = result->jsondata["list"];
        for (const auto& cat : infos) {
            Json::Value item;
            item["id"] = cat->getId();
            item["name"] = cat->getName();
            item["url"] = cat->getUrl();
            item["icon"] = cat->getIcon();
            item["color"] = cat->getColor();
            item["description"] = cat->getDescription();
            item["parent_id"] = cat->getParentId();
            item["status"] = cat->getStatus();
            item["create_time"] = cat->getCreateTime();
            item["update_time"] = cat->getUpdateTime();

            int64_t article_count = 0;
            int64_t view_count = 0;
            std::vector<blog::data::ArticleCategoryRelInfo::ptr> rels;
            ArticleCategoryRelMgr::GetInstance()->listByCategoryId(rels, cat->getId(), true);
            for (const auto& rel : rels) {
                auto article = ArticleMgr::GetInstance()->get(rel->getArticleId());
                if (article) {
                    ++article_count;
                    view_count += article->getViews();
                }
            }
            item["article_count"] = article_count;
            item["view_count"] = view_count;

            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
