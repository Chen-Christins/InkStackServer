/**
 * @file SSEStreamParser.hpp
 * @brief SSE 流解析器 — 逐行解析 AI 返回的 SSE，通过 AIProvider 提取文本并回调
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>
#include <string>

#include <chen/http/sse_session.h>

#include "common/ai/AIProvider.hpp"

namespace blog {
namespace ai {

/// SSE 流式回调解析器，每收到一块 body 数据就逐行解析、提取文本、推送 SSE 事件
/// 用法：传给 HttpConnection::DoRequestStreaming 作为 HttpStreamCallback
class SSEStreamParser {
public:
    typedef std::shared_ptr<SSEStreamParser> ptr;

    SSEStreamParser(chen::http::SSESession::ptr session, AIProvider::ptr provider);

    /// HttpStreamCallback 接口
    bool operator()(const char* data, size_t len);

    /// 累积的完整总结文本（流结束后读取，用于缓存）
    std::string full_text;

private:
    void processEvent();

    chen::http::SSESession::ptr session_;
    AIProvider::ptr provider_;
    std::string line_buf_;
    std::string current_event_;
};

}  // namespace ai
}  // namespace blog


