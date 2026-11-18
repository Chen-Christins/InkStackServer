/**
 * @file PlatformClient.hpp
 * @brief 平台服务 RPC 客户端（user 服务调用方）
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <chen/util/singleton.h>

#include "protocol_platform.h" // IWYU pragma: keep

namespace blog {
namespace client {

/**
 * @brief 平台服务客户端
 * @details 只有跨进程 RPC 调用，不 include 任何 platform 的实现头文件。
 *          系统设置带进程内缓存（默认 30 秒），避免每次请求都跨进程取配置。
 */
class PlatformClient {
public:
    /**
     * @brief 获取系统设置快照（带缓存）
     * @return 设置快照；从未成功拉取过时返回空结构（UpdatedAt == 0）
     */
    tagSystemSettings getSettings();

    /**
     * @brief 获取系统设置快照
     * @param[out] settings 设置快照
     * @return 是否拿到了可用快照（命中缓存或拉取成功）
     * @note 拉取失败时若存在历史缓存，仍返回上次快照并返回 true
     */
    bool tryGetSettings(tagSystemSettings& settings);

    /**
     * @brief 使缓存失效，下次调用重新拉取
     */
    void invalidateSettings();

    /**
     * @brief 踢下线指定设备（单向通知）
     * @param device_id 设备 id
     * @param reason 下发给客户端的提示
     * @return 是否已写入连接（不保证对端处理成功）
     */
    bool kickDevice(const std::string& device_id, const std::string& reason);

    /**
     * @brief 获取用户所属组织
     * @param uid 用户 id
     * @return 组织列表，调用失败时为空
     */
    std::vector<tagUserOrgBrief> listUserOrgs(int64_t uid);

    /**
     * @brief 获取用户可见作业
     * @param uid 用户 id
     * @return 作业列表，调用失败时为空
     */
    std::vector<tagUserAssignmentBrief> listUserAssignments(int64_t uid);

    /**
     * @brief 连通性自检
     * @return 对端 func_id，0 表示失败
     */
    int32_t ping();

private:
    std::mutex m_mutex;
    tagSystemSettings m_cached_settings;
    uint64_t m_cached_at_ms = 0;
    bool m_valid = false;
};

typedef chen::Singleton<PlatformClient> PlatformClientMgr;

} // namespace client
} // namespace blog
