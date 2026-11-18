#include "platform/servlets/notify/NotificationReadServlet.hpp"

#include "common/Util.hpp"

#include "platform/manager/NotificationManager.hpp"

#include <vector>

namespace blog {
namespace servlet {

NotificationReadServlet::NotificationReadServlet()
    : BlogLoginedServlet("NotificationReadServlet")
{
}

int32_t NotificationReadServlet::handle(
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

        // 支持两种格式: ?id=123 或 ?ids=1,2,3
        std::vector<int64_t> ids;
        std::string ids_str = request->getParam("ids");
        if (!ids_str.empty()) {
            for (auto& s : chen::StringUtil::Split(ids_str, ',')) {
                auto trimmed = chen::StringUtil::Trim(s);
                if (!trimmed.empty()) {
                    ids.push_back(chen::TypeUtil::Atoi(trimmed));
                }
            }
        } else {
            int64_t single_id = request->getParamAs<int64_t>("id", 0);
            if (!single_id) {
                result->setErrno(errcode::PARAM_MISSING, "id or ids is required");
                break;
            }
            ids.push_back(single_id);
        }

        NotificationMgr::GetInstance()->markRead(ids);

        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
} // namespace servlet
} // namespace blog
