#include "common/ai/GLMProvider.hpp"

#include <chen/util/json_util.h>

namespace blog {
namespace ai {

std::string GLMProvider::buildRequest(
    const std::string& model,
    int32_t maxTokens,
    const std::string& systemPrompt,
    const std::string& prompt,
    bool stream)
{
    Json::Value body;
    body["model"] = model;
    body["max_tokens"] = maxTokens;
    if (stream) {
        body["stream"] = true;
    }

    // GLM thinking 模式 — 禁用以获得直接回复
    Json::Value thinking;
    thinking["type"] = "disabled";
    body["thinking"] = thinking;

    Json::Value messages(Json::arrayValue);
    Json::Value sys;
    sys["role"] = "system";
    sys["content"] = systemPrompt;
    messages.append(sys);

    Json::Value usr;
    usr["role"] = "user";
    usr["content"] = prompt;
    messages.append(usr);

    body["messages"] = messages;
    return chen::JsonUtil::ToString(body);
}

std::string GLMProvider::endpointSuffix() const
{
    return "/api/paas/v4/chat/completions";
}

} // namespace ai
} // namespace blog
