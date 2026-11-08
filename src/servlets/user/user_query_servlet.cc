#include "user_query_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"
#include "blog/data/user_info.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserQueryServlet::UserQueryServlet()
    : BlogServlet("UserQueryServlet")
{
}

int32_t UserQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, user_ids, "user_ids");

        auto ids = chen::StringUtil::Split(user_ids, ',');
        std::vector<data::UserInfo::ptr> infos;

        for (auto& i : ids) {
            data::UserInfo::ptr info = UserMgr::GetInstance()->get(chen::TypeUtil::Atoi(i));
            if (info) {
                infos.push_back(info);
            }
        }

        Json::Value data;
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["name"] = i->getName();
            data.append(v);
        }
        result->set("data", data);
        INFO(logger) << "infos.size=" << infos.size() << " - " << chen::JsonUtil::ToString(data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
