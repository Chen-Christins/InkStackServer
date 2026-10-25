#include "article_verify_servlet.h"

#include <chen/iomanager/iomanager.h>
#include <chen/log/log.h>
#include <json/json.h>

#include "../../event/event_define.h"
#include "../../index.h"
#include "../../manager/article_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleVerifyServlet::ArticleVerifyServlet()
    : BlogLoginedServlet("ArticleVerifyServlet")
{
}

int32_t ArticleVerifyServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, state, "state");

        // 传入参数state，决定文章的去留
        if (state != ArticleManager::Status::PUBLISHED && state != ArticleManager::Status::REJECTED) {
            result->setErrno(errcode::ARTICLE_INVALID_STATE);
            break;
        }

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
        if (role != UserManager::Role::ADMIN && role != UserManager::Role::CHECKER) {
            result->setErrno(errcode::ACCESS_DENIED);
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

        if (info->getState() != ArticleManager::Status::CHECKING) {
            result->setErrno(errcode::ARTICLE_INVALID_STATE);
            break;
        }

        if (state == ArticleManager::Status::REJECTED) {
            info->setState(state);
        } else if (state == ArticleManager::Status::PUBLISHED) {
            if (info->getPublishTime() <= time(0)) {
                info->setState(ArticleManager::Status::PUBLISHED);
            } else {
                info->setState(ArticleManager::Status::PRIVATE);
            }
        }
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        if (data::ArticleInfoDao::Update(info, db)) {
            result->setErrno(errcode::ARTICLE_UPDATE_FAILED);
            info->setState(ArticleManager::Status::CHECKING);

            ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        ArticleMgr::GetInstance()->add(info);

        // 审核通过或拒绝后，从定时发布调度中移除
        ArticleMgr::GetInstance()->unscheduleArticle(id);

        IndexMgr::GetInstance()->updateArticle(info);

        // Notify author about review result + mark other admins' notifications as read
        {
            bool approved = (state == ArticleManager::Status::PUBLISHED || info->getState() == ArticleManager::Status::PUBLISHED);
            EventArticleReviewData data = {};
            data.type = approved ? "approved" : "rejected";
            data.article_id = id;
            data.article_title = info->getTitle();
            data.author_id = info->getUserId();
            data.reviewer_id = uid;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ARTICLE_REVIEW, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
