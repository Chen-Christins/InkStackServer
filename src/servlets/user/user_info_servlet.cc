#include "user_info_servlet.h"

#include "../../manager/user_follow_rel_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

UserInfoServlet::UserInfoServlet()
    : BlogServlet("UserInfoServlet")
{
}

int32_t UserInfoServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t viewer_uid = getUserId(request);
        if (!viewer_uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        int64_t target_uid = viewer_uid;
        int64_t id = request->getParamAs<int64_t>("user_id", -1);
        if (id != -1) {
            target_uid = id;
        }

        data::UserInfo::ptr info = UserMgr::GetInstance()->get(target_uid);
        if (!info) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        result->setErrno(errcode::SUCCESS);
        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("account", info->getAccount());
        result->set("avatar", info->getAvatar());

        if (target_uid == viewer_uid) {
            result->set("email", info->getEmail());
        } else {
            auto viewer = UserMgr::GetInstance()->get(viewer_uid);
            if (viewer && viewer->getRole() == UserManager::Role::ADMIN) {
                result->set("email", info->getEmail());
            }
        }

        result->set("role", info->getRole());
        result->set("bio", info->getBio());
        result->set("website", info->getWebsite());
        result->set("location", info->getLocation());
        result->set("status", info->getIsDeleted());
        result->set("github", info->getGithub());
        result->set("login_time", info->getLoginTime());
        result->set("create_time", info->getCreateTime());

        int64_t following = UserFollowRelMgr::GetInstance()->countFollowing(target_uid);
        result->set("following_count", following);

        int64_t follower = UserFollowRelMgr::GetInstance()->countFollowers(target_uid);
        result->set("follower_count", follower);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
