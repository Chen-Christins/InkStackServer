#include "admin_feedback_list_servlet.h"

#include "../../include/managers.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackListServlet::AdminFeedbackListServlet()
    : BlogLoginedServlet("AdminFeedbackListServlet")
{
}

int32_t AdminFeedbackListServlet::handle(
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

        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        std::string type = request->getParam("type");
        int32_t page = request->getParamAs<int32_t>("page", 1);
        int32_t page_size = request->getParamAs<int32_t>("page_size", 20);

        std::vector<data::UserFeedbackInfo::ptr> infos;
        int64_t total = 0;

        if (!FeedbackMgr::GetInstance()->list(infos, type, page, page_size, &total)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "query feedbacks failed");
            break;
        }

        Json::Value list(Json::arrayValue);
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = std::to_string(i->getId());
            v["type"] = i->getType();
            v["content"] = i->getContent();
            v["contact"] = i->getContact();
            v["created_at"] = static_cast<int64_t>(i->getCreateTime() * 1000);
            list.append(v);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackListServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
