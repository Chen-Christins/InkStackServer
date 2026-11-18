#include "article/servlets/article/ArticleDetailServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "article/manager/ArticleCategoryRelManager.hpp"
#include "article/manager/ArticleLabelRelManager.hpp"
#include "article/manager/ArticleManager.hpp"
#include "article/manager/CategoryManager.hpp"
#include "article/manager/LabelManager.hpp"
#include "user/manager/UserFollowRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleDetailServlet::ArticleDetailServlet()
    : BlogServlet("ArticleDetailServlet")
{
}

int32_t ArticleDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        auto type = request->getParamAs<uint32_t>("type");

        int64_t cur_uid = getUserId(request);
        if (!cur_uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        data::ArticleInfo::ptr info = ArticleMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }
        int32_t state = info->getState();
        bool is_deleted = info->getIsDeleted();
        auto current_user = UserMgr::GetInstance()->get(cur_uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        bool is_author = (cur_uid == info->getUserId());
        if (is_deleted) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }
        if (state != ArticleManager::Status::PUBLISHED && role != UserManager::Role::ADMIN && !is_author) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }
        int64_t uid = info->getUserId();
        if (type == 1 && cur_uid != uid) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }
        auto author_info = UserMgr::GetInstance()->get(uid);
        result->set("id", info->getId());
        result->set("author", author_info->getName());
        result->set("author_avatar", author_info->getAvatar());

        // author stats
        std::vector<data::ArticleInfo::ptr> authorArticles;
        ArticleMgr::GetInstance()->listByUserId(authorArticles, uid, true);
        int64_t author_praise_total = 0;
        int64_t author_article_count = 0;
        for (auto& a : authorArticles) {
            if (a->getState() == ArticleManager::Status::PUBLISHED) {
                author_article_count++;
                author_praise_total += a->getPraise();
            }
        }
        result->set("author_article_count", author_article_count);
        result->set("author_praise_count", author_praise_total);
        result->set("author_follower_count", UserFollowRelMgr::GetInstance()->countFollowers(uid));
        result->set("title", info->getTitle());
        result->set("content", info->getContent());
        result->set("user_id", uid);
        result->set("type", info->getType());
        result->set("publish_time", info->getPublishTime());
        result->set("update_time", info->getUpdateTime());
        result->set("state", info->getState());
        result->set("is_deleted", info->getIsDeleted());
        result->set("views", info->getViews());
        result->set("praise", info->getPraise());
        result->set("favorites", info->getFavorites());

        std::vector<data::ArticleCategoryRelInfo::ptr> cinfos;
        ArticleCategoryRelMgr::GetInstance()->listByArticleId(cinfos, id, true);

        for (auto& i : cinfos) {
            auto c = CategoryMgr::GetInstance()->get(i->getCategoryId());
            if (c && c->getIsDeleted() == 0) {
                Json::Value item;
                item["id"] = c->getId();
                item["name"] = c->getName();
                item["color"] = c->getColor();
                result->append("categorys", item);
            }
        }

        std::vector<data::ArticleLabelRelInfo::ptr> linfos;
        ArticleLabelRelMgr::GetInstance()->listByArticleId(linfos, id, true);
        for (auto& i : linfos) {
            auto l = LabelMgr::GetInstance()->get(i->getLabelId());
            if (l && l->getIsDeleted() == 0) {
                Json::Value item;
                item["id"] = l->getId();
                item["name"] = l->getName();
                item["color"] = l->getColor();
                result->append("labels", item);
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
