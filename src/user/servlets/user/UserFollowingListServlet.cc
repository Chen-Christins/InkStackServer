#include "user/servlets/user/UserFollowingListServlet.hpp"

#include "user/manager/UserFollowRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

UserFollowingListServlet::UserFollowingListServlet()
    : BlogLoginedServlet("UserFollowingListServlet")
{
}

int32_t UserFollowingListServlet::handle(
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

        int64_t target_uid = request->getParamAs<int64_t>("user_id", uid);
        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);

        std::vector<data::UserFollowRelInfo::ptr> rels;
        UserFollowRelMgr::GetInstance()->listFollowing(rels, target_uid, offset, size);

        Json::Value arr(Json::arrayValue);
        for (auto& rel : rels) {
            auto user = UserMgr::GetInstance()->get(rel->getFollowingId());
            if (!user || user->getIsDeleted()) {
                continue;
            }
            int64_t following_id = user->getId();
            Json::Value item;
            item["id"] = following_id;
            item["name"] = user->getName();
            item["account"] = user->getAccount();
            item["avatar"] = user->getAvatar();
            item["bio"] = user->getBio();
            item["following_count"] = UserFollowRelMgr::GetInstance()->countFollowing(following_id);
            item["follower_count"] = UserFollowRelMgr::GetInstance()->countFollowers(following_id);
            item["create_time"] = rel->getCreateTime();
            arr.append(item);
        }

        int64_t total = UserFollowRelMgr::GetInstance()->countFollowing(target_uid);

        result->setErrno(errcode::SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
