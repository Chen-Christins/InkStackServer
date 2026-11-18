/**
 * @file BadgeManager.hpp
 * @brief 成就徽章管理器
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/badge_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class BadgeManager {
public:
    BadgeManager();

    /**
     * @brief 确保默认徽章定义存在（表为空时自动插入）
     */
    void ensureDefaults();

    /**
     * @brief 获取所有有效徽章定义
     * @param[out] badges 徽章列表，按 sort_order 升序
     */
    void listAll(std::vector<data::BadgeInfo::ptr>& badges);

private:
    void insertDefault(
        const std::string& name,
        const std::string& desc,
        const std::string& icon,
        const std::string& color,
        const std::string& condition_key,
        int64_t condition_value,
        int32_t sort_order);
};

typedef chen::Singleton<BadgeManager> BadgeMgr;

} // namespace blog
