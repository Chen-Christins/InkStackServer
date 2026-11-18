#include "platform/servlets/organization/OrganizationCreateServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationCreateServlet::OrganizationCreateServlet()
    : BlogLoginedServlet("OrganizationCreateServlet")
{
}

int32_t OrganizationCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t oid = request->getParamAs<int64_t>("id", 0);

        int64_t user_id = getUserId(request);
        if (!user_id) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto uinfo = UserMgr::GetInstance()->get(user_id);
        if (!uinfo) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (uinfo->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        bool new_org = false;
        data::OrganizationInfo::ptr info;
        if (oid) {
            info = OrganizationMgr::GetInstance()->get(oid);
            if (!info) {
                result->setErrno(errcode::ORG_NOT_FOUND);
                break;
            }

            info->setName(name);
        } else {
            info = OrganizationMgr::GetInstance()->getByName(name);
            if (!info) {
                info = std::make_shared<data::OrganizationInfo>();
                info->setName(name);
                info->setCreateTime(time(0));
                new_org = true;
            } else if (info->getIsDeleted()) {
                info->setCreateTime(time(0));
            }
        }
        info->setType(type);
        info->setStatus(status);
        info->setOwnerId(user_id);
        info->setDescription(desc);
        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::OrganizationInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::ORG_UPDATE_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationMgr::GetInstance()->add(info);

        if (new_org) {
            auto rel = std::make_shared<data::OrganizationUserRelInfo>();
            rel->setOrgId(info->getId());
            rel->setUserId(user_id);
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
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("type", info->getType());
        result->set("status", info->getStatus());
        result->set("description", info->getDescription());
        result->set("create_time", info->getCreateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
