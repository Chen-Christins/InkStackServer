#include "platform/rpc/PlatformRpcService.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include "protocol_platform.h" // IWYU pragma: keep

#include "common/rpc/RpcEndpoint.hpp"
#include "common/rpc/RpcHubClient.hpp"
#include "platform/manager/AssignmentManager.hpp"
#include "platform/manager/AssignmentOrganizationRelManager.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "platform/manager/SystemSettingsManager.hpp"
#include "platform/manager/WSConnectionManager.hpp"

namespace blog {
namespace rpc {

static chen::Logger::ptr logger = LOG_NAME("system");

namespace {

/**
 * @brief 系统设置实体 -> 跨服务快照
 */
tagSystemSettings ToSettingsSnapshot(const data::SystemSettingsInfo::ptr& settings)
{
    tagSystemSettings out;
    if (!settings) {
        return out;
    }
    out.SiteName = settings->getSiteName();
    out.SiteDescription = settings->getSiteDescription();
    out.SiteKeywords = settings->getSiteKeywords();
    out.SiteLogo = settings->getSiteLogo();
    out.SiteIcon = settings->getSiteIcon();
    out.Favicon = settings->getFavicon();
    out.Language = settings->getLanguage();
    out.Timezone = settings->getTimezone();
    out.EnableRegistration = settings->getEnableRegistration();
    out.RequireEmailVerification = settings->getRequireEmailVerification();
    out.AllowComments = settings->getAllowComments();
    out.ModerateComments = settings->getModerateComments();
    out.MaintenanceMode = settings->getMaintenanceMode();
    out.SessionTimeout = settings->getSessionTimeout();
    out.MaxFileSize = settings->getMaxFileSize();
    out.AllowedFileTypes = settings->getAllowedFileTypes();
    out.SmtpHost = settings->getSmtpHost();
    out.SmtpPort = settings->getSmtpPort();
    out.SmtpUsername = settings->getSmtpUsername();
    out.SmtpPassword = settings->getSmtpPassword();
    out.SmtpEncryption = settings->getSmtpEncryption();
    out.FromEmail = settings->getFromEmail();
    out.FromName = settings->getFromName();
    out.ReplyTo = settings->getReplyTo();
    out.AdminEmail = settings->getAdminEmail();
    out.BackupSchedule = settings->getBackupSchedule();
    out.UpdatedAt = settings->getUpdatedAt();
    return out;
}

/**
 * @brief 把事件消息投递到 WebSocket
 * @details payload 即最终下发的消息体；可选 target 字段指定接收用户，
 *          缺省或 <=0 时广播给全部在线连接。target 不会下发给客户端。
 */
void DispatchWebSocketMessage(const std::string& payload)
{
    Json::Value json;
    if (!chen::JsonUtil::FromString(json, payload) || !json.isObject()) {
        WARN(logger) << "push event ignored, invalid payload";
        return;
    }

    int64_t target = 0;
    if (json.isMember("target") && json["target"].isInt64()) {
        target = json["target"].asInt64();
    }
    if (json.isMember("target")) {
        json.removeMember("target");
    }

    const std::string message = chen::JsonUtil::ToString(json);
    if (target > 0) {
        WSMgr::GetInstance()->sendToUser(target, message);
    } else {
        WSMgr::GetInstance()->broadcast(message);
    }
}

} // namespace

void RegisterPlatformRpcMethods()
{
    auto hub = RpcHubClientMgr::GetInstance();

    hub->registerMethod(PLATFORM_CMD_PING, []() -> int32_t {
        return static_cast<int32_t>(FUNC_PLATFORM);
    });

    hub->registerMethod(PLATFORM_CMD_GET_SETTINGS, []() -> tagSystemSettings {
        return ToSettingsSnapshot(SystemSettingsMgr::GetInstance()->get());
    });

    hub->registerMethod(PLATFORM_CMD_KICK_DEVICE, [](std::string device_id, std::string reason) {
        WSMgr::GetInstance()->closeDevice(device_id, reason);
    });

    hub->registerMethod(PLATFORM_CMD_PUSH_EVENT, [](int32_t event_id, std::string payload) {
        (void)event_id;
        DispatchWebSocketMessage(payload);
    });

    hub->registerMethod(PLATFORM_CMD_LIST_USER_ORGS, [](int64_t uid) -> std::vector<tagUserOrgBrief> {
        std::vector<tagUserOrgBrief> briefs;

        std::vector<data::OrganizationUserRelInfo::ptr> rels;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(rels, uid, -1, true);
        std::sort(rels.begin(), rels.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->getCreateTime() < rhs->getCreateTime();
        });

        for (auto& rel : rels) {
            if (!rel) {
                continue;
            }
            auto org = OrganizationMgr::GetInstance()->get(rel->getOrgId());
            if (!org) {
                continue;
            }
            tagUserOrgBrief brief;
            brief.RelId = rel->getId();
            brief.OrgId = org->getId();
            brief.OrgName = org->getName();
            brief.OrgDescription = org->getDescription();
            brief.Type = org->getType();
            brief.Role = rel->getRole();
            brief.JoinTime = rel->getCreateTime();
            brief.MemberCount =
                OrganizationUserRelMgr::GetInstance()->getMemberCount(org->getId(), OrganizationUserRelManager::Status::APPROVED, true);
            briefs.push_back(brief);
        }
        return briefs;
    });

    hub->registerMethod(PLATFORM_CMD_LIST_USER_ASSIGNMENTS, [](int64_t uid) -> std::vector<tagUserAssignmentBrief> {
        std::vector<tagUserAssignmentBrief> briefs;

        std::vector<data::OrganizationUserRelInfo::ptr> rels;
        OrganizationUserRelMgr::GetInstance()->getOrgByUserId(rels, uid, -1, true);

        for (auto& rel : rels) {
            if (!rel) {
                continue;
            }
            std::vector<data::AssignmentOrganizationRelInfo::ptr> assign_rels;
            AssignmentOrganizationRelMgr::GetInstance()->getByPages(assign_rels, rel->getOrgId(), 0, INT32_MAX, -1, true);
            for (auto& assign_rel : assign_rels) {
                if (!assign_rel) {
                    continue;
                }
                auto assign = AssignmentMgr::GetInstance()->get(assign_rel->getAssignmentId());
                if (!assign || assign->getIsDeleted() != 0 || assign->getStatus() == AssignmentManager::Status::DRAFT) {
                    continue;
                }
                tagUserAssignmentBrief brief;
                brief.Id = assign->getId();
                brief.Name = assign->getName();
                brief.SubjectName = assign->getSubjectName();
                brief.Deadline = assign->getDeadline();
                brief.Status = assign->getStatus();
                brief.Priority = assign->getPriority();
                brief.CreateTime = assign->getCreateTime();
                brief.Description = assign->getDescription();
                brief.FileType = assign->getFileType();
                brief.MaxSize = assign->getMaxSize();
                briefs.push_back(brief);
            }
        }
        return briefs;
    });
}

} // namespace rpc
} // namespace blog
