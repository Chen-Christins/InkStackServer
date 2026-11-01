#include "article_list_by_label_servlet.h"

#include <chen/log/log.h>

#include "../../manager/article_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleListByLabelServlet::ArticleListByLabelServlet()
    : BlogServlet("ArticleListByLabelServlet")
{
}

int32_t ArticleListByLabelServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, label_id, "label_id");
        int64_t page_from = request->getParamAs<int64_t>("page_from");
        int64_t page_size = request->getParamAs<int64_t>("page_size", 6);

        int offset = (page_from - 1) * page_size;
        std::vector<data::ArticleInfo::ptr> infos;
        auto total = ArticleMgr::GetInstance()->listByLabelPages(infos, label_id, offset, page_size, true);
        result->set("total", total);
        Json::Value list;
        for (auto& i : infos) {
            Json::Value item;
            auto uinfo = UserMgr::GetInstance()->get(i->getUserId());
            item["id"] = i->getId();
            item["author"] = uinfo ? uinfo->getName() : "";
            item["title"] = i->getTitle();
            item["summary"] = i->getContent().substr(0, 100);
            item["type"] = i->getType();
            item["state"] = i->getState();
            item["views"] = i->getViews();
            item["praise"] = i->getPraise();
            item["favorites"] = i->getFavorites();
            item["publish_time"] = i->getPublishTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
