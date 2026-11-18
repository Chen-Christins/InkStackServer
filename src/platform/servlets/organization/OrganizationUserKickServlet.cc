#include "platform/servlets/organization/OrganizationUserKickServlet.hpp"

#include <chen/log/log.h>
#include <json/json.h>

#include "../../../common/util/Util.hpp"
#include "common/Permission.hpp"
#include "common/event/EventDefine.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationUserKickServlet::OrganizationUserKickServlet()
    : BlogLoginedServlet("OrganizationUserKickServlet")
{
}

int32_t OrganizationUserKickServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");           // organization_user_rel id
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");   // organization id
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id"); // user_id

        // 操作者权限检查
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto user = blog::UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();

        auto rel = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!rel) {
            result->setErrno(errcode::ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = rel->getRole();

        if (!permission::CanManageMembers(system_role, org_role)) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        // 踢出用户
        rel = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, user_id);
        if (!rel) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        rel->setStatus(OrganizationUserRelManager::Status::EXITED);
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

        // Notify org admins and kicked user
        {
            auto org = OrganizationMgr::GetInstance()->get(org_id);
            auto kicked_user = UserMgr::GetInstance()->get(user_id);
            auto oper_user = UserMgr::GetInstance()->get(uid);
            std::string org_name = org ? org->getName() : std::to_string(org_id);
            std::string kicked_name = kicked_user ? kicked_user->getName() : std::to_string(user_id);
            std::string oper_name = oper_user ? oper_user->getName() : std::to_string(uid);

            EventOrgMemberData data = {};
            data.type = "kicked";
            data.org_id = org_id;
            data.org_name = org_name;
            data.operator_id = uid;
            data.operator_name = oper_name;
            data.kicked_user_id = user_id;
            data.applicant_name = kicked_name;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
