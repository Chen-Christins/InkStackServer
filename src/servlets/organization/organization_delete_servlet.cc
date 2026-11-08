#include "organization_delete_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/organization_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationDeleteServlet::OrganizationDeleteServlet()
    : BlogLoginedServlet("OrganizationDeleteServlet")
{
}

int32_t OrganizationDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> assignment_ids;
        auto tmp = chen::StringUtil::Split(ids, ",");
        for (auto& i : tmp) {
            assignment_ids.insert(chen::TypeUtil::Atoi(i));
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user || current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }
        std::vector<data::OrganizationInfo::ptr> infos;
        if (!OrganizationMgr::GetInstance()->listByPages(infos, 0, INT32_MAX, -1, true)) {
            break;
        }

        std::vector<data::OrganizationInfo::ptr> del_organizations;
        for (auto& i : infos) {
            if (assignment_ids.count(i->getId())) {
                del_organizations.push_back(i);
            }
        }
        if (del_organizations.empty()) {
            result->setErrno(errcode::ORG_NO_VALID_ORGS);
            break;
        }

        // 收集所有待删除组织关联的用户关系记录
        std::vector<data::OrganizationUserRelInfo::ptr> all_rels;
        for (auto& org : del_organizations) {
            std::vector<data::OrganizationUserRelInfo::ptr> rels;
            OrganizationUserRelMgr::GetInstance()->getByPages(rels, org->getId(), 0, INT32_MAX, -1, true);
            for (auto& rel : rels) {
                all_rels.push_back(rel);
            }
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
        for (auto& i : del_organizations) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            data::OrganizationInfoDao::Update(i, db);
        }
        for (auto& rel : all_rels) {
            rel->setIsDeleted(1);
            rel->setUpdateTime(now);
            data::OrganizationUserRelInfoDao::Update(rel, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(errcode::DB_COMMIT_FAILED);

            for (auto& i : del_organizations) {
                i->setIsDeleted(0);
            }
            for (auto& rel : all_rels) {
                rel->setIsDeleted(0);
            }
            break;
        }
        for (auto& i : del_organizations) {
            OrganizationMgr::GetInstance()->add(i);
        }
        for (auto& rel : all_rels) {
            OrganizationUserRelMgr::GetInstance()->add(rel);
        }
        if (!del_organizations.empty()) {
            Json::Value json_ids;
            for (auto& i : del_organizations) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);

            // Notify all org members
            for (auto& org : del_organizations) {
                std::set<int64_t> notified_users;
                for (auto& rel : all_rels) {
                    if (rel->getOrgId() != org->getId() || rel->getIsDeleted())
                        continue;
                    int64_t member_id = rel->getUserId();
                    if (notified_users.count(member_id))
                        continue;
                    notified_users.insert(member_id);

                    EventOrgDeletedData data = {};
                    data.member_id = member_id;
                    data.org_id = org->getId();
                    data.org_name = org->getName();

                    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ORG_DELETED, std::move(data));
                }
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
