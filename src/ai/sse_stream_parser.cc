#include "sse_stream_parser.h"

#include "../util.h"

namespace blog {
namespace ai {

SSEStreamParser::SSEStreamParser(chen::http::SSESession::ptr session, AIProvider::ptr provider)
    : session_(session)
    , provider_(provider)
{
}

bool SSEStreamParser::operator()(const char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\n') {
            if (!line_buf_.empty() && line_buf_.back() == '\r') {
                line_buf_.pop_back();
            }

            if (line_buf_.empty()) {
                if (!current_event_.empty()) {
                    processEvent();
                    current_event_.clear();
                }
            } else if (line_buf_.size() >= 6 && line_buf_.substr(0, 6) == "data: ") {
                current_event_ = line_buf_.substr(6);
            }
            line_buf_.clear();
        } else {
            line_buf_ += c;
        }
    }
    return true;
}

void SSEStreamParser::processEvent()
{
    if (provider_->isDoneMarker(current_event_)) {
        SendSSEJson(session_, "done", "message", "总结生成完成");
        return;
    }

    Json::Value parsed;
    Json::Reader reader;
    if (!reader.parse(current_event_, parsed))
        return;

    std::string content = provider_->extractContent(parsed);
    if (!content.empty()) {
        SendSSEJson(session_, "chunk", "content", content);
        full_text += content;
    }

    if (provider_->isTerminal(parsed)) {
        SendSSEJson(session_, "done", "message", "总结生成完成");
        return;
    }

    std::string errMsg = provider_->getError(parsed);
    if (!errMsg.empty()) {
        SendSSEJson(session_, "error", "message", errMsg);
    }
}

} // namespace ai
} // namespace blog
