#pragma once

#include "blog/data/system_settings_info.h"

#include <mutex>

#include <chen/util/singleton.h>

namespace blog {

class SystemSettingsManager {
public:
    void add(blog::data::SystemSettingsInfo::ptr info);

    /**
     * @brief 获取当前系统设置
     * @note 返回的是全局共享对象，只能读取，不允许修改字段
     */
    blog::data::SystemSettingsInfo::ptr get();

    /**
     * @brief 复制当前系统设置，供写路径修改
     * @details 读请求与写请求并发时，直接改共享对象会让读侧拷贝到正在析构/改写的 std::string，
     *          因此写路径必须先 clone 再 update，由 update 整体替换缓存指针。
     */
    blog::data::SystemSettingsInfo::ptr clone();

    bool update(blog::data::SystemSettingsInfo::ptr info);

private:
    std::mutex m_mutex;
    blog::data::SystemSettingsInfo::ptr m_data;
};

typedef chen::Singleton<SystemSettingsManager> SystemSettingsMgr;

} // namespace blog
