/**
 * @file session_manager.h
 * @brief 会话配置 — 提供会话有效期
 * @author Christins
 * @date 2026-06-30
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

namespace blog {

/**
 * @brief 获取 session TTL（秒），优先读取系统设置，默认 24 小时
 */
int64_t GetSessionTTL();

} // namespace blog
