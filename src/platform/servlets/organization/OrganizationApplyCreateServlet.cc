#include "platform/servlets/organization/OrganizationApplyCreateServlet.hpp"

#include <chen/log/log.h>

#include "common/event/EventDefine.hpp"
#include "platform/manager/OrganizationApplyManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationApplyCreateServlet::OrganizationApplyCreateServlet()
    : BlogLoginedServlet("OrganizationApplyCreateServlet")
{
}

int32_t OrganizationApplyCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        std::string desc = request->getParamAs<std::string>("desc");

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

        auto info = std::make_shared<data::OrganizationApplyInfo>();
        info->setUserId(user_id);
        info->setOrgName(name);
        info->setOrgType(type);
        info->setOrgDescription(desc);
        info->setStatus(OrganizationApplyManager::Status::PENDING);
        info->setCreatedAt(time(0));
        info->setIsDeleted(0);

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::OrganizationApplyInfoDao::Insert(info, db)) {
            result->setErrno(errcode::ORG_APPLY_INSERT_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        result->set("apply_id", info->getId());

        // Notify all ADMIN users about new org application
        {
            auto applicant = UserMgr::GetInstance()->get(user_id);
            EventOrgApplyData data = {};
            data.type = "created";
            data.applicant_id = user_id;
            data.applicant_name = applicant ? applicant->getName() : std::to_string(user_id);
            data.org_name = name;
            data.apply_id = info->getId();

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_APPLY, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
