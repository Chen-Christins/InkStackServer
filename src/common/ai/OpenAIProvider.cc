#include "common/ai/OpenAIProvider.hpp"

#include <chen/util/json_util.h>

namespace blog {
namespace ai {

std::string OpenAIProvider::buildRequest(
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

std::string OpenAIProvider::endpointSuffix() const
{
    return "/v1/chat/completions";
}

std::string OpenAIProvider::extractContent(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        return choices[0]["delta"]["content"].asString();
    }
    return "";
}

bool OpenAIProvider::isTerminal(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        auto finish = choices[0]["finish_reason"].asString();
        return !finish.empty() && finish != "null";
    }
    return false;
}

std::string OpenAIProvider::getError(const Json::Value& /*parsed*/)
{
    return "";
}

std::string OpenAIProvider::extractNonStreamingContent(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        return choices[0]["message"]["content"].asString();
    }
    auto& err = parsed["error"];
    if (!err.isNull()) {
        return "";
    }
    return "";
}

void OpenAIProvider::authHeaders(const std::string& apiKey, std::map<std::string, std::string>& outHeaders)
{
    outHeaders["Authorization"] = "Bearer " + apiKey;
}

bool OpenAIProvider::isDoneMarker(const std::string& rawData)
{
    return rawData == "[DONE]";
}

} // namespace ai
} // namespace blog
