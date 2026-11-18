#include "platform/servlets/organization/OrganizationJoinCheckServlet.hpp"

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

OrganizationJoinCheckServlet::OrganizationJoinCheckServlet()
    : BlogLoginedServlet("OrganizationJoinCheckServlet")
{
}

int32_t OrganizationJoinCheckServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_TYPE(result, int32_t, state, "state");

        if (state != OrganizationUserRelManager::Status::REJECTED && state != OrganizationUserRelManager::Status::APPROVED) {
            result->setErrno(errcode::PARAM_INVALID, "invalid state");
            break;
        }

        auto user = blog::UserMgr::GetInstance()->get(user_id);
        auto org = blog::OrganizationMgr::GetInstance()->get(org_id);
        if (!user || !org) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        // check operater permission
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

        if (!permission::CanManageMembers(system_role, org_role)) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        // update user organization relation
        rel = blog::OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, user_id);
        if (!rel) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }
        if (rel->getStatus() != OrganizationUserRelManager::Status::PENDING) {
            result->setErrno(errcode::ORG_INVALID_STATE);
            break;
        }
        rel->setStatus(state);
        rel->setRole(OrganizationManager::Role::MEMBER);

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

        // Notify applicant + mark other admins' notifications as read
        {
            EventOrgMemberData data = {};
            data.type = (state == OrganizationUserRelManager::Status::APPROVED) ? "join_approved" : "join_rejected";
            data.org_id = org_id;
            data.org_name = org->getName();
            data.applicant_id = user_id;
            data.operator_id = uid;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
