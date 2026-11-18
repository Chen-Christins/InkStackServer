#include "user/servlets/admin/UserAdminDetailServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "blog/data/comment_info.h"
#include "user/client/ArticleClient.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

UserAdminDetailServlet::UserAdminDetailServlet()
    : BlogAdminServlet("UserAdminDetailServlet")
{
}

int32_t UserAdminDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        auto info = UserMgr::GetInstance()->get(user_id);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        int64_t article_count = client::ArticleClientMgr::GetInstance()->countByUser(user_id, true);

        std::vector<data::CommentInfo::ptr> comments;
        auto db = getDB();
        if (db) {
            data::CommentInfoDao::QueryByUserId(comments, user_id, db);
        }
        int64_t comment_count = comments.size();

        result->set("id", info->getId());
        result->set("account", info->getAccount());
        result->set("name", info->getName());
        result->set("email", info->getEmail());
        result->set("avatar", info->getAvatar());
        result->set("role", info->getRole());
        result->set("state", info->getState());
        result->set("create_time", info->getCreateTime());
        result->set("login_time", info->getLoginTime());
        result->set("article_count", article_count);
        result->set("comment_count", comment_count);
        result->set("bio", info->getBio());
        result->set("website", info->getWebsite());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
