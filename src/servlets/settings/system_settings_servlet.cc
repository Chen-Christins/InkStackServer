#include "system_settings_servlet.h"

#include <chen/log/log.h>

#include "../../manager/system_settings_manager.h"
#include "../../manager/user_manager.h"

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

SystemSettingsServlet::SystemSettingsServlet()
    : BlogLoginedServlet("SystemSettingsServlet")
{
}

static void BuildSettingsJson(Json::Value& json, data::SystemSettingsInfo::ptr info)
{
    json["id"] = info->getId();
    json["siteName"] = info->getSiteName();
    json["siteDescription"] = info->getSiteDescription();
    json["siteKeywords"] = info->getSiteKeywords();
    json["siteIcon"] = info->getSiteIcon();
    json["siteLogo"] = info->getSiteLogo();
    json["favicon"] = info->getFavicon();
    json["adminEmail"] = info->getAdminEmail();
    json["timezone"] = info->getTimezone();
    json["language"] = info->getLanguage();

    json["smtpHost"] = info->getSmtpHost();
    json["smtpPort"] = info->getSmtpPort();
    json["smtpUsername"] = info->getSmtpUsername();
    if (!info->getSmtpPassword().empty()) {
        json["smtpPassword"] = "****";
    } else {
        json["smtpPassword"] = "";
    }
    json["smtpEncryption"] = info->getSmtpEncryption();
    json["fromEmail"] = info->getFromEmail();
    json["fromName"] = info->getFromName();
    json["replyTo"] = info->getReplyTo();

    json["enableRegistration"] = info->getEnableRegistration() ? true : false;
    json["requireEmailVerification"] = info->getRequireEmailVerification() ? true : false;
    json["allowComments"] = info->getAllowComments() ? true : false;
    json["moderateComments"] = info->getModerateComments() ? true : false;
    json["maxFileSize"] = info->getMaxFileSize();
    json["allowedFileTypes"] = info->getAllowedFileTypes();
    json["sessionTimeout"] = info->getSessionTimeout();
    json["maintenanceMode"] = info->getMaintenanceMode() ? true : false;
    json["backupSchedule"] = info->getBackupSchedule();
    json["createdAt"] = info->getCreatedAt();
    json["updatedAt"] = info->getUpdatedAt();
}

int32_t SystemSettingsServlet::handle(
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
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user || user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        auto method = request->getMethod();

        if (method == chen::http::HttpMethod::GET) {
            auto settings = SystemSettingsMgr::GetInstance()->get();
            if (!settings) {
                result->setErrno(errcode::SETTINGS_NOT_LOADED);
                break;
            }
            Json::Value data;
            BuildSettingsJson(data, settings);
            result->setData(data);
        } else if (method == chen::http::HttpMethod::PUT) {
            auto info = SystemSettingsMgr::GetInstance()->get();
            if (!info) {
                result->setErrno(errcode::SETTINGS_NOT_LOADED);
                break;
            }

            std::string param;

            param = request->getParam("siteName");
            if (!param.empty()) {
                info->setSiteName(param);
            }
            param = request->getParam("siteDescription");
            if (!param.empty()) {
                info->setSiteDescription(param);
            }
            param = request->getParam("siteKeywords");
            if (!param.empty()) {
                info->setSiteKeywords(param);
            }
            param = request->getParam("siteIcon");
            if (!param.empty()) {
                info->setSiteIcon(param);
            }
            param = request->getParam("siteLogo");
            if (!param.empty()) {
                info->setSiteLogo(param);
            }
            param = request->getParam("favicon");
            if (!param.empty()) {
                info->setFavicon(param);
            }
            param = request->getParam("adminEmail");
            if (!param.empty()) {
                info->setAdminEmail(param);
            }
            param = request->getParam("timezone");
            if (!param.empty()) {
                info->setTimezone(param);
            }
            param = request->getParam("language");
            if (!param.empty()) {
                info->setLanguage(param);
            }

            param = request->getParam("smtpHost");
            if (!param.empty()) {
                info->setSmtpHost(param);
            }
            int32_t smtpPort = request->getParamAs<int32_t>("smtpPort", 0);
            if (smtpPort > 0) {
                info->setSmtpPort(smtpPort);
            }
            param = request->getParam("smtpUsername");
            if (!param.empty()) {
                info->setSmtpUsername(param);
            }
            param = request->getParam("smtpPassword");
            if (!param.empty() && param != "****") {
                info->setSmtpPassword(param);
            }
            param = request->getParam("smtpEncryption");
            if (!param.empty()) {
                info->setSmtpEncryption(param);
            }
            param = request->getParam("fromEmail");
            if (!param.empty()) {
                info->setFromEmail(param);
            }
            param = request->getParam("fromName");
            if (!param.empty()) {
                info->setFromName(param);
            }
            param = request->getParam("replyTo");
            if (!param.empty()) {
                info->setReplyTo(param);
            }

            param = request->getParam("enableRegistration");
            if (!param.empty()) {
                info->setEnableRegistration(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("requireEmailVerification");
            if (!param.empty()) {
                info->setRequireEmailVerification(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("allowComments");
            if (!param.empty()) {
                info->setAllowComments(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("moderateComments");
            if (!param.empty()) {
                info->setModerateComments(param == "true" || param == "1" ? 1 : 0);
            }
            int32_t maxFileSize = request->getParamAs<int32_t>("maxFileSize", 0);
            if (maxFileSize > 0) {
                info->setMaxFileSize(maxFileSize);
            }
            param = request->getParam("allowedFileTypes");
            if (!param.empty()) {
                info->setAllowedFileTypes(param);
            }
            int32_t sessionTimeout = request->getParamAs<int32_t>("sessionTimeout", 0);
            if (sessionTimeout > 0) {
                info->setSessionTimeout(sessionTimeout);
            }
            param = request->getParam("maintenanceMode");
            if (!param.empty()) {
                info->setMaintenanceMode(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("backupSchedule");
            if (!param.empty()) {
                info->setBackupSchedule(param);
            }

            info->setUpdatedAt(time(0));

            if (!SystemSettingsMgr::GetInstance()->update(info)) {
                result->setErrno(errcode::SETTINGS_SAVE_FAILED);
                break;
            }
            Json::Value data;
            BuildSettingsJson(data, info);
            result->setData(data);
        } else {
            result->setErrno(errcode::METHOD_NOT_ALLOWED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace blog::servlet
