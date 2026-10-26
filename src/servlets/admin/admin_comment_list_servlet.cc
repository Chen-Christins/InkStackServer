#include "admin_comment_list_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/comment_praise_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <json/json.h>

namespace blog {
namespace servlet {

AdminCommentListServlet::AdminCommentListServlet()
    : BlogLoginedServlet("AdminCommentListServlet")
{
}

int32_t AdminCommentListServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, int64_t, page_size, "page_size");

        std::string status_str = request->getParam("status");
        int32_t status = 0;
        if (!status_str.empty()) {
            if (status_str == "approved") {
                status = CommentManager::APPROVED;
            } else if (status_str == "pending") {
                status = CommentManager::PENDING;
            } else if (status_str == "rejected") {
                status = CommentManager::REJECTED;
            } else if (status_str == "spam") {
                status = CommentManager::SPAM;
            }
        }
        std::string keyword = request->getParam("keyword");
        int64_t article_id = request->getParamAs<int64_t>("article_id", 0);
        int32_t is_reported = request->getParamAs<int32_t>("is_reported", -1);

        std::vector<data::CommentInfo::ptr> comments;
        int64_t total = CommentMgr::GetInstance()->listByAdmin(comments, page_num, page_size, status, keyword, article_id, is_reported);

        Json::Value list(Json::arrayValue);
        for (auto& c : comments) {
            Json::Value item;
            item["id"] = std::to_string(c->getId());
            item["content"] = c->getContent();
            item["parent_id"] = c->getParentId() > 0 ? std::to_string(c->getParentId()) : "";
            item["status"] = [](int32_t s) -> const char* {
                switch (s) {
                case CommentManager::PENDING:
                    return "pending";
                case CommentManager::APPROVED:
                    return "approved";
                case CommentManager::REJECTED:
                    return "rejected";
                case CommentManager::SPAM:
                    return "spam";
                default:
                    return "unknown";
                }
            }(c->getStatus());
            item["created_at"] = c->getCreateTime();
            item["updated_at"] = c->getUpdateTime();
            item["likes"] = CommentPraiseRelMgr::GetInstance()->countByComment(c->getId());
            item["reply_count"] = CommentMgr::GetInstance()->countReplies(c->getId());
            item["is_reported"] = c->getIsReported() != 0;
            item["report_count"] = c->getReportCount();
            item["ip"] = c->getIp();
            item["user_agent"] = c->getUserAgent();

            // author info
            Json::Value author;
            auto u = UserMgr::GetInstance()->get(c->getUserId());
            if (u) {
                author["id"] = std::to_string(u->getId());
                author["name"] = u->getName();
                author["email"] = u->getEmail();
                author["avatar"] = u->getAvatar();
            }
            item["author"] = author;

            // article info
            Json::Value article;
            auto a = ArticleMgr::GetInstance()->get(c->getArticleId());
            if (a) {
                article["id"] = std::to_string(a->getId());
                article["title"] = a->getTitle();
            }
            item["article"] = article;

            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
