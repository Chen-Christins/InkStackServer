/**
 * @file webauthn.h
 * @brief WebAuthn 注册与认证校验服务
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include <json/json.h>
#include <chen/config/config.h>

struct WebAuthnConf {
    std::string rp_id;
    std::string rp_name = "InkStack";
    std::string origin;
    int64_t timeout_ms = 60000;
    std::string user_verification = "preferred";
    std::string attestation = "none";
    std::string user_handle_secret;
    int64_t challenge_ttl = 300;

    bool operator==(const WebAuthnConf& other) const
    {
        return rp_id == other.rp_id
            && rp_name == other.rp_name
            && origin == other.origin
            && timeout_ms == other.timeout_ms
            && user_verification == other.user_verification
            && attestation == other.attestation
            && user_handle_secret == other.user_handle_secret
            && challenge_ttl == other.challenge_ttl;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, WebAuthnConf> {
public:
    WebAuthnConf operator()(const std::string& value)
    {
        WebAuthnConf conf;
        YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["rp_id"]) {
            conf.rp_id = node["rp_id"].as<std::string>();
        }
        if (node["rp_name"]) {
            conf.rp_name = node["rp_name"].as<std::string>();
        }
        if (node["origin"]) {
            conf.origin = node["origin"].as<std::string>();
        }
        if (node["timeout_ms"]) {
            conf.timeout_ms = node["timeout_ms"].as<int64_t>();
        }
        if (node["user_verification"]) {
            conf.user_verification = node["user_verification"].as<std::string>();
        }
        if (node["attestation"]) {
            conf.attestation = node["attestation"].as<std::string>();
        }
        if (node["user_handle_secret"]) {
            conf.user_handle_secret = node["user_handle_secret"].as<std::string>();
        }
        if (node["challenge_ttl"]) {
            conf.challenge_ttl = node["challenge_ttl"].as<int64_t>();
        }
        return conf;
    }
};

template <>
class LexicalCast<WebAuthnConf, std::string> {
public:
    std::string operator()(const WebAuthnConf& cfg)
    {
        YAML::Node node;
        node["rp_id"] = cfg.rp_id;
        node["rp_name"] = cfg.rp_name;
        node["origin"] = cfg.origin;
        node["timeout_ms"] = cfg.timeout_ms;
        node["user_verification"] = cfg.user_verification;
        node["attestation"] = cfg.attestation;
        node["user_handle_secret"] = cfg.user_handle_secret;
        node["challenge_ttl"] = cfg.challenge_ttl;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

namespace blog {
namespace webauthn {

/**
 * @brief 注册成功后需要落库的凭据信息
 */
struct RegistrationResult {
    std::string credential_id; ///< base64url
    std::string public_key;    ///< base64url COSE_Key
    int32_t alg = 0;
    int64_t sign_count = 0;
    std::string transports;
    std::string aaguid;
    int32_t backup_eligible = 0;
    int32_t backup_state = 0;
};

/**
 * @brief 认证成功后需要更新的凭据状态
 */
struct AuthenticationResult {
    std::string credential_id; ///< base64url
    int64_t sign_count = 0;
    bool user_verified = false;
    int32_t backup_state = 0;
};

class WebAuthnService {
public:
    /**
     * @brief WebAuthn 是否已配置（rp_id / origin）
     */
    static bool IsConfigured();

    /**
     * @brief 生成注册 options 并写入一次性 challenge
     * @param uid 用户 ID
     * @param account 账号
     * @param display_name 显示名
     * @param passkey_name 凭据名称（finish 时回读）
     * @param exclude_credential_ids 已注册的 credential ID（base64url）
     * @param options 输出 PublicKeyCredentialCreationOptions
     * @return 0 成功，否则业务错误码
     */
    static int32_t CreateRegistrationOptions(
        int64_t uid,
        const std::string& account,
        const std::string& display_name,
        const std::string& passkey_name,
        const std::vector<std::string>& exclude_credential_ids,
        Json::Value& options);

    /**
     * @brief 校验注册响应
     * @param uid 用户 ID
     * @param credential 浏览器 PublicKeyCredential.toJSON()
     * @param result 输出凭据
     * @param passkey_name 输出 begin 阶段保存的凭据名称
     * @return 0 成功，否则业务错误码
     */
    static int32_t VerifyRegistration(int64_t uid, const Json::Value& credential, RegistrationResult& result, std::string& passkey_name);

    /**
     * @brief 生成认证 options 并写入一次性 challenge
     * @param uid 已指定用户 ID；0 表示 discoverable credential 登录
     * @param allow_credential_ids 允许的 credential ID（base64url）
     * @param options 输出 PublicKeyCredentialRequestOptions
     * @return 0 成功，否则业务错误码
     */
    static int32_t CreateAuthenticationOptions(int64_t uid, const std::vector<std::string>& allow_credential_ids, Json::Value& options);

    /**
     * @brief 从浏览器 PublicKeyCredential.toJSON() 中提取 credential ID
     * @param credential 浏览器凭据 JSON
     * @param credential_id 输出规范化 base64url credential ID
     * @return 是否成功
     */
    static bool ExtractCredentialId(const Json::Value& credential, std::string& credential_id);

    /**
     * @brief 校验认证响应
     * @param uid 凭据所属用户
     * @param stored_public_key 数据库中保存的 base64url COSE_Key
     * @param stored_sign_count 数据库中保存的签名计数器
     * @param credential 浏览器 PublicKeyCredential.toJSON()
     * @param result 输出认证结果
     * @return 0 成功，否则业务错误码
     */
    static int32_t VerifyAuthentication(
        int64_t uid,
        const std::string& stored_public_key,
        int64_t stored_sign_count,
        const Json::Value& credential,
        AuthenticationResult& result);

    static std::string RpId();
    static std::string RpName();
    static std::string Origin();

private:
    static std::string UserHandle(int64_t uid);
};

} // namespace webauthn
} // namespace blog
