#include "broadcast_list_servlet.h"

#include "../../util.h"
#include "blog/data/notification_info.h"

#include <chen/db/query_builder.h>
#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

BroadcastListServlet::BroadcastListServlet()
    : BlogServlet("BroadcastListServlet")
{
}

int32_t BroadcastListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t offset = request->getParamAs<int64_t>("offset", 0);
        int64_t size = request->getParamAs<int64_t>("size", 20);
        if (size > 100) {
            size = 100;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        // 查 is_broadcast=1 且未删除的，按创建时间倒序
        auto qb = data::NotificationInfoDao::newQuery();
        qb->where("is_broadcast", "=", (int64_t)1);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->orderBy("id", "DESC");
        qb->limit((int32_t)size);
        qb->offset((int32_t)offset);

        std::vector<data::NotificationInfo::ptr> notifs;
        if (data::NotificationInfoDao::QueryByBuilder(notifs, qb, db)) {
            ERROR(logger) << "QueryByBuilder failed";
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (auto& info : notifs) {
            if (!info) {
                continue;
            }
            Json::Value item;
            item["id"] = info->getId();
            item["title"] = info->getTitle();
            item["content"] = info->getContent();
            item["type"] = info->getType();
            item["level"] = info->getLevel();
            item["start_time"] = info->getStartTime();
            item["end_time"] = info->getEndTime();
            item["create_time"] = info->getCreateTime();
            list.append(item);
        }
        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
