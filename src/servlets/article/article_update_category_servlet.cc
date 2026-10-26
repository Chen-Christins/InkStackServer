#include "article_update_category_servlet.h"

#include <chen/log/log.h>

#include "../../index.h"
#include "../../manager/article_category_rel_manager.h"
#include "../../manager/article_manager.h"
#include "../../manager/category_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleUpdateCategoryServlet::ArticleUpdateCategoryServlet()
    : BlogLoginedServlet("ArticleUpdateCategoryServlet")
{
}

int32_t ArticleUpdateCategoryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        std::string adds = request->getParam("add_category_ids");
        std::string dels = request->getParam("del_category_ids");

        if (adds.empty() && dels.empty()) {
            result->setErrno(errcode::ARTICLE_CATEGORY_EMPTY);
            break;
        }

        auto ainfo = ArticleMgr::GetInstance()->get(id);
        if (!ainfo) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        if (ainfo->getUserId() != uid) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        std::vector<std::string> tmps;
        if (!adds.empty()) {
            tmps = chen::StringUtil::Split(adds, ',');
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }

        time_t now = time(0);
        std::vector<data::ArticleCategoryRelInfo::ptr> new_infos;
        std::vector<data::ArticleCategoryRelInfo::ptr> update_add_infos;
        std::vector<data::ArticleCategoryRelInfo::ptr> update_del_infos;

        for (auto& i : tmps) {
            int64_t cid = chen::TypeUtil::Atoi(i);
            auto cinfo = CategoryMgr::GetInstance()->get(cid);
            if (!cinfo || cinfo->getUserId() != uid) {
                WARN(logger) << "skip category not owned by user, user_id=" << uid << " category_id=" << cid;
                continue;
            }
            auto acinfo = ArticleCategoryRelMgr::GetInstance()->getByArticleIdCategoryId(id, cid);
            if (acinfo) {
                if (acinfo->getIsDeleted()) {
                    acinfo->setIsDeleted(0);
                    acinfo->setUpdateTime(now);
                    update_add_infos.push_back(acinfo);
                    if (data::ArticleCategoryRelInfoDao::Update(acinfo, db)) {
                        ERROR(logger) << "db error errno=" << db->getErrno() << ", errstr=" << db->getErrStr();
                    }
                }
            } else {
                acinfo.reset(new data::ArticleCategoryRelInfo);
                acinfo->setArticleId(id);
                acinfo->setCategoryId(cid);
                acinfo->setUpdateTime(now);
                new_infos.push_back(acinfo);
                if (data::ArticleCategoryRelInfoDao::Insert(acinfo, db)) {
                    ERROR(logger) << "db error errno=" << db->getErrno() << ", errstr=" << db->getErrStr();
                }
            }
        }
        tmps.clear();
        if (!dels.empty()) {
            tmps = chen::StringUtil::Split(dels, ',');
        }
        for (auto& i : tmps) {
            int64_t cid = chen::TypeUtil::Atoi(i);
            auto cinfo = CategoryMgr::GetInstance()->get(cid);
            if (!cinfo || cinfo->getUserId() != uid) {
                WARN(logger) << "skip category not owned by user, user_id=" << uid << " category_id=" << cid;
                continue;
            }
            auto acinfo = ArticleCategoryRelMgr::GetInstance()->getByArticleIdCategoryId(id, cid);
            if (acinfo) {
                if (acinfo->getIsDeleted() == 0) {
                    acinfo->setIsDeleted(1);
                    acinfo->setUpdateTime(now);
                    new_infos.push_back(acinfo);
                    update_del_infos.push_back(acinfo);
                    if (data::ArticleCategoryRelInfoDao::Update(acinfo, db)) {
                        ERROR(logger) << "db error errno=" << db->getErrno() << ", errstr=" << db->getErrStr();
                    }
                }
            }
        }

        if (!trans->commit()) {
            for (auto& i : update_add_infos) {
                i->setIsDeleted(1);
            }
            for (auto& i : update_del_infos) {
                i->setIsDeleted(0);
            }
            result->setErrno(errcode::DB_COMMIT_FAILED);
            break;
        }

        // 分类关联已变化，同步刷新搜索索引中的 CAT_ID，避免按分类筛选结果滞后
        IndexMgr::GetInstance()->updateArticle(ainfo);

        if (!update_add_infos.empty()) {
            auto& v = result->jsondata["add_category_ids"];
            for (auto& i : update_add_infos) {
                v.append(i->getId());
            }
        }
        if (!new_infos.empty()) {
            auto& v = result->jsondata["add_category_ids"];
            for (auto& i : new_infos) {
                v.append(i->getId());
            }
        }
        if (!update_del_infos.empty()) {
            auto& v = result->jsondata["del_category_ids"];
            for (auto& i : update_del_infos) {
                v.append(i->getId());
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
