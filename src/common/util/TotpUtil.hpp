/**
 * @file TotpUtil.hpp
 * @brief TOTP 双因素认证工具（RFC 6238 / RFC 4226）
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace blog {

/// TOTP 工具类 — 生成密钥、验证码、OTP URI、恢复码
class TotpUtil {
public:
    /// 生成随机 Base32 编码的 TOTP 密钥（20 字节 = 160 bit）
    static std::string GenerateSecret();

    /// 验证 TOTP 验证码（允许前后 window 个时间步的容差）
    /// @param secret Base32 编码的密钥
    /// @param code 6 位验证码
    /// @param window 容差步数（默认 1，即 ±30s）
    /// @return true 验证通过
    static bool VerifyCode(const std::string& secret, const std::string& code, int window = 1);

    /// 生成 otpauth:// URI（可直接用于 QR 码生成）
    /// @param secret Base32 编码的密钥
    /// @param account 账号（邮箱或用户名）
    /// @param issuer 发行者名称
    static std::string GetOtpUri(const std::string& secret, const std::string& account, const std::string& issuer = "TechHaven");

    /// 生成恢复码（明文列表），同时返回哈希列表用于存储
    /// @param count 生成数量
    /// @param[out] hashedCodes 恢复码的 SHA-256 哈希列表
    /// @return 明文恢复码列表（只展示一次）
    static std::vector<std::string> GenerateRecoveryCodes(int count, std::vector<std::string>& hashedCodes);

    /// 对恢复码进行 SHA-256 哈希
    static std::string HashRecoveryCode(const std::string& code);

private:
    /// 动态截断（RFC 4226 §5.4）
    static uint32_t DynamicTruncation(const std::string& hmacResult);

    /// 生成指定时间步的 TOTP 值
    static uint32_t GenerateTotp(const std::string& secret, int64_t timeStep);
};

} // namespace blog
