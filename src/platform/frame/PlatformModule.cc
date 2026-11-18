#include "platform/frame/PlatformModule.hpp"

#include <exception>

#include <chen/ds/event_bus.h>
#include <chen/log/log.h>
#include <chen/util/macro.h>

#include "common/event/Events.hpp"
#include "common/frame/ServiceBootstrap.hpp"
#include "common/frame/ServletRegistrar.hpp"
#include "common/include/Tables.hpp"
#include "common/metrics/ServiceMetrics.hpp"
#include "platform/include/Servlets.hpp"
#include "platform/logic/BlogMsgFromBot.hpp"
#include "platform/logic/ChunkUpload.hpp"
#include "platform/manager/NotificationManager.hpp"
#include "platform/manager/OrganizationRepoManager.hpp"
#include "platform/manager/OrganizationRepoPrManager.hpp"
#include "platform/manager/WSConnectionManager.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

PlatformModule::PlatformModule()
    : Module("PlatformService", "1.0.8", "platform_module")
{
}

bool PlatformModule::onLoad()
{
    return BootstrapOnLoad("PlatformService");
}

bool PlatformModule::onUnload()
{
    INFO(logger) << "onUnload";
    BootstrapOnUnload();
    return true;
}

bool PlatformModule::onDeactivate()
{
    INFO(logger) << "onDeactivate";
    WSMgr::GetInstance()->closeAllConnections();
    return true;
}

void PlatformModule::onTick()
{
    static uint32_t s_sync_tick = 0;

    try {
        metrics::SampleRates();

        s_sync_tick++;

        NotificationMgr::GetInstance()->cleanupExpiredBroadcasts();
        ChunkUploadMgr::GetInstance()->cleanupExpiredSessions();

        if (s_sync_tick % 5 == 0) {
            CleanupExpiredSessions(3600);
        }

        if (s_sync_tick % 1440 == 0) {
            s_sync_tick = 0;

            OrganizationRepoMgr::GetInstance()->onTick();
            OrganizationRepoPrMgr::GetInstance()->onTick();
        }
    } catch (const std::exception& e) {
        ERROR(logger) << "onTick exception: " << e.what();
    } catch (...) {
        ERROR(logger) << "onTick unknown exception";
    }
}

uint64_t PlatformModule::getTickIntervalMs()
{
    return 60 * 1000;
}

bool PlatformModule::onServerReady()
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
    registerWSServlets();
    registerRPCMethods();

    if (!EventMsgsInit()) {
        ERROR(logger) << "EventMsgsInit failed";
        return false;
    }

    return true;
}

bool PlatformModule::onServerUp()
{
    INFO(logger) << "onServerUp";
    return true;
}

bool PlatformModule::initMySQL()
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
        XX(AssignmentInfoDao, "assignment")
        XX(OrganizationApplyInfoDao, "organization_apply")
        XX(OrganizationInfoDao, "organization")
        XX(OrganizationRepoPrsInfoDao, "organization_repo_prs")
        XX(OrganizationReposInfoDao, "organization_repos")
        XX(OrganizationUserRelInfoDao, "organization_user_rel")
        XX(AssignmentOrganizationRelInfoDao, "assignment_organization_rel")
        XX(AssignmentUserRelInfoDao, "assignment_user_rel")
        XX(ResourceInfoDao, "resource")
        XX(ChunkUploadInfoDao, "chunk_upload")
        XX(NotificationInfoDao, "notification")
        XX(BackupRecordInfoDao, "backup_record")
        XX(ExportRecordInfoDao, "export_record")
        XX(ConversationInfoDao, "conversation")
        XX(ConversationMessageInfoDao, "conversation_message")
        XX(RequirementInfoDao, "requirement")
        XX(BugInfoDao, "bug")
        XX(TaskInfoDao, "task")
        XX(SystemSettingsInfoDao, "system_settings")
        XX(HelpFaqsInfoDao, "help_faqs")
        XX(UserFeedbackInfoDao, "user_feedback")
#undef XX

        INFO(logger) << "migrate database begin";
#define XX(clazz)                                           \
    if (blog::data::clazz::MigrateTableMySQL(mysql) != 0) { \
        ERROR(logger) << "migrate table mysql failed";      \
        return false;                                       \
    }
        XX(AssignmentInfoDao)
        XX(OrganizationApplyInfoDao)
        XX(OrganizationInfoDao)
        XX(OrganizationRepoPrsInfoDao)
        XX(OrganizationReposInfoDao)
        XX(OrganizationUserRelInfoDao)
        XX(AssignmentOrganizationRelInfoDao)
        XX(AssignmentUserRelInfoDao)
        XX(ResourceInfoDao)
        XX(ChunkUploadInfoDao)
        XX(NotificationInfoDao)
        XX(BackupRecordInfoDao)
        XX(ExportRecordInfoDao)
        XX(ConversationInfoDao)
        XX(ConversationMessageInfoDao)
        XX(RequirementInfoDao)
        XX(BugInfoDao)
        XX(TaskInfoDao)
        XX(HelpFaqsInfoDao)
        XX(UserFeedbackInfoDao)
#undef XX
        INFO(logger) << "migrate database end";
    }

    return true;
}

void PlatformModule::registerServlets()
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
        dp->addServlet("/api/v1/notification/send", XX(NotificationSendServlet));
        dp->addServlet("/api/v1/notification/list", XX(NotificationListServlet));
        dp->addServlet("/api/v1/broadcast/list", XX(BroadcastListServlet));
        dp->addServlet("/api/v1/broadcast/close", XX(BroadcastCloseServlet));
        dp->addServlet("/api/v1/notification/unread_count", XX(NotificationUnreadCountServlet));
        dp->addServlet("/api/v1/notification/read", XX(NotificationReadServlet));
        dp->addServlet("/api/v1/notification/read_all", XX(NotificationReadAllServlet));
        dp->addServlet("/api/v1/messages/conversations", XX(ConversationServlet));
        dp->addServlet("/api/v1/messages/conversations/:id", XX(ConversationMessageServlet));
        dp->addServlet("/api/v1/messages/conversations/:id/read", XX(ConversationReadServlet));
        dp->addServlet("/api/v1/messages/conversations/:id/delete", XX(ConversationDeleteServlet));
        dp->addServlet("/api/v1/admin/dashboard/stats", XX(DashboardStatsServlet));
        dp->addServlet("/api/v1/admin/dashboard/trend", XX(DashboardTrendServlet));
        dp->addServlet("/api/v1/admin/dashboard/activities", XX(DashboardActivitiesServlet));
        dp->addServlet("/api/v1/admin/dashboard/recent-users", XX(DashboardRecentUsersServlet));
        dp->addServlet("/api/v1/admin/database/stats", XX(DatabaseStatsServlet));
        dp->addServlet("/api/v1/admin/database/backups", XX(BackupListServlet));
        dp->addServlet("/api/v1/admin/database/backups/create", XX(BackupCreateServlet));
        dp->addServlet("/api/v1/admin/database/backups/:id/delete", XX(BackupDeleteServlet));
        dp->addServlet("/api/v1/admin/database/backups/:id/download", XX(BackupDownloadServlet));
        dp->addServlet("/api/v1/admin/database/exports", XX(ExportListServlet));
        dp->addServlet("/api/v1/admin/database/exports/create", XX(ExportCreateServlet));
        dp->addServlet("/api/v1/admin/database/exports/:id/delete", XX(ExportDeleteServlet));
        dp->addServlet("/api/v1/admin/database/exports/:id/download", XX(ExportDownloadServlet));
        dp->addServlet("/api/v1/admin/database/cleanup", XX(CleanupServlet));
        dp->addServlet("/api/v1/error-codes", XX(ErrorCodesServlet));
        dp->addServlet("/api/v1/site/settings", XX(SiteSettingsServlet));
        dp->addServlet("/api/v1/site/status", XX(SiteStatusServlet));
        dp->addServlet("/api/v1/admin/settings", XX(SystemSettingsServlet));
        dp->addServlet("/api/v1/admin/settings/upload", XX(SystemSettingsUploadServlet));
        dp->addServlet("/api/v1/help/faqs", XX(FaqsServlet));
        dp->addServlet("/api/v1/help/feedback", XX(FeedbackServlet));
        dp->addServlet("/api/v1/admin/faq/delete", XX(AdminFaqDeleteServlet));
        dp->addServlet("/api/v1/admin/faq/edit", XX(AdminFaqEditServlet));
        dp->addServlet("/api/v1/admin/feedback/list", XX(AdminFeedbackListServlet));
        dp->addServlet("/api/v1/admin/feedback/delete", XX(AdminFeedbackDeleteServlet));
        dp->addServlet("/api/v1/admin/feedback/convert", XX(AdminFeedbackConvertServlet));
        dp->addServlet("/api/v1/stats", XX(StatsServlet));
        dp->addServlet("/api/v1/admin/metrics", XX(MetricsAdminServlet));
        dp->addServlet("/api/v1/file/upload", XX(FileUploadServlet));
        dp->addServlet("/api/v1/file/download", XX(FileDownloadServlet));
        dp->addServlet("/api/v1/upload/init", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/chunk", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/complete", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/cancel", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/status", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/assignment/admin/lists", XX(AssignmentAdminListsServlet));
        dp->addServlet("/api/v1/assignment/admin/stats", XX(AssignmentAdminStatsServlet));
        dp->addServlet("/api/v1/assignment/create", XX(AssignmentCreateServlet));
        dp->addServlet("/api/v1/assignment/delete", XX(AssignmentDeleteServlet));
        dp->addServlet("/api/v1/assignment/detail", XX(AssignmentDetailServlet));
        dp->addServlet("/api/v1/assignment/submission/list", XX(AssignmentSubmissionListServlet));
        dp->addServlet("/api/v1/organization/admin/lists", XX(OrganizationAdminListsServlet));
        dp->addServlet("/api/v1/organization/admin/stats", XX(OrganizationAdminStatsServlet));
        dp->addServlet("/api/v1/organization/create", XX(OrganizationCreateServlet));
        dp->addServlet("/api/v1/organization/delete", XX(OrganizationDeleteServlet));
        dp->addServlet("/api/v1/organization/detail", XX(OrganizationDetailServlet));
        dp->addServlet("/api/v1/organization/join", XX(OrganizationJoinServlet));
        dp->addServlet("/api/v1/organization/join_check", XX(OrganizationJoinCheckServlet));
        dp->addServlet("/api/v1/organization/list", XX(OrganizationListServlet));
        dp->addServlet("/api/v1/organization/user_list", XX(OrganizationUserListServlet));
        dp->addServlet("/api/v1/organization/user_switch_role", XX(OrganizationUserSwitchRoleServlet));
        dp->addServlet("/api/v1/organization/user_kick", XX(OrganizationUserKickServlet));
        dp->addServlet("/api/v1/organization/assignment_create", XX(AssignmentOrganizationCreateServlet));
        dp->addServlet("/api/v1/organization/assignment_list", XX(OrganizationAssignmentListServlet));
        dp->addServlet("/api/v1/organization/apply-create", XX(OrganizationApplyCreateServlet));
        dp->addServlet("/api/v1/organization/apply-list", XX(OrganizationApplyListServlet));
        dp->addServlet("/api/v1/organization/apply-review", XX(OrganizationApplyReviewServlet));
        dp->addServlet("/api/v1/organization/my-applies", XX(OrganizationMyAppliesServlet));
        dp->addServlet("/api/v1/organization/stats", XX(OrganizationStatsServlet));
        dp->addServlet("/api/v1/organization/repos/prs", XX(OrganizationRepoPrsListServlet));
        dp->addServlet("/api/v1/organization/repos/prs/delete", XX(OrganizationRepoPrsDeleteServlet));
        dp->addServlet("/api/v1/organization/repos/prs/sync", XX(OrganizationRepoPrsSyncServlet));
        dp->addServlet("/api/v1/organization/repos/stats", XX(OrganizationReposStatsServlet));
        dp->addServlet("/api/v1/organization/repos/token", XX(OrganizationReposTokenServlet));
        dp->addServlet("/api/v1/organization/repos", XX(OrganizationReposListServlet));
        dp->addServlet("/api/v1/organization/repos/add", XX(OrganizationReposAddServlet));
        dp->addServlet("/api/v1/organization/repos/sync", XX(OrganizationReposSyncServlet));
        dp->addServlet("/api/v1/organization/repos/delete", XX(OrganizationReposDeleteServlet));
        dp->addServlet("/api/v1/rd/check_access", XX(RdCheckAccessServlet));
        dp->addServlet("/api/v1/rd/requirements", XX(RdRequirementServlet));
        dp->addServlet("/api/v1/rd/requirements/edit", XX(RdRequirementEditServlet));
        dp->addServlet("/api/v1/rd/requirements/detail", XX(RdRequirementDetailServlet));
        dp->addServlet("/api/v1/rd/requirements/delete", XX(RdRequirementDeleteServlet));
        dp->addServlet("/api/v1/rd/bugs", XX(RdBugServlet));
        dp->addServlet("/api/v1/rd/bugs/edit", XX(RdBugEditServlet));
        dp->addServlet("/api/v1/rd/bugs/detail", XX(RdBugDetailServlet));
        dp->addServlet("/api/v1/rd/bugs/delete", XX(RdBugDeleteServlet));
        dp->addServlet("/api/v1/rd/tasks", XX(RdTaskServlet));
        dp->addServlet("/api/v1/rd/tasks/edit", XX(RdTaskEditServlet));
        dp->addServlet("/api/v1/rd/tasks/detail", XX(RdTaskDetailServlet));
        dp->addServlet("/api/v1/rd/tasks/delete", XX(RdTaskDeleteServlet));
        dp->addServlet("/api/v1/rd/trends", XX(RdTrendServlet));
        dp->addServlet("/api/v1/rd/stats", XX(RdStatsServlet));
        dp->addServlet("/api/v1/rd/pr-distribution", XX(RdPrDistributionServlet));
        dp->addServlet("/api/v1/rd/my-tickets", XX(RdMyTicketsServlet));
        dp->addServlet("/api/v1/rd/organizations", XX(RdOrganizationsServlet));
        dp->addServlet("/api/v1/rd/organizations/members", XX(RdOrganizationMembersServlet));
#undef XX
    }
}

void PlatformModule::registerWSServlets()
{
    INFO(logger) << "registerWSServlets";

    std::vector<chen::http::WSServer::ptr> ws_servers;
    getAllWSServer(ws_servers);

    if (ws_servers.empty()) {
        ERROR(logger) << "No WS server found";
        return;
    }

    for (auto& i : ws_servers) {
        auto ws = std::dynamic_pointer_cast<chen::http::WSServer>(i);
        ASSERT_RET(ws != nullptr);

        chen::http::ServletDispatch::ptr dp = ws->getWSServletDispatch();
        ASSERT_RET(dp != nullptr);

#define XX(clazz) std::make_shared<servlet::clazz>()
        dp->addServlet("/ws/v1/session", XX(WSGatewayServlet));
#undef XX
    }
}

void PlatformModule::registerRPCMethods()
{
    INFO(logger) << "registerRPCMethods";

    std::vector<chen::rpc::RpcServer::ptr> rpc_servers;
    getAllRpcServer(rpc_servers);

    if (rpc_servers.empty()) {
        ERROR(logger) << "No RPC server found";
        return;
    }

    for (auto& server : rpc_servers) {
        if (!server) {
            continue;
        }
        BlogMsgFromBotInit(server);
    }
}

} // namespace blog

extern "C" {

chen::Module* CreateModule()
{
    chen::Module* module = new blog::PlatformModule;
    INFO(blog::logger) << "PlatformModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(blog::logger) << "PlatformModule::DestroyModule";
    delete module;
}
}
