#include "common/util/TotpUtil.hpp"

#include <chen/util/encryptor_util.h>
#include <chen/util/string_util.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>

namespace blog {

/// TOTP 时间步长（秒）
static const int kTimeStep = 30;
/// TOTP 数字位数
static const int kDigits = 6;
/// 恢复码字符集（去掉容易混淆的 0/O/1/I/l）
static const char kRecoveryChars[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

std::string TotpUtil::GenerateSecret()
{
    std::string randomData = chen::EncryptorUtil::GenerateRandomKey(20);
    return chen::StringUtil::Base32Encode(randomData);
}

uint32_t TotpUtil::DynamicTruncation(const std::string& hmacResult)
{
    if (hmacResult.size() < 20) {
        return 0;
    }
    int offset = hmacResult[19] & 0x0F;
    uint32_t code = (static_cast<uint32_t>(hmacResult[offset] & 0x7F) << 24) | (static_cast<uint32_t>(hmacResult[offset + 1] & 0xFF) << 16)
        | (static_cast<uint32_t>(hmacResult[offset + 2] & 0xFF) << 8) | (static_cast<uint32_t>(hmacResult[offset + 3] & 0xFF));
    return code;
}

uint32_t TotpUtil::GenerateTotp(const std::string& secret, int64_t timeStep)
{
    // 将时间步转为 8 字节大端序
    uint8_t timeBytes[8];
    uint64_t t = static_cast<uint64_t>(timeStep);
    for (int i = 7; i >= 0; --i) {
        timeBytes[i] = t & 0xFF;
        t >>= 8;
    }
    std::string timeMessage(reinterpret_cast<char*>(timeBytes), 8);

    // Base32 解码密钥
    std::string key = chen::StringUtil::Base32Decode(secret);
    std::string hmac = chen::EncryptorUtil::HMAC_SHA1(key, timeMessage);

    uint32_t truncated = DynamicTruncation(hmac);
    return truncated % (1u << (kDigits * 3)); // 10^6
}

bool TotpUtil::VerifyCode(const std::string& secret, const std::string& code, int window)
{
    if (secret.empty() || code.size() != kDigits) {
        return false;
    }
    // 验证码必须全是数字
    for (char c : code) {
        if (c < '0' || c > '9') {
            return false;
        }
    }

    int64_t now = time(nullptr);
    int64_t currentTimeStep = now / kTimeStep;

    for (int i = -window; i <= window; ++i) {
        uint32_t expected = GenerateTotp(secret, currentTimeStep + i);
        // 格式化为 6 位数字（前导零）
        char expectedStr[8];
        snprintf(expectedStr, sizeof(expectedStr), "%06u", expected);
        if (code == expectedStr) {
            return true;
        }
    }
    return false;
}

std::string TotpUtil::GetOtpUri(const std::string& secret, const std::string& account, const std::string& issuer)
{
    std::stringstream ss;
    ss << "otpauth://totp/" << issuer << ":" << account << "?secret=" << secret << "&issuer=" << issuer << "&algorithm=SHA1"
       << "&digits=" << kDigits << "&period=" << kTimeStep;
    return ss.str();
}

std::string TotpUtil::HashRecoveryCode(const std::string& code)
{
    return chen::EncryptorUtil::SHA256(code);
}

std::vector<std::string> TotpUtil::GenerateRecoveryCodes(int count, std::vector<std::string>& hashedCodes)
{
    std::vector<std::string> codes;
    hashedCodes.clear();
    hashedCodes.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(kRecoveryChars) - 2);

    for (int i = 0; i < count; ++i) {
        std::string code;
        code.reserve(10);
        for (int j = 0; j < 10; ++j) {
            code += kRecoveryChars[dis(gen)];
        }
        // 格式化为 XXXXX-XXXXX
        code = code.substr(0, 5) + "-" + code.substr(5);
        codes.push_back(code);
        hashedCodes.push_back(HashRecoveryCode(code));
    }
    return codes;
}

} // namespace blog
