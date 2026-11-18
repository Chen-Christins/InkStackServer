/**
 * @file ClaudeProvider.hpp
 * @brief Claude / Anthropic 协议适配器
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include "common/ai/AIProvider.hpp"

namespace blog {
namespace ai {

class ClaudeProvider : public AIProvider {
public:
    typedef std::shared_ptr<ClaudeProvider> ptr;

    std::string buildRequest(
        const std::string& model,
        int32_t maxTokens,
        const std::string& systemPrompt,
        const std::string& prompt,
        bool stream = true) override;

    std::string endpointSuffix() const override;

    std::string extractContent(const Json::Value& parsed) override;

    std::string extractNonStreamingContent(const Json::Value& parsed) override;

    bool isTerminal(const Json::Value& parsed) override;

    std::string getError(const Json::Value& parsed) override;

    void authHeaders(const std::string& apiKey, std::map<std::string, std::string>& outHeaders) override;
};

} // namespace ai
} // namespace blog
