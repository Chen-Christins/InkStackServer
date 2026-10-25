/**
 * @file cbor.h
 * @brief WebAuthn 使用的最小 CBOR 解码器
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <string>
#include <vector>

namespace blog {
namespace webauthn {

enum class CborType {
    Invalid = 0,
    Unsigned,
    Negative,
    ByteString,
    TextString,
    Array,
    Map,
    Bool,
    Null,
    Float,
};

/**
 * @brief CBOR 值
 */
struct CborValue {
    CborType type = CborType::Invalid;
    int64_t int_value = 0;
    bool bool_value = false;
    double float_value = 0.0;
    std::string bytes;
    std::vector<CborValue> array;
    std::vector<CborValue> map_keys;
    std::vector<CborValue> map_values;

    /**
     * @brief 按文本 key 查 map
     */
    const CborValue* Get(const std::string& key) const;

    /**
     * @brief 按整数 key 查 map（COSE_Key 使用）
     */
    const CborValue* Get(int64_t key) const;

    bool IsInteger() const { return type == CborType::Unsigned || type == CborType::Negative; }
};

/**
 * @brief 解码一段 CBOR
 * @param data 输入数据
 * @param out 输出值
 * @param consumed 实际消费的字节数
 * @return 是否成功
 */
bool DecodeCbor(const std::string& data, CborValue& out, size_t& consumed);

/**
 * @brief 解码一段 CBOR，要求全部消费
 */
bool DecodeCbor(const std::string& data, CborValue& out);

} // namespace webauthn
} // namespace blog
