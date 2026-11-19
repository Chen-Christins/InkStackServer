#include "article_search_servlet.h"

#include <chen/log/log.h>

#include "../../manager/article_manager.h"
#include "../../manager/user_manager.h"
#include "../../search/search_sync.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleSearchServlet::ArticleSearchServlet()
    : BlogServlet("ArticleSearchServlet")
{
}

int32_t ArticleSearchServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string keyword = request->getParam("keyword");

        int64_t category_id = request->getParamAs<int64_t>("category_id");
        int64_t label_id = request->getParamAs<int64_t>("label_id");
        int64_t channel = request->getParamAs<int64_t>("channel");
        int64_t user_id = request->getParamAs<int64_t>("user_id");
        std::string year_month = request->getParam("year_month");

        int64_t page = request->getParamAs<int64_t>("page", 1);
        int64_t page_size = request->getParamAs<int64_t>("page_size", 20);
        if (page < 1) {
            page = 1;
        }
        if (page_size < 1) {
            page_size = 20;
        }
        if (page_size > 100) {
            page_size = 100;
        }

        tagSearchQuery query;
        query.Keyword = keyword;
        if (category_id > 0) {
            query.CategoryIds.push_back(static_cast<uint64_t>(category_id));
        }
        if (label_id > 0) {
            query.LabelIds.push_back(static_cast<uint64_t>(label_id));
        }
        // 公共搜索接口只允许查询已发布文章，避免私密/审核中文章被检索
        query.State = ArticleManager::Status::PUBLISHED;
        query.Channel = channel;
        query.UserId = user_id;
        query.YearMonth = year_month;
        query.Page = static_cast<int32_t>(page);
        query.PageSize = static_cast<int32_t>(page_size);

        tagSearchResult search_result;
        if (!SearchSyncMgr::GetInstance()->query(query, search_result)) {
            WARN(logger) << "article search unavailable, keyword=" << keyword;
            result->setErrno(errcode::ARTICLE_SEARCH_UNAVAILABLE);
            result->set("total", 0);
            result->set("list", Json::Value(Json::arrayValue));
            break;
        }

        result->set("total", search_result.Total);

        Json::Value list(Json::arrayValue);
        for (auto id : search_result.Ids) {
            auto info = ArticleMgr::GetInstance()->get(static_cast<int64_t>(id));
            if (!info || info->getIsDeleted()) {
                continue;
            }
            Json::Value item;
            auto uinfo = UserMgr::GetInstance()->get(info->getUserId());
            item["id"] = info->getId();
            item["author"] = uinfo ? uinfo->getName() : "";
            item["title"] = info->getTitle();
            item["summary"] = info->getContent().substr(0, 100);
            item["type"] = info->getType();
            item["state"] = info->getState();
            item["views"] = info->getViews();
            item["praise"] = info->getPraise();
            item["favorites"] = info->getFavorites();
            item["publish_time"] = info->getPublishTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
