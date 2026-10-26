#include "user_admin_lists_servlet.h"

#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/db/query_builder.h>
#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include "blog/data/article_info.h"
#include "blog/data/comment_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr g_logger = LOG_ROOT();

UserAdminListsServlet::UserAdminListsServlet()
    : BlogAdminServlet("UserAdminListsServlet")
{
}

int32_t UserAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t rrole = request->getParamAs<int32_t>("role", -1);
        int32_t state = request->getParamAs<int32_t>("state", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<data::UserInfo::ptr> users;
        uint64_t total = UserMgr::GetInstance()->listByPages(users, offset, page_size, rrole, state, days, true);

        // 批量查询文章数和评论数，避免逐用户查询
        std::map<int64_t, int64_t> article_counts, comment_counts;
        auto db = getDB();
        if (db && !users.empty()) {
            std::vector<int64_t> user_ids;
            for (auto& u : users) {
                user_ids.push_back(u->getId());
            }

            std::vector<std::pair<int64_t, int64_t>> rows;

            auto aqb = data::ArticleInfoDao::newQuery();
            aqb->select("user_id, COUNT(*) as cnt");
            aqb->whereIn("user_id", user_ids);
            aqb->where("is_deleted", "=", (int64_t)0);
            aqb->groupBy("user_id");
            if (aqb->queryPairs<int64_t, int64_t>(rows, db) == 0) {
                for (auto& [uid, cnt] : rows) {
                    article_counts[uid] = cnt;
                }
            }

            rows.clear();
            auto cqb = data::CommentInfoDao::newQuery();
            cqb->select("user_id, COUNT(*) as cnt");
            cqb->whereIn("user_id", user_ids);
            cqb->where("is_deleted", "=", (int64_t)0);
            cqb->groupBy("user_id");
            if (cqb->queryPairs<int64_t, int64_t>(rows, db) == 0) {
                for (auto& [uid, cnt] : rows) {
                    comment_counts[uid] = cnt;
                }
            }
        }

        result->set("total", total);
        auto& list = result->jsondata["list"];
        for (const auto& user : users) {
            Json::Value item;
            item["id"] = user->getId();
            item["name"] = user->getName();
            item["email"] = user->getEmail();
            item["avatar"] = user->getAvatar();
            item["role"] = user->getRole();
            item["state"] = user->getState();
            item["create_time"] = user->getCreateTime();
            item["login_time"] = user->getLoginTime();
            item["article_count"] = article_counts[user->getId()];
            item["comment_count"] = comment_counts[user->getId()];
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
}