#include "user/frame/UserModule.hpp"

#include <exception>

#include <chen/ds/event_bus.h>
#include <chen/log/log.h>
#include <chen/util/macro.h>

#include "common/event/Events.hpp"
#include "common/frame/ServiceBootstrap.hpp"
#include "common/frame/ServletRegistrar.hpp"
#include "common/include/Tables.hpp"
#include "common/metrics/ServiceMetrics.hpp"
#include "user/include/Servlets.hpp"
#include "user/manager/BadgeManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

UserModule::UserModule()
    : Module("UserService", "1.0.8", "user_module")
{
}

bool UserModule::onLoad()
{
    return BootstrapOnLoad("UserService");
}

bool UserModule::onUnload()
{
    INFO(logger) << "onUnload";
    BootstrapOnUnload();
    return true;
}

bool UserModule::onDeactivate()
{
    INFO(logger) << "onDeactivate";
    return true;
}

void UserModule::onTick()
{
    static uint32_t s_sync_tick = 0;

    try {
        metrics::SampleRates();

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

uint64_t UserModule::getTickIntervalMs()
{
    return 60 * 1000;
}

bool UserModule::onServerReady()
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

    if (!EventMsgsInit()) {
        ERROR(logger) << "EventMsgsInit failed";
        return false;
    }

    return true;
}

bool UserModule::onServerUp()
{
    INFO(logger) << "onServerUp";

    UserMgr::GetInstance()->ensureSuperAdmin();
    BadgeMgr::GetInstance()->ensureDefaults();

    return true;
}

bool UserModule::initMySQL()
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
        XX(EmailVerificationInfoDao, "email_verification")
        XX(UserInfoDao, "user")
        XX(UserFollowRelInfoDao, "user_follow_rel")
        XX(BadgeInfoDao, "badge")
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
        XX(UserFollowRelInfoDao)
        XX(BadgeInfoDao)
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

void UserModule::registerServlets()
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
        dp->addServlet("/api/v1/user/send_code", XX(UserSendCodeServlet));
        dp->addServlet("/api/v1/user/create", XX(UserCreateServlet));
        dp->addServlet("/api/v1/user/login", XX(UserLoginServlet));
        dp->addServlet("/api/v1/user/info", XX(UserInfoServlet));
        dp->addServlet("/api/v1/user/list", XX(UserListServlet));
        dp->addServlet("/api/v1/user/logout", XX(UserLogoutServlet));
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
        dp->addServlet("/api/v1/user/2fa/enable", XX(User2faEnableServlet));
        dp->addServlet("/api/v1/user/2fa/confirm", XX(User2faConfirmServlet));
        dp->addServlet("/api/v1/user/2fa/disable", XX(User2faDisableServlet));
        dp->addServlet("/api/v1/user/2fa/verify", XX(User2faVerifyServlet));
        dp->addServlet("/api/v1/user/2fa/reset", XX(User2faResetServlet));
        dp->addServlet("/api/v1/user/2fa/recovery", XX(User2faRecoveryServlet));
        dp->addServlet("/api/v1/user/oauth/github", XX(UserOauthGithubServlet));
        dp->addServlet("/api/v1/user/oauth/github/callback", XX(UserOauthGithubCallbackServlet));
        dp->addServlet("/api/v1/auth/passkey/login/begin", XX(PasskeyLoginBeginServlet));
        dp->addServlet("/api/v1/auth/passkey/login/finish", XX(PasskeyLoginFinishServlet));
        dp->addServlet("/api/v1/auth/passkey/list", XX(PasskeyListServlet));
        dp->addServlet("/api/v1/auth/passkey/delete", XX(PasskeyDeleteServlet));
        dp->addServlet("/api/v1/auth/passkey/register/begin", XX(PasskeyRegisterBeginServlet));
        dp->addServlet("/api/v1/auth/passkey/register/finish", XX(PasskeyRegisterFinishServlet));
        dp->addServlet("/api/v1/auth/email/recovery", XX(EmailRecoveryServlet));
#undef XX
    }
}

} // namespace blog

extern "C" {

chen::Module* CreateModule()
{
    chen::Module* module = new blog::UserModule;
    INFO(blog::logger) << "UserModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(blog::logger) << "UserModule::DestroyModule";
    delete module;
}
}
