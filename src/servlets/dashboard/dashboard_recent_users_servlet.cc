#include "dashboard_recent_users_servlet.h"

#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

DashboardRecentUsersServlet::DashboardRecentUsersServlet()
    : BlogLoginedServlet("DashboardRecentUsersServlet")
{
}

static const char* RoleToString(int32_t role)
{
    switch (role) {
    case UserManager::Role::USER:
        return "user";
    case UserManager::Role::ADMIN:
        return "admin";
    case UserManager::Role::EDITOR:
        return "editor";
    case UserManager::Role::CHECKER:
        return "checker";
    default:
        return "unknown";
    }
}

static const char* StatusToString(int32_t state)
{
    switch (state) {
    case UserManager::Status::INACTIVE:
        return "inactive";
    case UserManager::Status::ACTIVE:
        return "active";
    case UserManager::Status::BANNED:
        return "banned";
    default:
        return "unknown";
    }
}

int32_t DashboardRecentUsersServlet::handle(
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

        if ((int32_t)users.size() > limit) {
            users.resize(limit);
        }

        Json::Value list(Json::arrayValue);
        for (auto& u : users) {
            Json::Value item;
            item["name"] = u->getName();
            item["role"] = RoleToString(u->getRole());
            item["avatar"] = u->getAvatar();
            item["status"] = StatusToString(u->getState());
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
