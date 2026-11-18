#include "user/servlets/user/UserUnfollowServlet.hpp"

#include "user/manager/UserFollowRelManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

UserUnfollowServlet::UserUnfollowServlet()
    : BlogLoginedServlet("UserUnfollowServlet")
{
}

int32_t UserUnfollowServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, following_id, "following_id");

        if (!UserFollowRelMgr::GetInstance()->unfollow(uid, following_id)) {
            result->setErrno(errcode::USER_NOT_FOLLOWING);
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
