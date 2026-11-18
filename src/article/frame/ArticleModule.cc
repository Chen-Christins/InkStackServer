#include "article/frame/ArticleModule.hpp"

#include <exception>

#include <chen/ds/event_bus.h>
#include <chen/log/log.h>
#include <chen/util/macro.h>

#include "article/include/Servlets.hpp"
#include "article/logic/Index.hpp"
#include "article/manager/ArticleManager.hpp"
#include "common/event/Events.hpp"
#include "common/frame/ServiceBootstrap.hpp"
#include "common/frame/ServletRegistrar.hpp"
#include "common/include/Tables.hpp"
#include "common/metrics/ServiceMetrics.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

ArticleModule::ArticleModule()
    : Module("ArticleService", "1.0.8", "article_module")
{
}

bool ArticleModule::onLoad()
{
    return BootstrapOnLoad("ArticleService");
}

bool ArticleModule::onUnload()
{
    INFO(logger) << "onUnload";
    ArticleMgr::GetInstance()->stop();
    BootstrapOnUnload();
    return true;
}

bool ArticleModule::onDeactivate()
{
    INFO(logger) << "onDeactivate";
    ArticleMgr::GetInstance()->stop();
    return true;
}

void ArticleModule::onTick()
{
    static uint32_t s_sync_tick = 0;

    try {
        metrics::SampleRates();

        ArticleMgr::GetInstance()->onTick();

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

uint64_t ArticleModule::getTickIntervalMs()
{
    return 60 * 1000;
}

bool ArticleModule::onServerReady()
{
    INFO(logger) << "onServerReady";

    chen::EventBusMgr::GetInstance()->clearAll();

    if (!initMySQL()) {
        ERROR(logger) << "initMySQL failed";
        return false;
    }

    ArticleMgr::GetInstance()->start();

    if (!IndexMgr::GetInstance()->initFromFile()) {
        INFO(logger) << "index load failed";
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

    if (!EventMsgsInit()) {
        ERROR(logger) << "EventMsgsInit failed";
        return false;
    }

    return true;
}

bool ArticleModule::onServerUp()
{
    INFO(logger) << "onServerUp";
    return true;
}

bool ArticleModule::initMySQL()
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
        XX(ArticleInfoDao, "article")
        XX(CategoryInfoDao, "category")
        XX(LabelInfoDao, "label")
        XX(ArticleCategoryRelInfoDao, "article_category_rel")
        XX(ArticleLabelRelInfoDao, "article_label_rel")
        XX(ArticlePraiseRelInfoDao, "article_praise_rel")
#undef XX

        INFO(logger) << "migrate database begin";
#define XX(clazz)                                           \
    if (blog::data::clazz::MigrateTableMySQL(mysql) != 0) { \
        ERROR(logger) << "migrate table mysql failed";      \
        return false;                                       \
    }
        XX(ArticleInfoDao)
        XX(CategoryInfoDao)
        XX(LabelInfoDao)
        XX(ArticleCategoryRelInfoDao)
        XX(ArticleLabelRelInfoDao)
        XX(ArticlePraiseRelInfoDao)
#undef XX
        INFO(logger) << "migrate database end";
    }

    return true;
}

void ArticleModule::registerServlets()
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
        dp->addServlet("/api/v1/article/calendar", XX(ArticleCalendarServlet));
        dp->addServlet("/api/v1/article/admin/lists", XX(ArticleAdminListsServlet));
        dp->addServlet("/api/v1/article/admin/stats", XX(ArticleAdminStatsServlet));
        dp->addServlet("/api/v1/article/create", XX(ArticleCreateServlet));
        dp->addServlet("/api/v1/article/detail", XX(ArticleDetailServlet));
        dp->addServlet("/api/v1/article/publish", XX(ArticlePublishServlet));
        dp->addServlet("/api/v1/article/query", XX(ArticleQueryServlet));
        dp->addServlet("/api/v1/article/search", XX(ArticleSearchServlet));
        dp->addServlet("/api/v1/article/list_by_label", XX(ArticleListByLabelServlet));
        dp->addServlet("/api/v1/article/list_by_category", XX(ArticleListByCategoryServlet));
        dp->addServlet("/api/v1/article/delete", XX(ArticleDeleteServlet));
        dp->addServlet("/api/v1/article/verify", XX(ArticleVerifyServlet));
        dp->addServlet("/api/v1/article/update", XX(ArticleUpdateServlet));
        dp->addServlet("/api/v1/article/update_category", XX(ArticleUpdateCategoryServlet));
        dp->addServlet("/api/v1/article/switch_state", XX(ArticleSwitchStateServlet));
        dp->addServlet("/api/v1/article/is_praising", XX(ArticleIsPraisingServlet));
        dp->addServlet("/api/v1/article/praise", XX(ArticlePraiseServlet));
        dp->addServlet("/api/v1/article/praise/list", XX(ArticlePraiseListServlet));
        dp->addServlet("/api/v1/article/view", XX(ArticleViewServlet));
        dp->addServlet("/api/v1/article/ai-summary", XX(ArticleAISummaryServlet));
        dp->addServlet("/api/v1/category/create", XX(CategoryCreateServlet));
        dp->addServlet("/api/v1/category/delete", XX(CategoryDeleteServlet));
        dp->addServlet("/api/v1/category/query", XX(CategoryQueryServlet));
        dp->addServlet("/api/v1/category/admin/create", XX(CategoryAdminCreateServlet));
        dp->addServlet("/api/v1/category/admin/delete", XX(CategoryAdminDeleteServlet));
        dp->addServlet("/api/v1/category/admin/query", XX(CategoryAdminQueryServlet));
        dp->addServlet("/api/v1/label/create", XX(LabelCreateServlet));
        dp->addServlet("/api/v1/label/delete", XX(LabelDeleteServlet));
        dp->addServlet("/api/v1/label/query", XX(LabelQueryServlet));
#undef XX
    }
}

} // namespace blog

extern "C" {

chen::Module* CreateModule()
{
    chen::Module* module = new blog::ArticleModule;
    INFO(blog::logger) << "ArticleModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(blog::logger) << "ArticleModule::DestroyModule";
    delete module;
}
}
