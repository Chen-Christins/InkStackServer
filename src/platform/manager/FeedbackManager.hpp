/**
 * @file FeedbackManager.hpp
 * @brief 用户反馈管理器
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/user_feedback_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class FeedbackManager {
public:
    FeedbackManager();

    /**
     * @brief 分页查询反馈列表
     * @param[out] infos 反馈列表
     * @param type 反馈类型过滤（空字符串表示不过滤）
     * @param page 页码（从1开始）
     * @param page_size 每页条数
     * @param[out] total 总条数
     * @return true 成功，false 失败
     */
    bool list(
        std::vector<data::UserFeedbackInfo::ptr>& infos,
        const std::string& type = "",
        int32_t page = 1,
        int32_t page_size = 20,
        int64_t* total = nullptr);

    /**
     * @brief 根据 ID 获取反馈
     * @param id 反馈ID
     * @return 反馈对象，不存在返回 nullptr
     */
    data::UserFeedbackInfo::ptr get(int64_t id);

    /**
     * @brief 软删除反馈（设置 is_deleted = 1）
     * @param id 反馈ID
     * @return true 成功，false 失败
     */
    bool remove(int64_t id);
};

typedef chen::Singleton<FeedbackManager> FeedbackMgr;

} // namespace blog
