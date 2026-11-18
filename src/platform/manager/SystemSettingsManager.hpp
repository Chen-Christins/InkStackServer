#pragma once

#include "blog/data/system_settings_info.h"

#include <mutex>

#include <chen/util/singleton.h>

namespace blog {

class SystemSettingsManager {
public:
    void add(blog::data::SystemSettingsInfo::ptr info);

    blog::data::SystemSettingsInfo::ptr get();

    bool update(blog::data::SystemSettingsInfo::ptr info);

private:
    std::mutex m_mutex;
    blog::data::SystemSettingsInfo::ptr m_data;
};

typedef chen::Singleton<SystemSettingsManager> SystemSettingsMgr;

} // namespace blog
