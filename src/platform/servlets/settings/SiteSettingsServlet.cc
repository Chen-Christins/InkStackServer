#include "platform/servlets/settings/SiteSettingsServlet.hpp"

#include "platform/manager/SystemSettingsManager.hpp"

#include <chen/log/log.h>

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

SiteSettingsServlet::SiteSettingsServlet()
    : BlogServlet("SiteSettingsServlet")
{
}

int32_t SiteSettingsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto settings = SystemSettingsMgr::GetInstance()->get();
        if (!settings) {
            result->setErrno(errcode::SETTINGS_NOT_LOADED);
            break;
        }
        result->set("siteName", settings->getSiteName());
        result->set("siteDescription", settings->getSiteDescription());
        result->set("siteKeywords", settings->getSiteKeywords());
        result->set("siteIcon", settings->getSiteIcon());
        result->set("siteLogo", settings->getSiteLogo());
        result->set("favicon", settings->getFavicon());
        result->set("timezone", settings->getTimezone());
        result->set("language", settings->getLanguage());
        result->set("enableRegistration", settings->getEnableRegistration() ? true : false);
        result->set("maintenanceMode", settings->getMaintenanceMode() ? true : false);
        result->set("allowComments", settings->getAllowComments() ? true : false);
        result->set("moderateComments", settings->getModerateComments() ? true : false);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace blog::servlet
