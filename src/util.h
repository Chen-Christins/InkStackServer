/**
 * @file util.h
 * @brief 通用工具函数（邮箱/账号校验、用户ID编解码等）
 * @author Christins
 * @date 2026-06-03
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/db/mysql.h>
#include <chen/http/sse_session.h>
#include <chen/util/encryptor_util.h>
#include <chen/util/type_util.h>
#include <chen/util/util.h>

#include <regex>
#include <sys/statvfs.h>

namespace blog {

chen::IDB::ptr GetDB();

/**
 * @brief 生成密码哈希（bcrypt，含随机盐）
 * @param password 明文密码
 * @return bcrypt 哈希；失败返回空串
 */
std::string HashPassword(const std::string& password);

/**
 * @brief 校验密码，兼容历史无盐 MD5 哈希
 * @param password 明文密码
 * @param stored 数据库中保存的哈希
 * @return 是否匹配
 */
bool VerifyPassword(const std::string& password, const std::string& stored);

/**
 * @brief 判断是否为历史 MD5 哈希（登录成功后用于透明升级为 bcrypt）
 * @param stored 数据库中保存的哈希
 * @return 是否为 32 位十六进制 MD5
 */
bool IsLegacyPasswordHash(const std::string& stored);

struct DiskUsage {
    int64_t totalSize;
    int64_t usedSize;
    int64_t availableSize;
};

inline DiskUsage GetDiskUsage(const std::string& path = ".")
{
    DiskUsage du = {0, 0, 0};
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        du.totalSize = vfs.f_frsize * vfs.f_blocks;
        du.availableSize = vfs.f_frsize * vfs.f_bavail;
        du.usedSize = du.totalSize - du.availableSize;
    }
    return du;
}

inline bool IsEmail(const std::string& str)
{
    static const std::regex pattern("([0-9A-Za-z\\-_\\.]+)@([0-9a-z]+\\.[a-z]{2,8}(\\.[a-z]{2,8})?)");
    return std::regex_match(str, pattern);
}

static const std::string s_uid_secret = "BlogServer!2025$%^UID#@!EncryptKey";

inline std::string EncryptUserId(int64_t uid)
{
    std::string plain = std::to_string(uid);
    for (size_t i = 0; i < plain.size(); i++) {
        plain[i] ^= s_uid_secret[i % s_uid_secret.size()];
    }
    return chen::StringUtil::Base64Encode(plain);
}

inline int64_t DecryptUserId(const std::string& encrypted)
{
    if (encrypted.empty()) {
        return 0;
    }
    std::string data = chen::StringUtil::Base64Decode(encrypted);
    if (data.empty()) {
        return 0;
    }
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= s_uid_secret[i % s_uid_secret.size()];
    }
    return chen::TypeUtil::Atoi(data);
}

inline bool IsValidAccount(const std::string& str)
{
    static const std::regex s_account_regex("[A-Za-z][0-9A-Za-z\\-_\\.]{4,15}");
    return std::regex_match(str, s_account_regex);
}

static const std::string s_api_key_secret = "BlogServer!2025$%^APIKey#@!SecretKey";

inline std::string EncryptApiKey(const std::string& api_key)
{
    if (api_key.empty()) {
        return "";
    }
    std::string data = api_key;
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= s_api_key_secret[i % s_api_key_secret.size()];
    }
    return chen::StringUtil::Base64Encode(data);
}

inline std::string DecryptApiKey(const std::string& encrypted)
{
    if (encrypted.empty()) {
        return "";
    }
    std::string data = chen::StringUtil::Base64Decode(encrypted);
    if (data.empty()) {
        return "";
    }
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= s_api_key_secret[i % s_api_key_secret.size()];
    }
    return data;
}

inline std::string MaskApiKey(const std::string& api_key)
{
    if (api_key.length() <= 8) {
        return std::string(api_key.length(), '*');
    }
    // 保留前4位和后4位，中间用星号替代
    return api_key.substr(0, 3) + std::string(api_key.length() - 8, '*') + api_key.substr(api_key.length() - 2);
}

inline void SendWX(const std::string& group, const std::string& msg)
{
    // TODO: ...
}

#define DEFINE_AND_CHECK_STRING(result, var, param)                                 \
    std::string var = request->getParam(param);                                     \
    if (var.empty()) {                                                              \
        result->setErrnoFmt(errcode::PARAM_MISSING, "param %s is required", param); \
        break;                                                                      \
    }

#define DEFINE_AND_CHECK_STRING_WITH_DEFAULT(result, var, param, default_val) std::string var = request->getParam(param, default_val);

#define DEFINE_AND_CHECK_TYPE(result, type, var, param)                             \
    type var;                                                                       \
    if (!request->checkGetParamAs(param, var)) {                                    \
        result->setErrnoFmt(errcode::PARAM_MISSING, "param %s is required", param); \
        break;                                                                      \
    }

inline void SendSSEJson(chen::http::SSESession::ptr session, const std::string& type, const std::string& key, const std::string& value)
{
    Json::Value obj;
    obj["type"] = type;
    obj[key] = value;
    session->sendEvent(chen::JsonUtil::ToString(obj));
}

} // namespace blog
