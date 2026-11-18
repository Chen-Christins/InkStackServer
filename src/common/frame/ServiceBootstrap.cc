#include "common/frame/ServiceBootstrap.hpp"

#include <map>
#include <memory>
#include <ranges>

#include <chen/config/config.h>
#include <chen/db/fox_thread.h>
#include <chen/db/redis.h>
#include <chen/ds/event_bus.h>
#include <chen/http/session_data.h>
#include <chen/log/log.h>

#include "common/frame/CrashHandler.hpp"
#include "common/i18n/I18n.hpp"
#include "common/manager/ErrorCodeManager.hpp"
#include "common/metrics/ServiceMetrics.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

static chen::ConfigVar<std::string>::ptr g_work_path = chen::Config::Lookup<std::string>("server.work_path", std::string(""), "server work path");

std::string GetWorkPath()
{
    return g_work_path->getValue();
}

bool BootstrapOnLoad(const std::string& service_name)
{
    INFO(logger) << service_name << " onLoad version=1.0.8";

    if (g_work_path->getValue().empty()) {
        ERROR(logger) << "work path is empty";
        return false;
    }

    const std::string crash_log = g_work_path->getValue() + "/logs/crash.log";
    if (InstallCrashHandler(crash_log)) {
        INFO(logger) << "crash handler installed, path=" << crash_log;
    } else {
        WARN(logger) << "install crash handler failed, path=" << crash_log;
    }

    if (!I18n::Init(g_work_path->getValue() + "/locale")) {
        WARN(logger) << "i18n unavailable, error messages will fall back to errors.json";
    }

    chen::FoxThreadMgr::GetInstance()->init();
    chen::FoxThreadMgr::GetInstance()->start();
    chen::RedisMgr::GetInstance()->init();

    return true;
}

void BootstrapOnUnload()
{
    chen::EventBusMgr::GetInstance()->clearAll();
    chen::FoxThreadMgr::GetInstance()->stop();
    UninstallCrashHandler();
}

std::vector<chen::MySQL::ptr> ConnectMySQL()
{
    std::vector<chen::MySQL::ptr> dbs;
    const auto& mysql_dbs = g_mysql_dbs->getValue();
    for (const auto& params : mysql_dbs | std::views::values) {
        auto mysql = std::make_shared<chen::MySQL>(params);
        if (!mysql->connect()) {
            ERROR(logger) << "connect mysql failed";
            return {};
        }
        dbs.push_back(mysql);
    }
    return dbs;
}

bool LoadErrorCodes()
{
    const std::string errors_path = g_work_path->getValue() + "/errors.json";
    if (!ErrorCodeMgr::GetInstance()->load(errors_path)) {
        ERROR(logger) << "Failed to load error codes from " << errors_path;
        return false;
    }
    return true;
}

void InitServiceMetrics(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers)
{
    metrics::Init(name, version, http_servers, ws_servers);
}

void CleanupExpiredSessions(int64_t ttl_seconds)
{
    chen::http::SessionDataMgr::GetInstance()->check(ttl_seconds);
}

} // namespace blog
