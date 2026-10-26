#include "user_device_list_servlet.h"

#include "../../auth/session_store.h"
#include "../../util/ua_parser.h"

namespace blog {
namespace servlet {

UserDeviceListServlet::UserDeviceListServlet()
    : BlogLoginedServlet("UserDeviceListServlet")
{
}

int32_t UserDeviceListServlet::handle(
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

        std::string current_hash;
        std::string current_session = request->getCookie(CookieKey::SESSION_KEY);
        if (!current_session.empty()) {
            current_hash = SessionStore::HashId(current_session);
        }

        std::vector<SessionInfo> sessions;
        SessionStore::List(uid, sessions);

        result->setErrno(errcode::SUCCESS);
        result->set("total", sessions.size());
        auto& list = result->jsondata["list"];
        for (const auto& s : sessions) {
            Json::Value item;
            item["id"] = s.id;
            item["device_id"] = s.id;
            item["device_name"] = ParseDeviceName(s.user_agent);
            item["platform"] = DetectPlatform(s.user_agent);
            item["ip"] = s.ip;
            item["is_active"] = 1;
            item["is_current"] = (!current_hash.empty() && current_hash == s.id);
            item["login_time"] = s.created_at;
            item["last_active_time"] = s.last_seen;
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
