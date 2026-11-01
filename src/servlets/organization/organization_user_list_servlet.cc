#include "organization_user_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationUserListServlet::OrganizationUserListServlet()
    : BlogLoginedServlet("OrganizationUserListServlet")
{
}

int32_t OrganizationUserListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");

        if (status == OrganizationUserRelManager::Status::PENDING) {
            int64_t uid = getUserId(request);
            auto info = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(id, uid);
            if (!info) {
                result->setErrno(errcode::ORG_NOT_MEMBER);
                break;
            }
            auto current_user = UserMgr::GetInstance()->get(uid);
            if (!current_user) {
                result->setErrno(errcode::USER_NOT_FOUND);
                break;
            }
            if (info->getRole() != OrganizationManager::Role::ORG_ADMIN && info->getRole() != OrganizationManager::Role::DEV_LEAD
                && current_user->getRole() != UserManager::Role::ADMIN) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
        }

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<data::OrganizationUserRelInfo::ptr> rels;
        int64_t total = blog::OrganizationUserRelMgr::GetInstance()->getByPages(rels, id, offset, page_size, status, true);

        std::sort(rels.begin(), rels.end(), [](const auto& a, const auto& b) {
            return a->getCreateTime() < b->getCreateTime();
        });

        result->set("total", total);
        Json::Value list;
        for (auto& i : rels) {
            auto user = blog::UserMgr::GetInstance()->get(i->getUserId());
            Json::Value item;
            item["id"] = i->getId();
            item["user_id"] = i->getUserId();
            item["name"] = user->getName();
            item["avatar"] = user->getAvatar();
            item["email"] = user->getEmail();
            item["role"] = i->getRole();
            item["status"] = i->getStatus();
            item["join_time"] = i->getCreateTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
