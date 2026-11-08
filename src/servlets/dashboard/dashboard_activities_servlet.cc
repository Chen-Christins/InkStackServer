#include "dashboard_activities_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/comment_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

DashboardActivitiesServlet::DashboardActivitiesServlet()
    : BlogLoginedServlet("DashboardActivitiesServlet")
{
}

struct ActivityItem {
    std::string type;
    std::string title;
    int64_t create_time;
};

static std::string TimeAgo(int64_t now, int64_t ts)
{
    int64_t diff = now - ts;
    if (diff < 0) {
        diff = 0;
    }
    if (diff < 60) {
        return "just now";
    }
    if (diff < 3600) {
        return std::to_string(diff / 60) + "分钟前";
    }
    if (diff < 86400) {
        return std::to_string(diff / 3600) + "小时前";
    }
    return std::to_string(diff / 86400) + "天前";
}

int32_t DashboardActivitiesServlet::handle(
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

        int32_t limit = request->getParamAs<int32_t>("limit", 5);
        if (limit <= 0) {
            limit = 5;
        }
        int32_t fetch_count = limit * 2;

        std::vector<ActivityItem> all;

        // recent users
        {
            std::vector<int64_t> ids;
            UserMgr::GetInstance()->getAllIds(ids, true);
            std::vector<data::UserInfo::ptr> users;
            for (auto id : ids) {
                auto u = UserMgr::GetInstance()->get(id);
                if (u) {
                    users.push_back(u);
                }
            }
            std::sort(users.begin(), users.end(), [](auto& a, auto& b) {
                return a->getCreateTime() > b->getCreateTime();
            });
            for (size_t i = 0; i < users.size() && i < (size_t)fetch_count; i++) {
                ActivityItem act;
                act.type = "user_register";
                act.title = "新用户 \"" + users[i]->getName() + "\" 注册了账户";
                act.create_time = users[i]->getCreateTime();
                all.push_back(act);
            }
        }

        // recent articles
        {
            std::vector<data::ArticleInfo::ptr> articles;
            ArticleMgr::GetInstance()->listByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true);
            std::sort(articles.begin(), articles.end(), [](auto& a, auto& b) {
                return a->getCreateTime() > b->getCreateTime();
            });
            for (size_t i = 0; i < articles.size() && i < (size_t)fetch_count; i++) {
                ActivityItem act;
                act.type = "article_publish";
                // get author name
                auto author = UserMgr::GetInstance()->get(articles[i]->getUserId());
                std::string author_name = author ? author->getName() : "Unknown";
                act.title = author_name + " 发布了新文章《" + articles[i]->getTitle() + "》";
                act.create_time = articles[i]->getCreateTime();
                all.push_back(act);
            }
        }

        // recent comments
        {
            std::vector<data::CommentInfo::ptr> comments;
            CommentMgr::GetInstance()->listByAdmin(comments, 1, fetch_count, 0, "", 0, -1);
            for (auto& c : comments) {
                ActivityItem act;
                act.type = "comment_create";
                auto commenter = UserMgr::GetInstance()->get(c->getUserId());
                std::string commenter_name = commenter ? commenter->getName() : "Unknown";
                // get article title for context
                auto article = ArticleMgr::GetInstance()->get(c->getArticleId());
                std::string article_title = article ? article->getTitle() : "Unknown";
                act.title = commenter_name + " 在《" + article_title + "》中发表了评论";
                act.create_time = c->getCreateTime();
                all.push_back(act);
            }
        }

        // sort merged by create_time desc
        std::sort(all.begin(), all.end(), [](auto& a, auto& b) {
            return a.create_time > b.create_time;
        });

        if ((int32_t)all.size() > limit) {
            all.resize(limit);
        }

        int64_t now = time(0);
        Json::Value list(Json::arrayValue);
        for (auto& act : all) {
            Json::Value item;
            item["type"] = act.type;
            item["title"] = act.title;
            item["timestamp"] = act.create_time;
            item["time"] = TimeAgo(now, act.create_time);
            list.append(item);
        }

        result->set("list", list);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
