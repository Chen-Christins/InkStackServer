#include "platform/servlets/organization/OrganizationJoinServlet.hpp"

#include <chen/log/log.h>
#include <json/json.h>

#include "common/event/EventDefine.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationJoinServlet::OrganizationJoinServlet()
    : BlogLoginedServlet("OrganizationJoinServlet")
{
}

int32_t OrganizationJoinServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        int64_t uid = getUserId(request);

        auto org = OrganizationMgr::GetInstance()->get(id);
        if (!org || org->getIsDeleted()) {
            result->setErrno(errcode::ARTICLE_INVALID_ID);
            break;
        }

        if (org->getStatus() == OrganizationManager::Status::INACTIVE) {
            result->setErrno(errcode::ORG_DISABLED);
            break;
        }

        auto info = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(id, uid);
        if (!info) {
            info = std::make_shared<data::OrganizationUserRelInfo>();
            info->setOrgId(id);
            info->setUserId(uid);
            info->setCreateTime(time(0));
        } else if (info->getStatus() == OrganizationUserRelManager::Status::APPROVED) {
            result->setErrno(errcode::ORG_ALREADY_JOINED);
            break;
        } else if (info->getStatus() == OrganizationUserRelManager::Status::PENDING) {
            result->setErrno(errcode::ORG_ALREADY_APPLIED);
            break;
        }
        info->setRole(OrganizationManager::Role::MEMBER);             // 普通成员
        info->setStatus(OrganizationUserRelManager::Status::PENDING); // 申请中
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::OrganizationUserRelInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::ORG_UPDATE_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationUserRelMgr::GetInstance()->add(info);

        // Notify org admins about join request
        {
            EventOrgMemberData data = {};
            data.type = "join_request";
            data.org_id = id;
            data.org_name = org->getName();
            data.applicant_id = uid;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_MEMBER, std::move(data));
        }

        result->set("id", org->getId());
        result->set("name", org->getName());
        result->set("description", org->getDescription());
        result->set("type", org->getType());
        result->set("status", org->getStatus());
        result->set("user_in_org", info->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
