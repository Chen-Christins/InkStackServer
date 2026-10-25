#include "session_manager.h"

#include "system_settings_manager.h"

namespace blog {

/// 默认 session TTL（24 小时 = 86400 秒）
static const int64_t kDefaultSessionTTL = 24 * 3600;

int64_t GetSessionTTL()
{
    auto settings = SystemSettingsMgr::GetInstance()->get();
    if (settings && settings->getSessionTimeout() > 0) {
        return static_cast<int64_t>(settings->getSessionTimeout()) * 3600;
    }
    return kDefaultSessionTTL;
}

} // namespace blog
