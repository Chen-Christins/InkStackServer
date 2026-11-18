#include "platform/manager/SystemSettingsManager.hpp"

#include <chen/log/log.h>

#include "../../common/util/Util.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

void SystemSettingsManager::add(blog::data::SystemSettingsInfo::ptr info)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_data = info;
}

blog::data::SystemSettingsInfo::ptr SystemSettingsManager::get()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_data) {
            return m_data;
        }
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::SystemSettingsInfoDao::Query(1, db);
    if (!info) {
        // 创建默认行
        info.reset(new data::SystemSettingsInfo);
        info->setId(1);
        if (data::SystemSettingsInfoDao::Insert(info, db)) {
            ERROR(logger) << "SystemSettingsManager insert default row fail";
            return info;
        }
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_data = info;
    return info;
}

bool SystemSettingsManager::update(blog::data::SystemSettingsInfo::ptr info)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    info->setId(1);
    if (blog::data::SystemSettingsInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "SystemSettingsManager update fail";
        return false;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_data = info;
    return true;
}

} // namespace blog
