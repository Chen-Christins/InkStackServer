#include "user_admin_recover_servlet.h"

#include "../../event/event_define.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

#include <chen/log/log.h>

#include <set>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminRecoverServlet::UserAdminRecoverServlet()
    : BlogAdminServlet("UserAdminRecoverServlet")
{
}

int32_t UserAdminRecoverServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");

        std::set<int64_t> user_ids;
        auto tmp = chen::StringUtil::Split(ids, ',');
        for (auto& i : tmp) {
            user_ids.insert(chen::TypeUtil::Atoi(i));
        }

        std::vector<data::UserInfo::ptr> infos;
        for (const int64_t& id : user_ids) {
            auto info = UserMgr::GetInstance()->get(id);
            if (!info || !info->getIsDeleted()) {
                continue;
            }
            infos.emplace_back(info);
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
        for (auto& i : infos) {
            i->setIsDeleted(0);
            i->setState(UserManager::Status::ACTIVE);
            i->setUpdateTime(now);
            data::UserInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(errcode::DB_COMMIT_FAILED);

            for (auto& i : infos) {
                i->setIsDeleted(0);
            }
            break;
        }
        for (auto& i : infos) {
            UserMgr::GetInstance()->update(i);
        }
        if (!infos.empty()) {
            auto& jids = result->jsondata["ids"];
            for (auto& i : infos) {
                jids.append(i->getId());
            }

            // Notify recovered users
            for (auto& u : infos) {
                EventUserAdminData data = {};
                data.type = "account_recovered";
                data.user_id = u->getId();

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_USER_ADMIN, std::move(data));
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
