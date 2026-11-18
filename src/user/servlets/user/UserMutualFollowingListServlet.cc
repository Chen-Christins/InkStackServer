#include "user/servlets/user/UserMutualFollowingListServlet.hpp"

#include <chen/util/string_util.h>

#include <algorithm>

#include "user/manager/UserFollowRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

UserMutualFollowingListServlet::UserMutualFollowingListServlet()
    : BlogLoginedServlet("UserMutualFollowingListServlet")
{
}

int32_t UserMutualFollowingListServlet::handle(
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

        std::string keyword = chen::StringUtil::Trim(request->getParam("keyword"));
        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);
        std::string kw_lower = chen::StringUtil::ToLower(keyword);

        std::vector<int64_t> mutual_ids;
        UserFollowRelMgr::GetInstance()->listMutualFollowing(mutual_ids, uid);

        std::vector<int64_t> filtered_ids;
        filtered_ids.reserve(mutual_ids.size());
        for (auto user_id : mutual_ids) {
            auto user = UserMgr::GetInstance()->get(user_id);
            if (!user || user->getIsDeleted()) {
                continue;
            }
            if (!kw_lower.empty()) {
                std::string name_lower = chen::StringUtil::ToLower(user->getName());
                std::string account_lower = chen::StringUtil::ToLower(user->getAccount());
                if (name_lower.find(kw_lower) == std::string::npos && account_lower.find(kw_lower) == std::string::npos) {
                    continue;
                }
            }
            filtered_ids.push_back(user_id);
        }

        int64_t total = (int64_t)filtered_ids.size();
        int32_t start = offset;
        int32_t end = std::min(start + size, (int32_t)filtered_ids.size());
        if (start < 0) {
            start = 0;
        }
        if (start >= (int32_t)filtered_ids.size()) {
            start = (int32_t)filtered_ids.size();
            end = start;
        }

        Json::Value arr(Json::arrayValue);
        for (int32_t i = start; i < end; ++i) {
            auto user = UserMgr::GetInstance()->get(filtered_ids[i]);
            if (!user) {
                continue;
            }
            Json::Value item;
            item["id"] = user->getId();
            item["name"] = user->getName();
            item["account"] = user->getAccount();
            item["avatar"] = user->getAvatar();
            arr.append(item);
        }

        result->setErrno(errcode::SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
        result->set("size", (int32_t)arr.size());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
}