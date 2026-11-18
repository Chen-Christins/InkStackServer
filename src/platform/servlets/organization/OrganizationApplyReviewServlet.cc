#include "platform/servlets/organization/OrganizationApplyReviewServlet.hpp"

#include <chen/log/log.h>

#include "common/event/EventDefine.hpp"
#include "platform/manager/OrganizationApplyManager.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationApplyReviewServlet::OrganizationApplyReviewServlet()
    : BlogLoginedServlet("OrganizationApplyReviewServlet")
{
}

int32_t OrganizationApplyReviewServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, apply_id_str, "apply_id");
        DEFINE_AND_CHECK_STRING(result, action, "action");
        std::string reason = request->getParamAs<std::string>("reason");

        int64_t apply_id = 0;
        try {
            apply_id = chen::TypeUtil::Atoi(apply_id_str);
        } catch (...) {
            result->setErrno(errcode::PARAM_INVALID, "invalid apply_id");
            break;
        }

        if (action != "approve" && action != "reject") {
            result->setErrno(errcode::PARAM_INVALID, "action must be approve or reject");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto uinfo = UserMgr::GetInstance()->get(uid);
        if (!uinfo || uinfo->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto apply = OrganizationApplyMgr::GetInstance()->get(apply_id);
        if (!apply) {
            result->setErrno(errcode::ORG_APPLY_NOT_FOUND);
            break;
        }

        if (apply->getStatus() != OrganizationApplyManager::Status::PENDING) {
            result->setErrno(errcode::ORG_APPLY_ALREADY_REVIEWED);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (action == "approve") {
            // Create the organization
            auto org = std::make_shared<data::OrganizationInfo>();
            org->setName(apply->getOrgName());
            org->setType(apply->getOrgType());
            org->setDescription(apply->getOrgDescription());
            org->setOwnerId(apply->getUserId());
            org->setStatus(OrganizationManager::Status::ACTIVE);
            org->setIsDeleted(0);
            org->setCreateTime(time(0));
            org->setUpdateTime(time(0));

            if (data::OrganizationInfoDao::Insert(org, db)) {
                result->setErrno(errcode::ORG_INSERT_FAILED);
                ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            OrganizationMgr::GetInstance()->add(org);

            // Add applicant as org admin (role=5)
            auto rel = std::make_shared<data::OrganizationUserRelInfo>();
            rel->setOrgId(org->getId());
            rel->setUserId(apply->getUserId());
            rel->setRole(OrganizationManager::Role::ORG_ADMIN);
            rel->setStatus(OrganizationUserRelManager::Status::APPROVED);
            rel->setCreateTime(time(0));
            rel->setUpdateTime(time(0));

            if (data::OrganizationUserRelInfoDao::InsertOrUpdate(rel, db)) {
                result->setErrno(errcode::ORG_USER_REL_FAILED);
                ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            OrganizationUserRelMgr::GetInstance()->add(rel);

            // Update apply record
            apply->setStatus(OrganizationApplyManager::Status::APPROVED);
            apply->setReviewReason(reason);
            apply->setReviewedAt(time(0));

            if (data::OrganizationApplyInfoDao::Update(apply, db)) {
                result->setErrno(errcode::ORG_APPLY_UPDATE_FAILED);
                ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            result->set("org_id", org->getId());

            // Notify applicant
            {
                EventOrgApplyData data = {};
                data.type = "approved";
                data.applicant_id = apply->getUserId();
                data.org_name = apply->getOrgName();
                data.org_id = org->getId();

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_APPLY, std::move(data));
            }
        } else {
            // Reject
            apply->setStatus(OrganizationApplyManager::Status::REJECTED);
            apply->setReviewReason(reason);
            apply->setReviewedAt(time(0));

            if (data::OrganizationApplyInfoDao::Update(apply, db)) {
                result->setErrno(errcode::ORG_APPLY_UPDATE_FAILED);
                ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            // Notify applicant
            {
                EventOrgApplyData data = {};
                data.type = "rejected";
                data.applicant_id = apply->getUserId();
                data.org_name = apply->getOrgName();
                data.reason = reason;

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_APPLY, std::move(data));
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
