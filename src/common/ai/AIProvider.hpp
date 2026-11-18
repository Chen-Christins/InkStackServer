/**
 * @file AIProvider.hpp
 * @brief AI 厂商适配器 — 抽象接口 + 工厂
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include <json/json.h>

#include <memory>
#include <string>

namespace blog {
namespace ai {

/**
 * @brief AI 厂商抽象接口 — 封装不同厂商的请求构建与响应解析差异
 */
class AIProvider {
public:
    typedef std::shared_ptr<AIProvider> ptr;
    virtual ~AIProvider() = default;

    /**
     * @brief 构建 API 请求体 JSON
     * @param model 模型名
     * @param maxTokens 最大输出 token 数
     * @param systemPrompt 系统提示词
     * @param prompt 用户提示词
     * @param stream true=流式格式, false=非流式格式
     * @return JSON 字符串
     */
    virtual std::string buildRequest(
        const std::string& model,
        int32_t maxTokens,
        const std::string& systemPrompt,
        const std::string& prompt,
        bool stream = true) = 0;

    /**
     * @brief API endpoint 路径后缀（如 /v1/chat/completions）
     * @return 路径后缀字符串
     */
    virtual std::string endpointSuffix() const = 0;

    /**
     * @brief 从流式 data: 事件的 JSON 中提取文本块
     * @param parsed 已解析的 JSON 值
     * @return 文本块内容；无内容返回空串
     */
    virtual std::string extractContent(const Json::Value& parsed) = 0;

    /**
     * @brief 从非流式响应的 JSON 中提取完整文本
     * @param parsed 已解析的 JSON 值
     * @return 完整响应文本；无内容返回空串
     */
    virtual std::string extractNonStreamingContent(const Json::Value& parsed) = 0;

    /**
     * @brief 判断该事件是否表示流结束
     * @param parsed 已解析的 JSON 值
     * @return true=流已结束
     */
    virtual bool isTerminal(const Json::Value& parsed) = 0;

    /**
     * @brief 从响应 JSON 中提取错误消息
     * @param parsed 已解析的 JSON 值
     * @return 错误消息；无错误返回空串
     */
    virtual std::string getError(const Json::Value& parsed) = 0;

    /**
     * @brief 检查非 JSON 的流结束标记（如 OpenAI 的 [DONE]）
     * @param rawData 原始文本行
     * @return true=命中结束标记
     */
    virtual bool isDoneMarker(const std::string& rawData) { return false; }

    /**
     * @brief 填充厂商特定的认证请求头
     * @param apiKey API 密钥
     * @param outHeaders [out] 要填充的请求头 map
     */
    virtual void authHeaders(const std::string& apiKey, std::map<std::string, std::string>& outHeaders) = 0;
};

/**
 * @brief AI 厂商工厂函数
 * @param type 厂商类型（"openai"、"claude"、"glm" 等）
 * @return 对应厂商的 AIProvider 实例指针；默认返回 OpenAIProvider
 */
AIProvider::ptr createProvider(const std::string& type);

} // namespace ai
} // namespace blog
