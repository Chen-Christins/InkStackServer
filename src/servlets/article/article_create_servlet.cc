#include "article_create_servlet.h"

#include <chen/log/log.h>

#include "../../manager/article_category_rel_manager.h"
#include "../../manager/article_label_rel_manager.h"
#include "../../manager/article_manager.h"
#include "../../manager/category_manager.h"
#include "../../manager/label_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleCreateServlet::ArticleCreateServlet()
    : BlogLoginedServlet("ArticleCreateServlet")
{
}

int32_t ArticleCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, title, "title");
        DEFINE_AND_CHECK_STRING(result, content, "content");
        DEFINE_AND_CHECK_TYPE(result, int32_t, type, "type");
        std::string category = request->getParam("category");
        std::string label = request->getParam("label");

        if (type != ArticleManager::Type::ORIGINAL && type != ArticleManager::Type::REPRINT) {
            result->setErrno(errcode::ARTICLE_INVALID_TYPE);
            break;
        }

        int64_t uid = getUserId(request);
        INFO(logger) << "uid=" << uid;
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        data::ArticleInfo::ptr info(new data::ArticleInfo);
        info->setTitle(title);
        info->setContent(content);
        info->setType(type);
        info->setUserId(uid);
        info->setState(ArticleManager::Status::PRIVATE);
        info->setCreateTime(time(0));
        info->setUpdateTime(time(0));

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

        if (data::ArticleInfoDao::Insert(info, db)) {
            ERROR(logger) << "insert article fail title=" << title << " user_id=" << uid << " errno=" << db->getErrno()
                          << " errstr=" << db->getErrStr();
            result->setErrno(errcode::ARTICLE_INSERT_FAILED);
            break;
        }
        result->set("id", info->getId());

        // 处理分类关联
        time_t now = time(0);
        if (!category.empty()) {
            auto cat_ids = chen::StringUtil::Split(category, ',');
            for (auto& s : cat_ids) {
                int64_t cid = chen::TypeUtil::Atoi(s);
                auto cinfo = CategoryMgr::GetInstance()->get(cid);
                if (!cinfo || cinfo->getUserId() != uid) {
                    WARN(logger) << "skip category not owned by user, user_id=" << uid << " category_id=" << cid;
                    continue;
                }
                data::ArticleCategoryRelInfo::ptr rel(new data::ArticleCategoryRelInfo);
                rel->setArticleId(info->getId());
                rel->setCategoryId(cid);
                rel->setUpdateTime(now);
                if (data::ArticleCategoryRelInfoDao::Insert(rel, db)) {
                    ERROR(logger) << "insert article_category_rel fail article_id=" << info->getId() << " category_id=" << cid
                                  << " errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }

        // 处理标签关联
        if (!label.empty()) {
            auto label_ids = chen::StringUtil::Split(label, ',');
            for (auto& s : label_ids) {
                int64_t lid = chen::TypeUtil::Atoi(s);
                auto linfo = LabelMgr::GetInstance()->get(lid);
                if (!linfo) {
                    continue;
                }
                data::ArticleLabelRelInfo::ptr rel(new data::ArticleLabelRelInfo);
                rel->setArticleId(info->getId());
                rel->setLabelId(lid);
                rel->setUpdateTime(now);
                if (data::ArticleLabelRelInfoDao::Insert(rel, db)) {
                    ERROR(logger) << "insert article_label_rel fail article_id=" << info->getId() << " label_id=" << lid
                                  << " errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                }
            }
        }

        if (!trans->commit()) {
            result->setErrno(errcode::DB_COMMIT_FAILED);
            break;
        }

        ArticleMgr::GetInstance()->add(info);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
