#include "article_ai_summary_servlet.h"

#include "../../ai/ai_provider.h"
#include "../../ai/sse_stream_parser.h"
#include "../../auth/auth_service.h"
#include "../../manager/article_manager.h"
#include "../../manager/user_ai_config_manager.h"
#include "../../util.h"

#include <chen/config/config.h>
#include <chen/http/http_connection.h>
#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

// 系统默认 AI 配置 — 用户未配置时的兜底
static chen::ConfigVar<std::string>::ptr g_ai_type = chen::Config::Lookup("ai.type", std::string(""), "default AI provider type");
static chen::ConfigVar<std::string>::ptr g_ai_model = chen::Config::Lookup("ai.model", std::string(""), "default AI model");
static chen::ConfigVar<std::string>::ptr g_ai_api_key = chen::Config::Lookup("ai.api_key", std::string(""), "default AI API key");

static const char* SYSTEM_PROMPT = "你是一个专业的文章总结助手。";

ArticleAISummaryServlet::ArticleAISummaryServlet()
    : chen::http::SSEServlet("ArticleAISummaryServlet")
{
}

int32_t ArticleAISummaryServlet::onConnect(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    AuthContext ctx = AuthService::Resolve(request, nullptr, false);
    int64_t uid = ctx.uid;
    if (!uid) {
        session->sendEvent(R"({"type":"error","message":"not login"})");
        return -1;
    }

    int64_t article_id = 0;
    std::string article_title;
    std::string article_content;
    {
        auto article_id_str = request->getParam("article_id");
        if (article_id_str.empty()) {
            session->sendEvent(R"({"type":"error","message":"param article_id is required"})");
            return -1;
        }
        try {
            article_id = chen::TypeUtil::Atoi(article_id_str);
        } catch (...) {
            session->sendEvent(R"({"type":"error","message":"invalid article_id"})");
            return -1;
        }

        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article) {
            session->sendEvent(R"({"type":"error","message":"article not found"})");
            return -1;
        }
        article_title = article->getTitle();
        article_content = article->getContent();

        const size_t MAX_CONTENT_LEN = 12000;
        if (article_content.size() > MAX_CONTENT_LEN) {
            article_content = article_content.substr(0, MAX_CONTENT_LEN) + "...";
        }
    }

    std::string ai_type;
    std::string ai_url;
    std::string ai_key;
    std::string ai_model;
    int32_t ai_max_tokens = 4096;
    {
        auto config = UserAIConfigMgr::GetInstance()->getByUserId(uid);
        if (config) {
            // 用户配置：API key 是加密存储的，需要解密
            std::string encrypted_key = config->getApiKey();
            ai_key = DecryptApiKey(encrypted_key);
            ai_type = config->getType();
            ai_url = config->getUrl();
            ai_model = config->getModel();
            ai_max_tokens = config->getMaxTokens();

            TRACE(logger) << "AI config source=user, type=" << ai_type << ", model=" << ai_model << ", encrypted_key_len=" << encrypted_key.size()
                          << ", decrypted_key_len=" << ai_key.size();

            if (ai_key.empty() && !encrypted_key.empty()) {
                ERROR(logger) << "DecryptApiKey returned empty for non-empty encrypted key "
                              << "(len=" << encrypted_key.size() << "), " << "possible Base64Decode failure or raw key stored in DB";
            }
        } else {
            // 系统默认配置：API key 是明文，无需解密
            ai_type = g_ai_type->getValue();
            ai_model = g_ai_model->getValue();
            ai_key = g_ai_api_key->getValue();

            INFO(logger) << "AI config source=system, type=" << ai_type << ", model=" << ai_model << ", key_len=" << ai_key.size();
        }

        if (ai_type.empty() || ai_key.empty()) {
            session->sendEvent(R"({"type":"error","message":"AI config not found"})");
            return -1;
        }

        // 推导默认 base URL（用户未配置 URL 时，根据厂商类型自动补全，默认 GLM）
        if (ai_url.empty()) {
            if (ai_type == "claude") {
                ai_url = "https://api.anthropic.com";
            } else if (ai_type == "openai") {
                ai_url = "https://api.openai.com";
            } else {
                ai_url = "https://open.bigmodel.cn";
            }
        }

        TRACE(logger) << "AI final config: type=" << ai_type << ", url=" << ai_url << ", model=" << ai_model << ", max_tokens=" << ai_max_tokens;
    }

    auto provider = ai::createProvider(ai_type);
    if (!provider) {
        ERROR(logger) << "failed to create AI provider for type: " << ai_type;
        session->sendEvent(R"({"type":"error","message":"unsupported AI provider"})");
        return -1;
    }

    std::string prompt = "请用简洁的中文总结以下文章的核心内容，控制在200字以内：\n\n"
                         "标题："
        + article_title + "\n\n" + article_content;

    std::string api_body_str = provider->buildRequest(ai_model, ai_max_tokens, SYSTEM_PROMPT, prompt, true);

    auto uri = chen::Uri::Create(ai_url + provider->endpointSuffix());
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    provider->authHeaders(ai_key, headers);

    SendSSEJson(session, "start", "message", "开始生成总结...");

    const int MAX_RETRIES = 3;
    chen::http::HttpResult::ptr http_result;
    ai::SSEStreamParser parser(session, provider);
    std::string raw_body;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        if (attempt > 1) {
            WARN(logger) << "retrying AI API call, attempt " << attempt << "/" << MAX_RETRIES;
            // 重置 parser，丢弃上一次的不完整数据
            parser = ai::SSEStreamParser(session, provider);
            raw_body.clear();
        }

        auto callback = [&parser, &raw_body](const char* data, size_t len) {
            raw_body.append(data, len);
            return parser(data, len);
        };

        http_result = chen::http::HttpConnection::DoRequestStreaming(chen::http::HttpMethod::POST, uri, 60000, callback, headers, api_body_str);

        if (http_result && http_result->result == 0) {
            break;
        }

        ERROR(logger) << "AI API call failed (attempt " << attempt << "/" << MAX_RETRIES
                      << "): " << (http_result ? http_result->error : "null result");
    }

    if (!http_result || http_result->result != 0) {
        ERROR(logger) << "AI API call failed after " << MAX_RETRIES << " attempts";
        SendSSEJson(session, "error", "message", "AI service call failed");
        return -1;
    }

    auto ai_response = http_result->response;
    if (!ai_response) {
        SendSSEJson(session, "error", "message", "no response from AI service");
        return -1;
    }

    int status = static_cast<int>(ai_response->getStatus());
    if (status != 200) {
        ERROR(logger) << "AI service returned status " << status << " body=" << raw_body;
        SendSSEJson(session, "error", "message", "AI service returned error");
        return -1;
    }

    SendSSEJson(session, "done", "message", "总结生成完成");

    return 0;
}

int32_t ArticleAISummaryServlet::onClose(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    return 0;
}

} // namespace servlet
} // namespace blog
