#include "article_delete_servlet.h"

#include <chen/log/log.h>

#include "../../index.h"
#include "../../manager/article_manager.h"
#include "../../util.h"
#include "blog/data/article_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleDeleteServlet::ArticleDeleteServlet()
    : BlogLoginedServlet("ArticleDeleteServlet")
{
}

int32_t ArticleDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> art_ids;
        auto tmp = chen::StringUtil::Split(ids, ',');
        for (auto& i : tmp) {
            art_ids.insert(chen::TypeUtil::Atoi(i));
        }

        int64_t uid = getUserId(request);
        std::vector<data::ArticleInfo::ptr> infos;
        for (auto& id : art_ids) {
            auto info = ArticleMgr::GetInstance()->get(id);
            if (!info) {
                continue;
            }
            if (info->getUserId() != uid) {
                continue;
            }
            if (info->getIsDeleted()) {
                continue;
            }
            infos.push_back(info);
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
        for (auto& i : infos) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            data::ArticleInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(errcode::DB_COMMIT_FAILED);

            for (auto& i : infos) {
                i->setIsDeleted(0);
            }
            break;
        }
        if (!infos.empty()) {
            auto& jids = result->jsondata["ids"];
            for (auto& i : infos) {
                jids.append(i->getId());
                ArticleMgr::GetInstance()->del(i->getId());
                // 从搜索索引中移除
                IndexMgr::GetInstance()->removeArticle(i->getId());
                // 从定时发布调度中移除
                ArticleMgr::GetInstance()->unscheduleArticle(i->getId());
                // 删除的文章如果是已发布状态，清除对应月份的日历缓存
                if (i->getState() == ArticleManager::Status::PUBLISHED && i->getPublishTime() > 0) {
                    ArticleMgr::GetInstance()->clearCalendarCache(i->getUserId(), i->getPublishTime());
                }
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
