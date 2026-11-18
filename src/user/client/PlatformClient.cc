#include "user/client/PlatformClient.hpp"

#include <chen/util/time_util.h>

#include "common/rpc/RpcEndpoint.hpp"
#include "common/rpc/RpcHubClient.hpp"

namespace blog {
namespace client {

namespace {

/// 系统设置缓存有效期（毫秒）
constexpr uint64_t kSettingsTtlMs = 30 * 1000;

} // namespace

tagSystemSettings PlatformClient::getSettings()
{
    tagSystemSettings settings;
    tryGetSettings(settings);
    return settings;
}

bool PlatformClient::tryGetSettings(tagSystemSettings& settings)
{
    const uint64_t now = chen::TimeUtil::GetCurrentMs();
    {
        std::lock_guard lock(m_mutex);
        if (m_valid && now - m_cached_at_ms < kSettingsTtlMs) {
            settings = m_cached_settings;
            return true;
        }
    }

    tagSystemSettings fresh;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_PLATFORM, PLATFORM_CMD_GET_SETTINGS, fresh)) {
        std::lock_guard lock(m_mutex);
        if (m_valid) {
            settings = m_cached_settings;
            return true;
        }
        return false;
    }

    std::lock_guard lock(m_mutex);
    m_cached_settings = fresh;
    m_cached_at_ms = now;
    m_valid = true;
    settings = m_cached_settings;
    return true;
}

void PlatformClient::invalidateSettings()
{
    std::lock_guard lock(m_mutex);
    m_valid = false;
}

bool PlatformClient::kickDevice(const std::string& device_id, const std::string& reason)
{
    return rpc::RpcHubClientMgr::GetInstance()->notify(rpc::FUNC_PLATFORM, PLATFORM_CMD_KICK_DEVICE, device_id, reason);
}

std::vector<tagUserOrgBrief> PlatformClient::listUserOrgs(int64_t uid)
{
    std::vector<tagUserOrgBrief> orgs;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_PLATFORM, PLATFORM_CMD_LIST_USER_ORGS, orgs, uid)) {
        return {};
    }
    return orgs;
}

std::vector<tagUserAssignmentBrief> PlatformClient::listUserAssignments(int64_t uid)
{
    std::vector<tagUserAssignmentBrief> assignments;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_PLATFORM, PLATFORM_CMD_LIST_USER_ASSIGNMENTS, assignments, uid)) {
        return {};
    }
    return assignments;
}

int32_t PlatformClient::ping()
{
    int32_t func_id = 0;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_PLATFORM, PLATFORM_CMD_PING, func_id)) {
        return 0;
    }
    return func_id;
}

} // namespace client
} // namespace blog
