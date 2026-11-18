#include "platform/servlets/database/CleanupServlet.hpp"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/util.h>

#include "../../../common/util/Util.hpp"
#include "blog/data/backup_record_info.h"
#include "blog/data/export_record_info.h"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();
static chen::ConfigVar<std::string>::ptr server_work_path = chen::Config::Lookup<std::string>("server.work_path");

CleanupServlet::CleanupServlet()
    : BlogLoginedServlet("CleanupServlet")
{
}

int32_t CleanupServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        int64_t now = time(0);
        int64_t deadline = now - 30 * 24 * 3600;
        int64_t cleaned = 0;
        int64_t freed = 0;

        auto db = GetDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        auto qb_old = data::BackupRecordInfoDao::newQuery();
        qb_old->where("is_deleted", "=", (int64_t)1);
        qb_old->where("create_time", "<", deadline);

        std::vector<data::BackupRecordInfo::ptr> old_backups;
        data::BackupRecordInfoDao::QueryByBuilder(old_backups, qb_old, db);
        for (auto& b : old_backups) {
            if (!b->getFilePath().empty()) {
                std::string fp = b->getFilePath();
                if (fp[0] != '/') {
                    fp = server_work_path->getValue() + "/" + fp;
                }
                int64_t sz = b->getSize();
                if (chen::FSUtil::Unlink(fp, true) == 0) {
                    freed += sz;
                }
            }
            data::BackupRecordInfoDao::DeleteById(b->getId(), db);
            cleaned++;
        }

        auto qb_old_exp = data::ExportRecordInfoDao::newQuery();
        qb_old_exp->where("is_deleted", "=", (int64_t)1);
        qb_old_exp->where("create_time", "<", deadline);

        std::vector<data::ExportRecordInfo::ptr> old_exports;
        data::ExportRecordInfoDao::QueryByBuilder(old_exports, qb_old_exp, db);
        for (auto& e : old_exports) {
            if (!e->getFilePath().empty()) {
                std::string fp = e->getFilePath();
                if (fp[0] != '/') {
                    fp = server_work_path->getValue() + "/" + fp;
                }
                int64_t sz = e->getSize();
                if (chen::FSUtil::Unlink(fp, true) == 0) {
                    freed += sz;
                }
            }
            data::ExportRecordInfoDao::DeleteById(e->getId(), db);
            cleaned++;
        }

        result->set("cleaned", cleaned);
        result->set("freedBytes", freed);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
