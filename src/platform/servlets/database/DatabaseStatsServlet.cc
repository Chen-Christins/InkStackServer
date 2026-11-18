#include "platform/servlets/database/DatabaseStatsServlet.hpp"

#include "article/manager/ArticleManager.hpp"
#include "platform/manager/BackupRecordManager.hpp"
#include "article/manager/CategoryManager.hpp"
#include "comment/manager/CommentManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"
#include "blog/data/label_info.h"

namespace blog {
namespace servlet {

DatabaseStatsServlet::DatabaseStatsServlet()
    : BlogLoginedServlet("DatabaseStatsServlet")
{
}

int32_t DatabaseStatsServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto astats = ArticleMgr::GetInstance()->getStats(0, -1, 0, "");
        auto cstats = CommentMgr::GetInstance()->getStats();

        int64_t total_users = 0;
        std::vector<int64_t> uids;
        UserMgr::GetInstance()->getAllIds(uids, true);
        total_users = uids.size();

        int64_t total_categories = 0;
        std::vector<data::CategoryInfo::ptr> categories;
        CategoryMgr::GetInstance()->listAll(categories, true);
        total_categories = categories.size();

        int64_t total_labels = 0;
        auto db = GetDB();
        if (db) {
            auto qb = data::LabelInfoDao::newQuery();
            qb->where("is_deleted", "=", (int64_t)0);
            qb->executeCount(total_labels, db);
        }

        int64_t total_backups = 0;
        std::vector<data::BackupRecordInfo::ptr> backups;
        BackupRecordMgr::GetInstance()->list(backups, total_backups, "", "", "", 0, 1);

        auto du = GetDiskUsage();

        result->set("total_size", du.totalSize);
        result->set("used_size", du.usedSize);
        result->set("available_size", du.availableSize);
        result->set("total_records", astats.total + total_users + cstats.total + total_categories + total_labels);
        result->set("articles", astats.total);
        result->set("users", total_users);
        result->set("comments", cstats.total);
        result->set("categories", total_categories);
        result->set("tags", total_labels);
        result->set("backups", total_backups);

        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
