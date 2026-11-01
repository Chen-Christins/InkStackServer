#include "organization_my_applies_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_apply_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationMyAppliesServlet::OrganizationMyAppliesServlet()
    : BlogLoginedServlet("OrganizationMyAppliesServlet")
{
}

int32_t OrganizationMyAppliesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<data::OrganizationApplyInfo::ptr> applies;
        int64_t total = OrganizationApplyMgr::GetInstance()->listByUserId(applies, uid, offset, page_size, true);

        result->set("total", total);
        Json::Value list;
        for (auto& i : applies) {
            Json::Value item;
            item["id"] = i->getId();
            item["user_id"] = i->getUserId();

            auto apply_user = UserMgr::GetInstance()->get(i->getUserId());
            item["user_name"] = apply_user ? apply_user->getName() : "";

            item["org_name"] = i->getOrgName();
            item["org_type"] = i->getOrgType();
            item["org_description"] = i->getOrgDescription();
            item["status"] = i->getStatus();
            item["review_reason"] = i->getReviewReason();
            item["created_at"] = i->getCreatedAt();
            item["reviewed_at"] = i->getReviewedAt();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
