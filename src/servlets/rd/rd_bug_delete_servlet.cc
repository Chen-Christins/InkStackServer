#include "rd_bug_delete_servlet.h"

#include "../../manager/bug_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../permission.h"
#include "../../util.h"

namespace blog {
namespace servlet {

RdBugDeleteServlet::RdBugDeleteServlet()
    : BlogLoginedServlet("RdBugDeleteServlet")
{
}

int32_t RdBugDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        std::string idsStr = request->getParam("ids");

        std::set<int64_t> delIds;
        if (id) {
            delIds.insert(id);
        }
        if (!idsStr.empty()) {
            for (auto& s : chen::StringUtil::Split(idsStr, ",")) {
                delIds.insert(chen::TypeUtil::Atoi(s));
            }
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserManager::Role::ADMIN);

        int32_t org_role = 0;
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            org_role = rel->getRole();
        }

        std::vector<data::BugInfo::ptr> all;
        BugMgr::GetInstance()->listByOrg(all, org_id, 0, INT32_MAX, -1, true);
        std::vector<data::BugInfo::ptr> delItems;
        for (auto& i : all) {
            if (!delIds.count(i->getId())) {
                continue;
            }
            if (!is_platform_admin && uid != i->getCreatorId() && !permission::CanDeleteBug(org_role)) {
                continue;
            }
            delItems.push_back(i);
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }
        time_t now = time(0);
        for (auto& i : delItems) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            data::BugInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            for (auto& i : delItems) {
                i->setIsDeleted(0);
            }
            result->setErrno(errcode::DB_COMMIT_FAILED);
            break;
        }
        if (!delItems.empty()) {
            Json::Value json_ids;
            for (auto& i : delItems) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
