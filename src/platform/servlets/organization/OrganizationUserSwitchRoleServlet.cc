#include "platform/servlets/organization/OrganizationUserSwitchRoleServlet.hpp"

#include <chen/log/log.h>
#include <json/json.h>

#include "common/event/EventDefine.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Permission.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationUserSwitchRoleServlet::OrganizationUserSwitchRoleServlet()
    : BlogLoginedServlet("OrganizationUserSwitchRoleServlet")
{
}

int32_t OrganizationUserSwitchRoleServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");           // organization_user_rel id
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");   // organization id
        DEFINE_AND_CHECK_TYPE(result, int32_t, role, "role");       // new role
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id"); // user_id

        // 操作者权限检查
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = blog::UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = current_user->getRole();

        auto rel = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!rel) {
            result->setErrno(errcode::ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = rel->getRole();

        if (!permission::CanSwitchRole(system_role, org_role, role)) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        // 更新用户角色
        rel = blog::OrganizationUserRelMgr::GetInstance()->get(id);
        if (!rel || rel->getOrgId() != org_id || rel->getUserId() != user_id) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            ;
            break;
        }

        if (role != OrganizationManager::Role::MEMBER && role != OrganizationManager::Role::REPORTER && role != OrganizationManager::Role::DEVELOPER
            && role != OrganizationManager::Role::DEV_LEAD && role != OrganizationManager::Role::ORG_ADMIN) {
            result->setErrno(errcode::PARAM_INVALID, "invalid role");
            break;
        }

        rel->setRole(role);
        rel->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::OrganizationUserRelInfoDao::InsertOrUpdate(rel, db)) {
            result->setErrno(errcode::ORG_USER_REL_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << ", errstr=" << db->getErrStr();
            break;
        }

        OrganizationUserRelMgr::GetInstance()->add(rel);

        // Notify user about role change
        {
            auto org = OrganizationMgr::GetInstance()->get(org_id);
            std::string org_name = org ? org->getName() : std::to_string(org_id);
            const char* role_name = "普通成员";
            if (role == OrganizationManager::Role::REPORTER) {
                role_name = "报告者";
            } else if (role == OrganizationManager::Role::DEVELOPER) {
                role_name = "开发者";
            } else if (role == OrganizationManager::Role::DEV_LEAD) {
                role_name = "研发主管";
            } else if (role == OrganizationManager::Role::ORG_ADMIN) {
                role_name = "组织管理员";
            }

            EventOrgMemberData data = {};
            data.type = "role_change";
            data.org_id = org_id;
            data.org_name = org_name;
            data.operator_id = uid;
            data.target_user_id = user_id;
            data.new_role_name = role_name;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_MEMBER, std::move(data));
        }

        auto user = blog::UserMgr::GetInstance()->get(rel->getUserId());
        result->set("id", rel->getId());
        result->set("user_id", rel->getUserId());
        result->set("name", user->getName());
        result->set("avatar", user->getAvatar());
        result->set("email", user->getEmail());
        result->set("role", rel->getRole());
        result->set("status", rel->getStatus());
        result->set("join_time", rel->getCreateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
