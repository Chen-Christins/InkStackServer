#include "platform/servlets/database/BackupCreateServlet.hpp"

#include <chen/config/config.h>
#include <chen/log/log.h>

#include "common/event/EventDefine.hpp"
#include "platform/manager/BackupRecordManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"

#include <sys/stat.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_data_path =
    chen::Config::Lookup<std::string>("server.data_path", std::string(""), "server data storage path");

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

BackupCreateServlet::BackupCreateServlet()
    : BlogLoginedServlet("BackupCreateServlet")
{
}

int32_t BackupCreateServlet::handle(
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

        std::string type;
        if (!request->checkGetParamAs("type", type) || type.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "param type is required");
            break;
        }
        if (type != "full" && type != "incremental") {
            result->setErrno(errcode::PARAM_INVALID, "type must be full or incremental");
            break;
        }

        std::string timestamp = std::to_string(time(0));
        std::string name = "backup_" + type + "_" + timestamp;

        auto info = BackupRecordMgr::GetInstance()->create(uid, type, name, "");
        if (!info) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "create backup failed");
            break;
        }

        std::string storage_path = g_data_path->getValue();
        if (storage_path.empty()) {
            ERROR(logger) << "storage_path is empty";
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        int64_t backup_id = info->getId();
        auto mysql_dbs = g_mysql_dbs->getValue();

        // Trigger async backup via event bus
        {
            EventDatabaseBackupData data = {};
            data.backup_id = backup_id;
            data.type = type;
            data.name = name;
            data.mysql_dbs = mysql_dbs;
            data.work_path = storage_path;
            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_DATABASE_BACKUP, std::move(data));
        }

        Json::Value item;
        item["id"] = info->getId();
        item["name"] = info->getName();
        item["type"] = info->getType();
        item["size"] = info->getSize();
        item["file_count"] = info->getFileCount();
        item["status"] = info->getStatus();
        item["created_at"] = info->getCreateTime();
        auto created_by_user = UserMgr::GetInstance()->get(info->getCreatedBy());
        if (created_by_user) {
            item["created_by"] = created_by_user->getName();
        }
        item["description"] = info->getDescription();

        result->set("data", item);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
