#include "article_publish_servlet.h"

#include <chen/log/log.h>

#include "../../event/event_define.h"
#include "../../search/search_sync.h"
#include "../../manager/article_manager.h"
#include "../../manager/user_manager.h"
#include "../../permission.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticlePublishServlet::ArticlePublishServlet()
    : BlogLoginedServlet("ArticlePublishServlet")
{
}

int32_t ArticlePublishServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, publish_time, "publish_time");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        data::ArticleInfo::ptr info = ArticleMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        if (info->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }
        if (info->getState() == ArticleManager::Status::PUBLISHED || info->getState() == ArticleManager::Status::CHECKING) {
            result->setErrno(errcode::ARTICLE_INVALID_STATE);
            break;
        }
        info->setState(ArticleManager::Status::CHECKING);
        time_t now = time(0);
        if (publish_time > now) {
            info->setPublishTime(publish_time);
        } else {
            info->setPublishTime(now);
        }
        info->setUpdateTime(now);

        // 检查是否可以跳过审核（管理员和审核员直接发布）
        auto author = UserMgr::GetInstance()->get(uid);
        bool bypass_review = false;
        if (author) {
            bypass_review = permission::CanBypassReview(author->getRole());
        }

        if (bypass_review) {
            // 跳过审核，直接发布或设为私密（定时发布）
            if (info->getPublishTime() <= now) {
                info->setState(ArticleManager::Status::PUBLISHED);
            } else {
                info->setState(ArticleManager::Status::PRIVATE);
            }
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }
        if (data::ArticleInfoDao::Update(info, db)) {
            result->setErrno(errcode::ARTICLE_UPDATE_FAILED);
            break;
        }
        ArticleMgr::GetInstance()->add(info);
        SearchSyncMgr::GetInstance()->markDirty(info->getId(), SearchSync::OP_UPSERT);

        // 清除对应月份的日历缓存
        ArticleMgr::GetInstance()->clearCalendarCache(info->getUserId(), info->getPublishTime());

        // 定时发布文章加入 Redis sorted set 调度
        if (info->getPublishTime() > now && info->getState() != ArticleManager::Status::PUBLISHED) {
            ArticleMgr::GetInstance()->scheduleArticle(id, info->getPublishTime());
        }

        // 未跳过审核时，通知审核员
        if (!bypass_review) {
            EventArticleReviewData data = {};
            data.type = "request";
            data.article_id = id;
            data.article_title = info->getTitle();
            data.author_id = uid;
            data.author_name = author ? author->getName() : std::to_string(uid);

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ARTICLE_REVIEW, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
