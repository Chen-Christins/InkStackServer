#include "user_send_code_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../event/event_define.h"
#include "../../manager/system_settings_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

UserSendCodeServlet::UserSendCodeServlet()
    : BlogServlet("UserSendCodeServlet")
{
}

int32_t UserSendCodeServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, agent, "agent");

        if (email.empty() && agent.empty()) {
            result->setErrno(errcode::PARAM_MISSING);
            break;
        }

        if (!IsEmail(email)) {
            result->setErrno(errcode::USER_INVALID_EMAIL);
            break;
        }

        if (type == "1" && blog::UserMgr::GetInstance()->getByEmail(email)) {
            result->setErrno(errcode::USER_EMAIL_EXISTS);
            break;
        }
        if (type == "5") {
            auto recovery_user = blog::UserMgr::GetInstance()->getByEmail(email);
            if (!recovery_user) {
                result->setErrno(errcode::USER_EMAIL_NOT_REGISTER);
                break;
            }
            if (recovery_user->getState() != UserManager::Status::ACTIVE || recovery_user->getIsDeleted()) {
                result->setErrno(errcode::ACCOUNT_INVALID);
                break;
            }
        }

        // IP 限流：每小时每 IP 最多 5 次
        {
            std::string ip = request->getHeader("X-Real-IP");
            if (ip.empty()) {
                ip = session->getRemoteAddressString();
                auto pos = ip.find(':');
                if (pos != std::string::npos) {
                    ip = ip.substr(0, pos);
                }
            }
            auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "INCR code_limit:ip:%s", ip.c_str());
            if (rpy && rpy->integer == 1) {
                chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "EXPIRE code_limit:ip:%s 3600", ip.c_str());
            }
            if (rpy && rpy->integer > 5) {
                result->setErrno(errcode::SEND_CODE_FREQUENT);
                break;
            }
        }

        // 检查 SMTP 配置（在写 DB 之前校验，避免产生无效验证码）
        auto sys_settings = SystemSettingsMgr::GetInstance()->get();
        if (!sys_settings || sys_settings->getSmtpHost().empty()) {
            result->setErrno(errcode::SMTP_NOT_CONFIGURED);
            break;
        }

        // 生成验证码并写入 Redis，10分钟过期
        std::string code = chen::RandomUtil::RandString(6);
        auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "SETEX email:verify:%s:%s 600 %s", type.c_str(), email.c_str(), code.c_str());
        if (!rpy) {
            result->setErrno(errcode::REDIS_OPERATION_FAILED);
            break;
        }

        INFO(logger) << "email=" << email << " type=" << type << " code=" << code;

        std::string site_name = sys_settings->getSiteName();

        // 异步发送邮件，不阻塞请求响应
        // type: 1-注册, 3-重置密码, 5-账号恢复
        bool is_register = (type == "1");
        bool is_recovery = (type == "5");
        std::string title = " " + site_name + (is_register ? " 账号注册验证码" : (is_recovery ? " 账号恢复验证码" : " 重置密码验证码"));
        std::string intro = is_register
            ? ("感谢注册 " + site_name + "，请使用以下验证码完成验证：")
            : (is_recovery ? "您正在恢复账号登录，请使用以下验证码完成验证：" : "您正在重置密码，请使用以下验证码完成验证：");
        std::string code_html = std::string() + "<div style=\"max-width:480px;margin:0 auto;padding:32px 24px;"
            + "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            + "background:#ffffff;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,0.08)\">"
            + "<div style=\"text-align:center;padding-bottom:24px;border-bottom:1px solid #f0f0f0\">"
            + "<h1 style=\"margin:0;font-size:22px;color:#1a1a1a\">" + site_name + "</h1>" + "</div>" + "<div style=\"padding:24px 0\">"
            + "<p style=\"margin:0 0 8px;font-size:15px;color:#555\">您好，</p>"
            + "<p style=\"margin:0 0 24px;font-size:15px;color:#555;line-height:1.6\">" + intro + "</p>"
            + "<div style=\"background:#f7f8fa;border-radius:8px;padding:20px;text-align:center;margin-bottom:24px\">"
            + "<span style=\"font-size:32px;font-weight:700;letter-spacing:6px;color:#1a1a1a;font-family:'Courier New',monospace\">" + code
            + "</span>" + "</div>" + "<p style=\"margin:0;font-size:13px;color:#999\">验证码 10 分钟内有效，请勿转发给他人。</p>" + "</div>"
            + "<div style=\"padding-top:16px;border-top:1px solid #f0f0f0;text-align:center\">"
            + "<p style=\"margin:0;font-size:12px;color:#bbb\">此邮件由系统自动发送，请勿回复。</p>" + "</div>" + "</div>";
        auto mail = chen::EMail::Create(
            sys_settings->getSmtpUsername(), sys_settings->getSmtpPassword(), title, code_html, {email}, {}, {sys_settings->getFromEmail()});
        std::string smtp_host = sys_settings->getSmtpHost();
        int32_t smtp_port = sys_settings->getSmtpPort();

        // 异步触发事件，发送邮件
        {
            EventUserSendCodeData data = {};
            data.email = mail;
            data.smtp_host = smtp_host;
            data.port = smtp_port;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_USER_SEND_CODE, std::move(data));
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
