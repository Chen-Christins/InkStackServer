#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <utility>

#include <libintl.h>
#include <locale.h>

#include <chen/log/log.h>
#include <chen/util/string_util.h>

#include "../manager/system_settings_manager.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

constexpr const char* kDomainPrefix = "techhaven_";
constexpr const char* kDefaultLanguage = "zh_CN";
const std::string kTrimChars = " \t\r\n\v\f";

std::string DomainFor(const std::string& language)
{
    return std::string(kDomainPrefix) + language;
}

double ParseQuality(const std::string& value)
{
    const std::string text = chen::StringUtil::Trim(value, kTrimChars);
    if (text.size() < 3) {
        return -1.0;
    }
    if ((text[0] != 'q' && text[0] != 'Q') || text[1] != '=') {
        return -1.0;
    }
    const std::string number = chen::StringUtil::Trim(text.substr(2), kTrimChars);
    if (number.empty()) {
        return -1.0;
    }
    char* end = nullptr;
    const double quality = std::strtod(number.c_str(), &end);
    if (end == number.c_str() || *end != '\0' || quality < 0.0 || quality > 1.0) {
        return -1.0;
    }
    return quality;
}

std::string BestFromAcceptLanguage(const std::string& header)
{
    if (header.empty()) {
        return "";
    }

    struct Candidate {
        double quality;
        std::string language;
    };

    std::vector<Candidate> candidates;
    size_t start = 0;
    while (start <= header.size()) {
        const size_t comma = header.find(',', start);

        std::string str = header.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        const std::string item = chen::StringUtil::Trim(str, kTrimChars);

        if (!item.empty()) {
            std::string tag;
            double quality = 1.0;
            bool first = true;
            bool valid = true;
            size_t pos = 0;
            while (pos <= item.size()) {
                const size_t semicolon = item.find(';', pos);

                std::string pt = item.substr(pos, semicolon == std::string::npos ? std::string::npos : semicolon - pos);
                const std::string part = chen::StringUtil::Trim(pt, kTrimChars);

                if (first) {
                    tag = part;
                    first = false;
                } else if (part.size() > 1 && (part[0] == 'q' || part[0] == 'Q') && part[1] == '=') {
                    const double parsed = ParseQuality(part);
                    if (parsed < 0.0) {
                        valid = false;
                        break;
                    }
                    quality = parsed;
                }
                if (semicolon == std::string::npos) {
                    break;
                }
                pos = semicolon + 1;
            }

            if (valid && quality > 0.0 && !tag.empty() && tag != "*") {
                std::string language = I18n::NormalizeLanguage(tag);
                if (!language.empty()) {
                    candidates.push_back({quality, std::move(language)});
                }
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.quality > rhs.quality;
    });

    return candidates.empty() ? "" : candidates.front().language;
}

} // namespace

const std::vector<std::string>& I18n::SupportedLanguages()
{
    static const std::vector<std::string> s_languages = {"en", "zh_CN"};
    return s_languages;
}

std::string I18n::DefaultLanguage()
{
    return kDefaultLanguage;
}

std::string I18n::NormalizeLanguage(std::string_view language)
{
    std::string value = chen::StringUtil::Trim(std::string(language), kTrimChars);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    std::replace(value.begin(), value.end(), '_', '-');

    if (value == "zh" || value.rfind("zh-", 0) == 0) {
        return "zh_CN";
    }
    if (value == "en" || value.rfind("en-", 0) == 0) {
        return "en";
    }
    return "";
}

bool I18n::Init(const std::string& localeDir)
{
    std::error_code ec;
    const bool exists = std::filesystem::is_directory(localeDir, ec);
    if (!exists) {
        WARN(logger) << "i18n locale directory not found: " << localeDir;
    }

    const char* currentLocale = setlocale(LC_MESSAGES, nullptr);
    if (!currentLocale || std::string(currentLocale) != "C") {
        if (!setlocale(LC_MESSAGES, "C")) {
            WARN(logger) << "setlocale(LC_MESSAGES, C) failed";
        }
    }

    bool ok = exists;
    for (const auto& language : SupportedLanguages()) {
        const std::string domain = DomainFor(language);
        if (!bindtextdomain(domain.c_str(), localeDir.c_str())) {
            WARN(logger) << "bindtextdomain failed: " << domain << " -> " << localeDir;
            ok = false;
        }
        if (!bind_textdomain_codeset(domain.c_str(), "UTF-8")) {
            WARN(logger) << "bind_textdomain_codeset failed: " << domain;
            ok = false;
        }
    }

    INFO(logger) << "i18n initialized, locale_dir=" << localeDir;
    return ok;
}

std::string I18n::Translate(std::string_view msgid, std::string_view language)
{
    if (msgid.empty()) {
        return "";
    }

    std::string normalized = NormalizeLanguage(language);
    if (normalized.empty()) {
        normalized = DefaultLanguage();
    }

    const std::string domain = DomainFor(normalized);
    const std::string key(msgid);
    const char* translated = dgettext(domain.c_str(), key.c_str());
    if (!translated) {
        return key;
    }
    return std::string(translated);
}

std::string I18n::ResolveLanguage(chen::http::HttpRequest::ptr request)
{
    if (request) {
        const std::string query = request->getParam("lang");
        std::string normalized = NormalizeLanguage(query);
        if (!normalized.empty()) {
            return normalized;
        }

        normalized = BestFromAcceptLanguage(request->getHeader("Accept-Language"));
        if (!normalized.empty()) {
            return normalized;
        }
    }

    auto settings = SystemSettingsMgr::GetInstance()->get();
    if (settings) {
        std::string normalized = NormalizeLanguage(settings->getLanguage());
        if (!normalized.empty()) {
            return normalized;
        }
    }

    return DefaultLanguage();
}

} // namespace blog
