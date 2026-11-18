#include "article/servlets/article/ArticlePraiseServlet.hpp"

#include "common/event/EventDefine.hpp"
#include "article/manager/ArticleManager.hpp"
#include "article/manager/ArticlePraiseRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

#include <json/json.h>

namespace blog {
namespace servlet {

ArticlePraiseServlet::ArticlePraiseServlet()
    : BlogLoginedServlet("ArticlePraiseServlet")
{
}

int32_t ArticlePraiseServlet::handle(
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

        // check article exists
        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article || article->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_NOT_FOUND);
            break;
        }

        bool already_praising = ArticlePraiseRelMgr::GetInstance()->isPraising(uid, article_id);

        if (already_praising) {
            // unlike
            if (!ArticlePraiseRelMgr::GetInstance()->unpraise(uid, article_id)) {
                result->setErrno(errcode::ARTICLE_UNPRAISE_FAILED);
                break;
            }
            ArticleMgr::GetInstance()->decPraiseCount(article_id);
            result->set("is_praising", false);
        } else {
            // like
            auto info = ArticlePraiseRelMgr::GetInstance()->praise(uid, article_id);
            if (!info) {
                result->setErrno(errcode::ARTICLE_PRAISE_FAILED);
                break;
            }
            ArticleMgr::GetInstance()->incPraiseCount(article_id);
            result->set("is_praising", true);

            // Notify article author (not self-praise)
            if (article->getUserId() != uid) {
                auto liker_info = UserMgr::GetInstance()->get(uid);
                // 文章点赞事件
                {
                    EventArticlePraiseData data = {};
                    data.author_id = article->getUserId();
                    data.liker_id = uid;
                    data.liker_name = liker_info ? liker_info->getName() : "someone";
                    data.article_id = article_id;
                    data.article_title = article->getTitle();

                    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ARTICLE_PRAISE, std::move(data));
                }
            }
        }

        result->set("praise_count", (int64_t)article->getPraise());
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
