/**
 * @file UserAIConfigManager.hpp
 * @brief 用户AI配置管理器
 * @author Christins
 * @date 2026-06-09
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/user_ai_config_info.h"

#include <chen/util/singleton.h>

namespace blog {

class UserAIConfigManager {
public:
    /**
     * @brief 获取用户的AI配置（Redis 缓存 → DB）
     * @param user_id 用户ID
     * @return 配置信息，未配置返回nullptr
     */
    blog::data::UserAiConfigInfo::ptr getByUserId(int64_t user_id);

    /**
     * @brief 保存用户AI配置（写DB同时更新缓存）
     * @param info 配置信息
     * @return true成功 false失败
     */
    bool save(blog::data::UserAiConfigInfo::ptr info);
};

typedef chen::Singleton<UserAIConfigManager> UserAIConfigMgr;

} // namespace blog
