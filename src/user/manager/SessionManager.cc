#include "user/manager/SessionManager.hpp"

#include "user/client/PlatformClient.hpp"

namespace blog {

/// 默认 session TTL（24 小时 = 86400 秒）
static const int64_t kDefaultSessionTTL = 24 * 3600;

int64_t GetSessionTTL()
{
    tagSystemSettings settings;
    if (client::PlatformClientMgr::GetInstance()->tryGetSettings(settings) && settings.SessionTimeout > 0) {
        return static_cast<int64_t>(settings.SessionTimeout) * 3600;
    }
    return kDefaultSessionTTL;
}

} // namespace blog
