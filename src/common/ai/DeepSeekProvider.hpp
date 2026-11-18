/**
 * @file DeepSeekProvider.hpp
 * @brief DeepSeek / OpenAI 兼容 协议适配器
 * @author Christins
 * @date 2026-07-18
 * @copyright Apache 2.0
 */
#pragma once

#include "common/ai/OpenAIProvider.hpp"

namespace blog {
namespace ai {

/**
 * @brief DeepSeek API 适配器 — 与 OpenAI 完全兼容，无需覆盖任何方法
 * @details 继承 OpenAIProvider 的所有实现（endpoint /v1/chat/completions、
 *          Bearer 认证、SSE 流式解析、[DONE] 结束标记等）。
 *          独立成类便于后续定制（如 reasoning_effort 参数、deepseek-chat 默认模型等）。
 */
class DeepSeekProvider : public OpenAIProvider {
public:
    typedef std::shared_ptr<DeepSeekProvider> ptr;
};

} // namespace ai
} // namespace blog
