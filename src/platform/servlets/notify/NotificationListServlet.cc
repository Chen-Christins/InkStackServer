#include "platform/servlets/notify/NotificationListServlet.hpp"

#include "platform/manager/NotificationManager.hpp"

namespace blog {
namespace servlet {

NotificationListServlet::NotificationListServlet()
    : BlogLoginedServlet("NotificationListServlet")
{
}

int32_t NotificationListServlet::handle(
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

        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);
        std::string type = request->getParam("type");

        std::vector<data::NotificationInfo::ptr> notifications;
        NotificationMgr::GetInstance()->listByUser(notifications, uid, offset, size, type);

        Json::Value arr(Json::arrayValue);
        for (auto& n : notifications) {
            Json::Value item;
            item["id"] = n->getId();
            item["title"] = n->getTitle();
            item["content"] = n->getContent();
            item["type"] = n->getType();

            const std::string& type = n->getType();
            int64_t target_id = n->getArticleId();
            if (type == "bug_assigned") {
                item["bug_id"] = target_id;
            } else if (type == "task_assigned") {
                item["task_id"] = target_id;
            } else if (type == "requirement_assigned") {
                item["requirement_id"] = target_id;
            } else if (type == "assignment_submitted" || type == "assignment_created") {
                item["assignment_id"] = target_id;
            } else if (type == "org_apply_approved") {
                item["org_id"] = target_id;
            } else if (type == "org_apply_request") {
                item["apply_id"] = target_id;
            } else {
                item["article_id"] = target_id;
                item["comment_id"] = n->getCommentId();
            }

            item["is_read"] = n->getIsRead();
            item["create_time"] = n->getCreateTime();
            arr.append(item);
        }

        int64_t total = NotificationMgr::GetInstance()->countByUser(uid, type);

        result->setErrno(errcode::SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
        result->set("size", (int32_t)notifications.size());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
} // namespace servlet
} // namespace blog
