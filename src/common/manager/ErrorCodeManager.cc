#include "common/manager/ErrorCodeManager.hpp"

#include <fstream>

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <json/json.h>

#include "common/i18n/I18n.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

ErrorCodeManager::ErrorCodeManager()
{
}

bool ErrorCodeManager::load(const std::string& configPath)
{
    m_configPath = configPath;

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        ERROR(logger) << "Failed to open error codes config: " << configPath;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
        ERROR(logger) << "Failed to parse error codes JSON: " << errs;
        return false;
    }
    ifs.close();

    m_version = root.get("version", "unknown").asString();

    m_keys.clear();
    m_zhMessages.clear();
    m_enMessages.clear();

    const Json::Value& errors = root["errors"];
    if (!errors.isArray()) {
        ERROR(logger) << "errors.json: 'errors' field is not an array";
        return false;
    }

    for (const auto& item : errors) {
        int32_t code = item["errno"].asInt();
        std::string key = item.get("key", "").asString();
        std::string zh = item.get("zh", "").asString();
        std::string en = item.get("en", "").asString();

        if (!key.empty()) {
            m_keys[code] = key;
        }
        m_zhMessages[code] = zh;
        m_enMessages[code] = en;
    }

    INFO(logger) << "ErrorCodeManager loaded " << m_zhMessages.size() << " error codes from " << configPath << " (version: " << m_version << ")";
    return true;
}

bool ErrorCodeManager::reload()
{
    if (m_configPath.empty()) {
        ERROR(logger) << "ErrorCodeManager: no config path, cannot reload";
        return false;
    }
    return load(m_configPath);
}

std::string ErrorCodeManager::getMessage(int32_t errCode) const
{
    // 默认使用中文
    return getMessage(errCode, "zh");
}

std::string ErrorCodeManager::getMessage(int32_t errCode, const std::string& lang) const
{
    const std::string normalized = I18n::NormalizeLanguage(lang);
    const bool english = normalized == "en";

    auto keyIt = m_keys.find(errCode);
    if (keyIt != m_keys.end() && !keyIt->second.empty()) {
        const std::string translated = I18n::Translate(keyIt->second, normalized.empty() ? I18n::DefaultLanguage() : normalized);
        if (!translated.empty() && translated != keyIt->second) {
            return translated;
        }
    }

    const auto& messages = english ? m_enMessages : m_zhMessages;
    auto it = messages.find(errCode);
    if (it != messages.end()) {
        return it->second;
    }
    return english ? "Unknown error" : "未知错误";
}

std::map<int32_t, std::string> ErrorCodeManager::getAllMessages(const std::string& lang) const
{
    std::map<int32_t, std::string> result;
    for (const auto& entry : m_zhMessages) {
        result[entry.first] = getMessage(entry.first, lang);
    }
    return result;
}

std::string ErrorCodeManager::toJson(const std::string& lang) const
{
    Json::Value result;
    const auto& messages = getAllMessages(lang);
    for (const auto& pair : messages) {
        result[std::to_string(pair.first)] = pair.second;
    }
    return chen::JsonUtil::ToString(result);
}

} // namespace blog
