#include "comment/frame/CommentModule.hpp"

#include <exception>

#include <chen/ds/event_bus.h>
#include <chen/log/log.h>
#include <chen/util/macro.h>

#include "comment/include/Servlets.hpp"
#include "common/event/Events.hpp"
#include "common/frame/ServiceBootstrap.hpp"
#include "common/frame/ServletRegistrar.hpp"
#include "common/include/Tables.hpp"
#include "common/metrics/ServiceMetrics.hpp"
#include "common/rpc/RpcEndpoint.hpp"
#include "common/rpc/RpcHubClient.hpp"
#include "comment/rpc/CommentRpcService.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

CommentModule::CommentModule()
    : Module("CommentService", "1.0.8", "comment_module")
{
}

bool CommentModule::onLoad()
{
    return BootstrapOnLoad("CommentService");
}

bool CommentModule::onUnload()
{
    INFO(logger) << "onUnload";
    rpc::RpcHubClientMgr::GetInstance()->stop();
    BootstrapOnUnload();
    return true;
}

bool CommentModule::onDeactivate()
{
    INFO(logger) << "onDeactivate";
    return true;
}

void CommentModule::onTick()
{
    static uint32_t s_sync_tick = 0;

    try {
        metrics::SampleRates();

        rpc::RpcHubClientMgr::GetInstance()->ensureConnected();

        s_sync_tick++;
        if (s_sync_tick % 5 == 0) {
            CleanupExpiredSessions(3600);
        }
        if (s_sync_tick % 1440 == 0) {
            s_sync_tick = 0;
        }
    } catch (const std::exception& e) {
        ERROR(logger) << "onTick exception: " << e.what();
    } catch (...) {
        ERROR(logger) << "onTick unknown exception";
    }
}

uint64_t CommentModule::getTickIntervalMs()
{
    return 60 * 1000;
}

bool CommentModule::onServerReady()
{
    INFO(logger) << "onServerReady";

    chen::EventBusMgr::GetInstance()->clearAll();

    if (!initMySQL()) {
        ERROR(logger) << "initMySQL failed";
        return false;
    }

    if (!LoadErrorCodes()) {
        return false;
    }

    std::vector<chen::http::HttpServer::ptr> http_servers;
    getAllHttpServer(http_servers);
    std::vector<chen::http::WSServer::ptr> ws_servers;
    getAllWSServer(ws_servers);
    InitServiceMetrics(getName(), getVersion(), http_servers, ws_servers);

    registerServlets();

    rpc::RegisterCommentRpcMethods();

    if (!EventMsgsInit()) {
        ERROR(logger) << "EventMsgsInit failed";
        return false;
    }

    return true;
}

bool CommentModule::onServerUp()
{
    INFO(logger) << "onServerUp";

    rpc::RpcHubClientMgr::GetInstance()->start(rpc::FUNC_COMMENT);
    return true;
}

bool CommentModule::initMySQL()
{
    INFO(logger) << "initMySQL";

    auto dbs = ConnectMySQL();
    if (dbs.empty()) {
        return false;
    }

    for (auto& mysql : dbs) {
#define XX(class_name, table_name)                             \
    if (data::class_name::CreateTableMySQL(mysql)) {           \
        ERROR(logger) << "create " table_name " table failed"; \
        return false;                                          \
    }
        XX(CommentInfoDao, "comment")
        XX(CommentPraiseRelInfoDao, "comment_praise_rel")
#undef XX

        INFO(logger) << "migrate database begin";
#define XX(clazz)                                           \
    if (blog::data::clazz::MigrateTableMySQL(mysql) != 0) { \
        ERROR(logger) << "migrate table mysql failed";      \
        return false;                                       \
    }
        XX(CommentInfoDao)
        XX(CommentPraiseRelInfoDao)
#undef XX
        INFO(logger) << "migrate database end";
    }

    return true;
}

void CommentModule::registerServlets()
{
    INFO(logger) << "registerServlets";

    std::vector<chen::http::HttpServer::ptr> http_servers;
    getAllHttpServer(http_servers);

    if (http_servers.empty()) {
        ERROR(logger) << "No HTTP server found";
        return;
    }

    for (auto& i : http_servers) {
        const auto hs = std::dynamic_pointer_cast<chen::http::HttpServer>(i);
        ASSERT_RET(hs != nullptr);

        const auto dispatch = hs->getServletDispatch();
        ASSERT_RET(dispatch != nullptr);

        ServletRegistrar dp(dispatch);

#define XX(clazz) std::make_shared<servlet::clazz>()
        dp->addServlet("/api/v1/article/comment/list", XX(CommentListServlet));
        dp->addServlet("/api/v1/article/comment/replies", XX(CommentRepliesServlet));
        dp->addServlet("/api/v1/article/comment/create", XX(CommentCreateServlet));
        dp->addServlet("/api/v1/article/comment/update", XX(CommentUpdateServlet));
        dp->addServlet("/api/v1/article/comment/delete", XX(CommentDeleteServlet));
        dp->addServlet("/api/v1/article/comment/praise", XX(CommentPraiseServlet));
        dp->addServlet("/api/v1/admin/comment/list", XX(AdminCommentListServlet));
        dp->addServlet("/api/v1/admin/comment/approve", XX(AdminCommentApproveServlet));
        dp->addServlet("/api/v1/admin/comment/reject", XX(AdminCommentRejectServlet));
        dp->addServlet("/api/v1/admin/comment/spam", XX(AdminCommentSpamServlet));
        dp->addServlet("/api/v1/admin/comment/delete", XX(AdminCommentDeleteServlet));
        dp->addServlet("/api/v1/admin/comment/stats", XX(AdminCommentStatsServlet));
#undef XX
    }
}

} // namespace blog

extern "C" {

chen::Module* CreateModule()
{
    chen::Module* module = new blog::CommentModule;
    INFO(blog::logger) << "CommentModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(blog::logger) << "CommentModule::DestroyModule";
    delete module;
}
}
