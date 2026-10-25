#include "blog_module.h"

#include <chen/application.h>
#include <chen/config/config.h>
#include <chen/db/mysql.h>
#include <chen/db/query_builder.h>
#include <chen/db/redis.h>
#include <chen/ds/event_bus.h>
#include <chen/http/http_server.h>
#include <chen/http/session_data.h>
#include <chen/http/ws_server.h>
#include <chen/http/ws_servlet.h>
#include <chen/log/log.h>
#include <chen/util/util.h>

#include <exception>
#include <memory>
#include <ranges>

#include "./chunk_upload.h"
#include "./crash_handler.h"
#include "./event/events.h"
#include "./i18n/i18n.h"
#include "./include/managers.h"
#include "./include/servlets.h"
#include "./include/tables.h"
#include "./index.h"
#include "./logic/BlogMsgFromBot.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

static chen::ConfigVar<std::string>::ptr g_work_path = chen::Config::Lookup<std::string>("server.work_path", std::string(""), "server work path");

BlogModule::BlogModule()
    : Module("Blog", "1.0.8", "blog_module")
{
}

bool BlogModule::onLoad()
{
    INFO(logger) << "onLoad version=" << getVersion();

    if (g_work_path->getValue().empty()) {
        ERROR(logger) << "work path is empty";
        return false;
    }

    std::string crash_log = g_work_path->getValue() + "/logs/crash.log";
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

bool BlogModule::onUnload()
{
    INFO(logger) << "onUnload";

    chen::EventBusMgr::GetInstance()->clearAll();

    ArticleMgr::GetInstance()->stop();

    chen::FoxThreadMgr::GetInstance()->stop();

    UninstallCrashHandler();

    return true;
}

bool BlogModule::onDeactivate()
{
    INFO(logger) << "onDeactivate";

    ArticleMgr::GetInstance()->stop();

    NotificationMgr::GetInstance()->closeAllConnections();

    MessageMgr::GetInstance()->closeAllChatConnections();

    return true;
}

void BlogModule::onTick()
{
    static int s_pr_sync_tick = 0;

    try {
        if (chen::TimeUtil::IsZeroOfDay()) {
            s_pr_sync_tick = 0;

            OrganizationRepoMgr::GetInstance()->onTick();

            OrganizationRepoPrMgr::GetInstance()->onTick();
        }

        ArticleMgr::GetInstance()->onTick();

        NotificationMgr::GetInstance()->cleanupExpiredBroadcasts();

        ChunkUploadMgr::GetInstance()->cleanupExpiredSessions();

        if (s_pr_sync_tick % 5 == 0) {
            chen::http::SessionDataMgr::GetInstance()->check(3600);
        }

        if (s_pr_sync_tick % 60 == 0) {
            INFO(logger) << "module status: \n" << Module::statusString();
        }

        s_pr_sync_tick++;
    } catch (const std::exception& e) {
        ERROR(logger) << "onTick exception: " << e.what();
    } catch (...) {
        ERROR(logger) << "onTick unknown exception";
    }
}

uint64_t BlogModule::getTickIntervalMs()
{
    // 设置定时器间隔为1分钟
    return 60 * 1000;
}

bool BlogModule::onServerReady()
{
    INFO(logger) << "onServerReady";

    chen::EventBusMgr::GetInstance()->clearAll();

    if (!initMySQL()) {
        ERROR(logger) << "initDB failed";
        return false;
    }

    ArticleMgr::GetInstance()->start();

    if (!IndexMgr::GetInstance()->initFromFile()) {
        INFO(logger) << "index load failed";
        // return false;
    }

    std::string errorsPath = g_work_path->getValue() + "/errors.json";
    if (!ErrorCodeMgr::GetInstance()->load(errorsPath)) {
        ERROR(logger) << "Failed to load error codes from " << errorsPath;
        return false;
    }

    registerServlets();

    registerWSServlets();

    registerRPCMethods();

    if (!EventMsgsInit()) {
        ERROR(logger) << "EventMsgsInit failed";
        return false;
    }

    return true;
}

bool BlogModule::onServerUp()
{
    INFO(logger) << "onServerUp";

    UserMgr::GetInstance()->ensureSuperAdmin();

    BadgeMgr::GetInstance()->ensureDefaults();

    return true;
}

bool BlogModule::initMySQL()
{
    INFO(logger) << "initMySQL";

    const auto& mysql_dbs = g_mysql_dbs->getValue();
    for (const auto& params : mysql_dbs | std::views::values) {
        auto mysql = std::make_shared<chen::MySQL>(params);
        if (!mysql->connect()) {
            ERROR(logger) << "connect mysql failed";
            return false;
        }

#define XX(class_name, table_name)                             \
    if (data::class_name::CreateTableMySQL(mysql)) {           \
        ERROR(logger) << "create " table_name " table failed"; \
        return false;                                          \
    }
        XX(EmailVerificationInfoDao, "email_verification")
        XX(UserInfoDao, "user")
        XX(ArticleInfoDao, "article")
        XX(CategoryInfoDao, "category")
        XX(LabelInfoDao, "label")
        XX(ArticleCategoryRelInfoDao, "article_category_rel")
        XX(ArticleLabelRelInfoDao, "article_label_rel")
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
        XX(UserFollowRelInfoDao, "user_follow_rel")
        XX(ArticlePraiseRelInfoDao, "article_praise_rel")
        XX(BackupRecordInfoDao, "backup_record")
        XX(ExportRecordInfoDao, "export_record")
        XX(CommentInfoDao, "comment")
        XX(CommentPraiseRelInfoDao, "comment_praise_rel")
        XX(ConversationInfoDao, "conversation")
        XX(ConversationMessageInfoDao, "conversation_message")
        XX(RequirementInfoDao, "requirement")
        XX(BugInfoDao, "bug")
        XX(TaskInfoDao, "task")
        XX(SystemSettingsInfoDao, "system_settings")
        XX(BadgeInfoDao, "badge")
        XX(HelpFaqsInfoDao, "help_faqs")
        XX(UserFeedbackInfoDao, "user_feedback")
        XX(UserAiConfigInfoDao, "user_ai_config")
        XX(UserLoginDeviceInfoDao, "user_login_device")
        XX(UserOauthInfoDao, "user_oauth")
        XX(UserPasskeyInfoDao, "user_passkey")
        XX(UserRecoveryCodeInfoDao, "user_recovery_code")
#undef XX

        INFO(logger) << "migrate database begin";
#define XX(clazz)                                           \
    if (blog::data::clazz::MigrateTableMySQL(mysql) != 0) { \
        ERROR(logger) << "migrate table mysql failed";      \
        return false;                                       \
    }
        XX(EmailVerificationInfoDao)
        XX(UserInfoDao)
        XX(ArticleInfoDao)
        XX(CategoryInfoDao)
        XX(LabelInfoDao)
        XX(ArticleCategoryRelInfoDao)
        XX(ArticleLabelRelInfoDao)
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
        XX(UserFollowRelInfoDao)
        XX(ArticlePraiseRelInfoDao)
        XX(CommentInfoDao)
        XX(CommentPraiseRelInfoDao)
        XX(ConversationInfoDao)
        XX(ConversationMessageInfoDao)
        XX(BackupRecordInfoDao)
        XX(ExportRecordInfoDao)
        XX(RequirementInfoDao)
        XX(BugInfoDao)
        XX(TaskInfoDao)
        XX(BadgeInfoDao)
        XX(HelpFaqsInfoDao)
        XX(UserFeedbackInfoDao)
        XX(UserAiConfigInfoDao)
        XX(UserLoginDeviceInfoDao)
        XX(UserOauthInfoDao)
        XX(UserPasskeyInfoDao)
        XX(UserRecoveryCodeInfoDao)
#undef XX
        INFO(logger) << "migrate database end";
    }

    return true;
}

void BlogModule::registerServlets()
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

        const auto dp = hs->getServletDispatch();
        ASSERT_RET(hs != nullptr);

#define XX(clazz) std::make_shared<servlet::clazz>()
        // 用户相关
        dp->addServlet("/api/v1/user/send_code", XX(UserSendCodeServlet));
        dp->addServlet("/api/v1/user/create", XX(UserCreateServlet));
        dp->addServlet("/api/v1/user/login", XX(UserLoginServlet));
        dp->addServlet("/api/v1/user/info", XX(UserInfoServlet));
        dp->addServlet("/api/v1/user/list", XX(UserListServlet));
        dp->addServlet("/api/v1/user/logout", XX(UserLogoutServlet));
        dp->addServlet("/api/v1/user/refresh_token", XX(UserRefreshTokenServlet));
        dp->addServlet("/api/v1/user/device/list", XX(UserDeviceListServlet));
        dp->addServlet("/api/v1/user/device/kick", XX(UserDeviceKickServlet));
        dp->addServlet("/api/v1/user/forget_passwd", XX(UserResetPasswdServlet));
        dp->addServlet("/api/v1/user/exists", XX(UserExistsServlet));
        dp->addServlet("/api/v1/user/update", XX(UserUpdateServlet));
        dp->addServlet("/api/v1/user/query", XX(UserQueryServlet));
        dp->addServlet("/api/v1/user/stats", XX(UserStatsServlet));
        dp->addServlet("/api/v1/user/achievements", XX(UserAchievementsServlet));
        dp->addServlet("/api/v1/user/ai-config", XX(UserAIConfigServlet));
        dp->addServlet("/api/v1/user/admin/create", XX(UserAdminCreateServlet));
        dp->addServlet("/api/v1/user/admin/delete", XX(UserAdminDeleteServlet));
        dp->addServlet("/api/v1/user/admin/recover", XX(UserAdminRecoverServlet));
        dp->addServlet("/api/v1/user/admin/reset_passwd", XX(UserAdminResetPasswdServlet));
        dp->addServlet("/api/v1/user/admin/lists", XX(UserAdminListsServlet));
        dp->addServlet("/api/v1/user/admin/stats", XX(UserAdminStatsServlet));
        dp->addServlet("/api/v1/user/admin/update", XX(UserAdminUpdateServlet));
        dp->addServlet("/api/v1/user/admin/detail", XX(UserAdminDetailServlet));
        dp->addServlet("/api/v1/user/organization/list", XX(UserOrganizationListServlet));
        dp->addServlet("/api/v1/user/assignment/list", XX(UserAssignmentListServlet));
        dp->addServlet("/api/v1/user/is_following", XX(UserIsFollowingServlet));
        dp->addServlet("/api/v1/user/follow", XX(UserFollowServlet));
        dp->addServlet("/api/v1/user/unfollow", XX(UserUnfollowServlet));
        dp->addServlet("/api/v1/user/following/list", XX(UserFollowingListServlet));
        dp->addServlet("/api/v1/user/follower/list", XX(UserFollowerListServlet));
        dp->addServlet("/api/v1/user/mutual_following/list", XX(UserMutualFollowingListServlet));
        // 双因素认证相关
        dp->addServlet("/api/v1/user/2fa/enable", XX(User2faEnableServlet));
        dp->addServlet("/api/v1/user/2fa/confirm", XX(User2faConfirmServlet));
        dp->addServlet("/api/v1/user/2fa/disable", XX(User2faDisableServlet));
        dp->addServlet("/api/v1/user/2fa/verify", XX(User2faVerifyServlet));
        dp->addServlet("/api/v1/user/2fa/reset", XX(User2faResetServlet));
        dp->addServlet("/api/v1/user/2fa/recovery", XX(User2faRecoveryServlet));
        // 第三方登录相关
        dp->addServlet("/api/v1/user/oauth/github", XX(UserOauthGithubServlet));
        dp->addServlet("/api/v1/user/oauth/github/callback", XX(UserOauthGithubCallbackServlet));
        // Passkey 登录 / 管理相关
        dp->addServlet("/api/v1/auth/passkey/login/begin", XX(PasskeyLoginBeginServlet));
        dp->addServlet("/api/v1/auth/passkey/login/finish", XX(PasskeyLoginFinishServlet));
        dp->addServlet("/api/v1/auth/passkey/list", XX(PasskeyListServlet));
        dp->addServlet("/api/v1/auth/passkey/delete", XX(PasskeyDeleteServlet));
        // Passkey 注册相关
        dp->addServlet("/api/v1/auth/passkey/register/begin", XX(PasskeyRegisterBeginServlet));
        dp->addServlet("/api/v1/auth/passkey/register/finish", XX(PasskeyRegisterFinishServlet));
        // 邮箱验证码恢复登录
        dp->addServlet("/api/v1/auth/email/recovery", XX(EmailRecoveryServlet));
        // 通知相关
        dp->addServlet("/api/v1/notification/send", XX(NotificationSendServlet));
        dp->addServlet("/api/v1/notification/list", XX(NotificationListServlet));
        dp->addServlet("/api/v1/broadcast/list", XX(BroadcastListServlet));
        dp->addServlet("/api/v1/broadcast/close", XX(BroadcastCloseServlet));
        dp->addServlet("/api/v1/notification/unread_count", XX(NotificationUnreadCountServlet));
        dp->addServlet("/api/v1/notification/read", XX(NotificationReadServlet));
        dp->addServlet("/api/v1/notification/read_all", XX(NotificationReadAllServlet));
        // 私信相关
        dp->addServlet("/api/v1/messages/conversations", XX(ConversationServlet));
        dp->addServlet("/api/v1/messages/conversations/:id", XX(ConversationMessageServlet));
        dp->addServlet("/api/v1/messages/conversations/:id/read", XX(ConversationReadServlet));
        dp->addServlet("/api/v1/messages/conversations/:id/delete", XX(ConversationDeleteServlet));
        // 文章相关
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
        // 文章评论相关
        dp->addServlet("/api/v1/article/comment/list", XX(CommentListServlet));
        dp->addServlet("/api/v1/article/comment/replies", XX(CommentRepliesServlet));
        dp->addServlet("/api/v1/article/comment/create", XX(CommentCreateServlet));
        dp->addServlet("/api/v1/article/comment/update", XX(CommentUpdateServlet));
        dp->addServlet("/api/v1/article/comment/delete", XX(CommentDeleteServlet));
        dp->addServlet("/api/v1/article/comment/praise", XX(CommentPraiseServlet));
        // 管理端评论相关
        dp->addServlet("/api/v1/admin/comment/list", XX(AdminCommentListServlet));
        dp->addServlet("/api/v1/admin/comment/approve", XX(AdminCommentApproveServlet));
        dp->addServlet("/api/v1/admin/comment/reject", XX(AdminCommentRejectServlet));
        dp->addServlet("/api/v1/admin/comment/spam", XX(AdminCommentSpamServlet));
        dp->addServlet("/api/v1/admin/comment/delete", XX(AdminCommentDeleteServlet));
        dp->addServlet("/api/v1/admin/comment/stats", XX(AdminCommentStatsServlet));
        // 仪表盘相关
        dp->addServlet("/api/v1/admin/dashboard/stats", XX(DashboardStatsServlet));
        dp->addServlet("/api/v1/admin/dashboard/trend", XX(DashboardTrendServlet));
        dp->addServlet("/api/v1/admin/dashboard/activities", XX(DashboardActivitiesServlet));
        dp->addServlet("/api/v1/admin/dashboard/recent-users", XX(DashboardRecentUsersServlet));
        // 数据库管理相关
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
        // 文章分类相关
        dp->addServlet("/api/v1/category/admin/create", XX(CategoryCreateServlet));
        dp->addServlet("/api/v1/category/admin/delete", XX(CategoryDeleteServlet));
        dp->addServlet("/api/v1/category/admin/query", XX(CategoryQueryServlet));
        // 错误码下发（公开接口）
        dp->addServlet("/api/v1/error-codes", XX(ErrorCodesServlet));
        // 站点公开配置
        dp->addServlet("/api/v1/site/settings", XX(SiteSettingsServlet));
        dp->addServlet("/api/v1/site/status", XX(SiteStatusServlet));
        // 系统设置相关
        dp->addServlet("/api/v1/admin/settings", XX(SystemSettingsServlet));
        dp->addServlet("/api/v1/admin/settings/upload", XX(SystemSettingsUploadServlet));
        // 帮助中心
        dp->addServlet("/api/v1/help/faqs", XX(FaqsServlet));
        dp->addServlet("/api/v1/help/feedback", XX(FeedbackServlet));
        // 管理端 - 帮助中心
        dp->addServlet("/api/v1/admin/faq/delete", XX(AdminFaqDeleteServlet));
        dp->addServlet("/api/v1/admin/faq/edit", XX(AdminFaqEditServlet));
        // 管理端 - 反馈管理
        dp->addServlet("/api/v1/admin/feedback/list", XX(AdminFeedbackListServlet));
        dp->addServlet("/api/v1/admin/feedback/delete", XX(AdminFeedbackDeleteServlet));
        dp->addServlet("/api/v1/admin/feedback/convert", XX(AdminFeedbackConvertServlet));
        // 首页统计（公开接口）
        dp->addServlet("/api/v1/stats", XX(StatsServlet));
        // 文章标签相关
        dp->addServlet("/api/v1/label/create", XX(LabelCreateServlet));
        dp->addServlet("/api/v1/label/delete", XX(LabelDeleteServlet));
        dp->addServlet("/api/v1/label/query", XX(LabelQueryServlet));
        // 文件相关
        dp->addServlet("/api/v1/file/upload", XX(FileUploadServlet));
        dp->addServlet("/api/v1/file/download", XX(FileDownloadServlet));
        // 大文件分片上传相关
        dp->addServlet("/api/v1/upload/init", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/chunk", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/complete", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/cancel", XX(ChunkUploadServlet));
        dp->addServlet("/api/v1/upload/status", XX(ChunkUploadServlet));
        // 作业相关
        dp->addServlet("/api/v1/assignment/admin/lists", XX(AssignmentAdminListsServlet));
        dp->addServlet("/api/v1/assignment/admin/stats", XX(AssignmentAdminStatsServlet));
        dp->addServlet("/api/v1/assignment/create", XX(AssignmentCreateServlet));
        dp->addServlet("/api/v1/assignment/delete", XX(AssignmentDeleteServlet));
        dp->addServlet("/api/v1/assignment/detail", XX(AssignmentDetailServlet));
        dp->addServlet("/api/v1/assignment/submission/list", XX(AssignmentSubmissionListServlet));
        // 组织相关
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
        // R&D 平台相关（新版统一 API）
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

void BlogModule::registerWSServlets()
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
        dp->addServlet("/ws/v1/notification", XX(NotifyServlet));
        dp->addServlet("/ws/v1/presence", XX(PresenceServlet));
        dp->addServlet("/ws/v1/messages", XX(MessageWSServlet));
#undef XX
    }
}

void BlogModule::registerRPCMethods()
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
    chen::Module* module = new blog::BlogModule;
    INFO(blog::logger) << "BlogModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(blog::logger) << "BlogModule::DestroyModule";
    delete module;
}
}
