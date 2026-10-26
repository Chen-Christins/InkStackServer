#include "article_update_servlet.h"

#include <chen/log/log.h>

#include <set>

#include "../../index.h"
#include "../../manager/article_category_rel_manager.h"
#include "../../manager/article_label_rel_manager.h"
#include "../../manager/article_manager.h"
#include "../../manager/category_manager.h"
#include "../../manager/label_manager.h"
#include "../../permission.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleUpdateServlet::ArticleUpdateServlet()
    : BlogLoginedServlet("ArticleUpdateServlet")
{
}

int32_t ArticleUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_STRING(result, title, "title");
        DEFINE_AND_CHECK_STRING(result, content, "content");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::ArticleInfo::ptr info = ArticleMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::ARTICLE_NOT_FOUND);
            break;
        }
        if (uid != info->getUserId()) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        int32_t old_state = info->getState();
        info->setTitle(title);
        info->setContent(content);

        if (updateOptionalFields(request, info, result)) {
            break;
        }

        // 检查文章作者是否可以跳过审核（管理员和审核员编辑后直接发布）
        bool bypass_review = false;
        auto article_author = UserMgr::GetInstance()->get(info->getUserId());
        if (article_author) {
            bypass_review = permission::CanBypassReview(article_author->getRole());
        }

        if (old_state == ArticleManager::Status::PUBLISHED) {
            // 可以跳过审核时，保持已发布状态不变
            if (!bypass_review) {
                info->setState(ArticleManager::Status::CHECKING);
            }
        } else if (old_state == ArticleManager::Status::REJECTED) {
            info->setState(ArticleManager::Status::PRIVATE);
        }
        info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }

        if (data::ArticleInfoDao::Update(info, db)) {
            ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            result->setErrno(errcode::ARTICLE_UPDATE_FAILED);
            break;
        }

        time_t now = chen::TimeUtil::GetCurrentSec();

        if (syncCategoryRels(request, id, db, now, result)) {
            break;
        }
        if (syncLabelRels(request, id, db, now, result)) {
            break;
        }

        if (!trans->commit()) {
            result->setErrno(errcode::DB_COMMIT_FAILED);
            break;
        }

        ArticleMgr::GetInstance()->add(info);

        // 更新定时发布调度：先移除旧的，再按需添加新的
        ArticleMgr::GetInstance()->unscheduleArticle(id);
        time_t now_publish = chen::TimeUtil::GetCurrentSec();
        int32_t new_state = info->getState();
        if ((new_state == ArticleManager::Status::CHECKING || new_state == ArticleManager::Status::PRIVATE) && info->getPublishTime() > now_publish) {
            ArticleMgr::GetInstance()->scheduleArticle(id, info->getPublishTime());
        }

        IndexMgr::GetInstance()->updateArticle(info);

        if (old_state == ArticleManager::Status::PUBLISHED && info->getPublishTime() > 0) {
            ArticleMgr::GetInstance()->clearCalendarCache(info->getUserId(), info->getPublishTime());
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

int32_t ArticleUpdateServlet::updateOptionalFields(chen::http::HttpRequest::ptr request, std::shared_ptr<data::ArticleInfo> info, Result::ptr result)
{
    std::string type_str = request->getParam("type");
    if (!type_str.empty()) {
        int32_t type = chen::TypeUtil::Atoi(type_str);
        if (type != ArticleManager::Type::ORIGINAL && type != ArticleManager::Type::REPRINT) {
            result->setErrno(errcode::ARTICLE_INVALID_TYPE);
            return -1;
        }
        info->setType(type);
    }

    std::string channel_str = request->getParam("channel");
    if (!channel_str.empty()) {
        int64_t channel = chen::TypeUtil::Atoi(channel_str);
        info->setChannel(channel);
    }

    return 0;
}

int32_t ArticleUpdateServlet::syncCategoryRels(chen::http::HttpRequest::ptr request, int64_t id, chen::IDB::ptr db, time_t now, Result::ptr result)
{
    std::string category = request->getParam("category");
    if (category.empty()) {
        return 0;
    }

    std::vector<data::ArticleCategoryRelInfo::ptr> old_rels;
    ArticleCategoryRelMgr::GetInstance()->listByArticleId(old_rels, id, true);

    int64_t uid = getUserId(request);
    if (!uid) {
        return 0;
    }

    std::set<int64_t> new_cat_ids;
    auto cat_id_strs = chen::StringUtil::Split(category, ',');
    for (auto& s : cat_id_strs) {
        int64_t cid = chen::TypeUtil::Atoi(s);
        auto cinfo = CategoryMgr::GetInstance()->get(cid);
        if (cinfo && cinfo->getUserId() == uid) {
            new_cat_ids.insert(cid);
        }
    }

    std::set<int64_t> old_cat_ids;
    for (auto& rel : old_rels) {
        old_cat_ids.insert(rel->getCategoryId());
    }

    for (int64_t cid : new_cat_ids) {
        if (old_cat_ids.find(cid) == old_cat_ids.end()) {
            auto existing = ArticleCategoryRelMgr::GetInstance()->getByArticleIdCategoryId(id, cid);
            if (existing && existing->getIsDeleted()) {
                existing->setIsDeleted(0);
                existing->setUpdateTime(now);
                if (data::ArticleCategoryRelInfoDao::Update(existing, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            } else if (!existing) {
                data::ArticleCategoryRelInfo::ptr rel(new data::ArticleCategoryRelInfo);
                rel->setArticleId(id);
                rel->setCategoryId(cid);
                rel->setUpdateTime(now);
                if (data::ArticleCategoryRelInfoDao::Insert(rel, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }
    }

    for (int64_t cid : old_cat_ids) {
        if (new_cat_ids.find(cid) == new_cat_ids.end()) {
            auto existing = ArticleCategoryRelMgr::GetInstance()->getByArticleIdCategoryId(id, cid);
            if (existing && !existing->getIsDeleted()) {
                existing->setIsDeleted(1);
                existing->setUpdateTime(now);
                if (data::ArticleCategoryRelInfoDao::Update(existing, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }
    }

    return 0;
}

int32_t ArticleUpdateServlet::syncLabelRels(chen::http::HttpRequest::ptr request, int64_t id, chen::IDB::ptr db, time_t now, Result::ptr result)
{
    std::string label = request->getParam("label");
    if (label.empty()) {
        return 0;
    }

    std::vector<data::ArticleLabelRelInfo::ptr> old_rels;
    ArticleLabelRelMgr::GetInstance()->listByArticleId(old_rels, id, true);

    std::set<int64_t> new_label_ids;
    auto label_id_strs = chen::StringUtil::Split(label, ',');
    for (auto& s : label_id_strs) {
        int64_t lid = chen::TypeUtil::Atoi(s);
        auto linfo = LabelMgr::GetInstance()->get(lid);
        if (linfo) {
            new_label_ids.insert(lid);
        }
    }

    std::set<int64_t> old_label_ids;
    for (auto& rel : old_rels) {
        old_label_ids.insert(rel->getLabelId());
    }

    for (int64_t lid : new_label_ids) {
        if (old_label_ids.find(lid) == old_label_ids.end()) {
            auto existing = ArticleLabelRelMgr::GetInstance()->getByArticleIdLabelId(id, lid);
            if (existing && existing->getIsDeleted()) {
                existing->setIsDeleted(0);
                existing->setUpdateTime(now);
                if (data::ArticleLabelRelInfoDao::Update(existing, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            } else if (!existing) {
                data::ArticleLabelRelInfo::ptr rel(new data::ArticleLabelRelInfo);
                rel->setArticleId(id);
                rel->setLabelId(lid);
                rel->setUpdateTime(now);
                if (data::ArticleLabelRelInfoDao::Insert(rel, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }
    }

    for (int64_t lid : old_label_ids) {
        if (new_label_ids.find(lid) == new_label_ids.end()) {
            auto existing = ArticleLabelRelMgr::GetInstance()->getByArticleIdLabelId(id, lid);
            if (existing && !existing->getIsDeleted()) {
                existing->setIsDeleted(1);
                existing->setUpdateTime(now);
                if (data::ArticleLabelRelInfoDao::Update(existing, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }
    }

    return 0;
}

} // namespace servlet
} // namespace blog
