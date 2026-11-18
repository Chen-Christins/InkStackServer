/**
 * @file GLMProvider.hpp
 * @brief 智谱 GLM / OpenAI 兼容 协议适配器
 * @author Christins
 * @date 2026-06-13
 * @copyright Apache 2.0
 */
#pragma once

#include "common/ai/OpenAIProvider.hpp"

namespace blog {
namespace ai {

class GLMProvider : public OpenAIProvider {
public:
    typedef std::shared_ptr<GLMProvider> ptr;

    std::string buildRequest(
        const std::string& model,
        int32_t maxTokens,
        const std::string& systemPrompt,
        const std::string& prompt,
        bool stream = true) override;

    std::string endpointSuffix() const override;
};

} // namespace ai
} // namespace blog
