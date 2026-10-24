#include "cbor.h"

#include <algorithm>
#include <cstdint>
#include <limits>

// SDK 没有 WebAuthn 使用的 CBOR 解码器；这里只实现 attestationObject/authData
// 需要的最小子集，遇到不支持的容器会直接拒绝。
namespace blog {
namespace webauthn {

namespace {

constexpr int kMaxDepth = 16;
constexpr uint64_t kMaxContainerSize = 4096;

bool ReadByte(const std::string& data, size_t& pos, uint8_t& value)
{
    if (pos >= data.size()) {
        return false;
    }
    value = static_cast<uint8_t>(data[pos++]);
    return true;
}

bool ReadUint(const std::string& data, size_t& pos, size_t count, uint64_t& value)
{
    if (count > 8 || pos + count > data.size()) {
        return false;
    }
    value = 0;
    for (size_t i = 0; i < count; ++i) {
        value = (value << 8) | static_cast<uint8_t>(data[pos++]);
    }
    return true;
}

bool ReadLength(const std::string& data, size_t& pos, uint8_t additional, uint64_t& value)
{
    if (additional < 24) {
        value = additional;
        return true;
    }
    switch (additional) {
    case 24:
        return ReadUint(data, pos, 1, value);
    case 25:
        return ReadUint(data, pos, 2, value);
    case 26:
        return ReadUint(data, pos, 4, value);
    case 27:
        return ReadUint(data, pos, 8, value);
    default:
        return false;
    }
}

bool DecodeItem(const std::string& data, size_t& pos, CborValue& out, int depth)
{
    if (depth > kMaxDepth) {
        return false;
    }

    uint8_t initial = 0;
    if (!ReadByte(data, pos, initial)) {
        return false;
    }

    uint8_t major = initial >> 5;
    uint8_t additional = initial & 0x1F;

    switch (major) {
    case 0: {
        uint64_t value = 0;
        if (!ReadLength(data, pos, additional, value)) {
            return false;
        }
        if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return false;
        }
        out.type = CborType::Unsigned;
        out.int_value = static_cast<int64_t>(value);
        return true;
    }
    case 1: {
        uint64_t value = 0;
        if (!ReadLength(data, pos, additional, value)) {
            return false;
        }
        if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return false;
        }
        out.type = CborType::Negative;
        out.int_value = -1 - static_cast<int64_t>(value);
        return true;
    }
    case 2:
    case 3: {
        uint64_t length = 0;
        if (!ReadLength(data, pos, additional, length)) {
            return false;
        }
        if (length > data.size() - pos) {
            return false;
        }
        out.type = major == 2 ? CborType::ByteString : CborType::TextString;
        out.bytes.assign(data.data() + pos, static_cast<size_t>(length));
        pos += static_cast<size_t>(length);
        return true;
    }
    case 4: {
        uint64_t length = 0;
        if (!ReadLength(data, pos, additional, length)) {
            return false;
        }
        if (length > kMaxContainerSize) {
            return false;
        }
        out.type = CborType::Array;
        out.array.clear();
        out.array.reserve(static_cast<size_t>(length));
        for (uint64_t i = 0; i < length; ++i) {
            CborValue item;
            if (!DecodeItem(data, pos, item, depth + 1)) {
                return false;
            }
            out.array.push_back(std::move(item));
        }
        return true;
    }
    case 5: {
        uint64_t length = 0;
        if (!ReadLength(data, pos, additional, length)) {
            return false;
        }
        if (length > kMaxContainerSize) {
            return false;
        }
        out.type = CborType::Map;
        out.map_keys.clear();
        out.map_values.clear();
        out.map_keys.reserve(static_cast<size_t>(length));
        out.map_values.reserve(static_cast<size_t>(length));
        for (uint64_t i = 0; i < length; ++i) {
            CborValue key;
            CborValue value;
            if (!DecodeItem(data, pos, key, depth + 1) || !DecodeItem(data, pos, value, depth + 1)) {
                return false;
            }
            out.map_keys.push_back(std::move(key));
            out.map_values.push_back(std::move(value));
        }
        return true;
    }
    case 6: {
        uint64_t tag = 0;
        if (!ReadLength(data, pos, additional, tag)) {
            return false;
        }
        // 标签本身不参与 WebAuthn 校验，直接解码被标签包裹的值
        return DecodeItem(data, pos, out, depth + 1);
    }
    case 7: {
        if (additional == 20) {
            out.type = CborType::Bool;
            out.bool_value = false;
            return true;
        }
        if (additional == 21) {
            out.type = CborType::Bool;
            out.bool_value = true;
            return true;
        }
        if (additional == 22) {
            out.type = CborType::Null;
            return true;
        }
        if (additional == 25 || additional == 26 || additional == 27) {
            // 只消费浮点字节，WebAuthn 注册校验不需要具体数值
            size_t count = additional == 25 ? 2 : (additional == 26 ? 4 : 8);
            uint64_t raw = 0;
            if (!ReadUint(data, pos, count, raw)) {
                return false;
            }
            (void)raw;
            out.type = CborType::Float;
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

} // namespace

const CborValue* CborValue::Get(const std::string& key) const
{
    if (type != CborType::Map) {
        return nullptr;
    }
    size_t count = std::min(map_keys.size(), map_values.size());
    for (size_t i = 0; i < count; ++i) {
        if (map_keys[i].type == CborType::TextString && map_keys[i].bytes == key) {
            return &map_values[i];
        }
    }
    return nullptr;
}

const CborValue* CborValue::Get(int64_t key) const
{
    if (type != CborType::Map) {
        return nullptr;
    }
    size_t count = std::min(map_keys.size(), map_values.size());
    for (size_t i = 0; i < count; ++i) {
        if (map_keys[i].IsInteger() && map_keys[i].int_value == key) {
            return &map_values[i];
        }
    }
    return nullptr;
}

bool DecodeCbor(const std::string& data, CborValue& out, size_t& consumed)
{
    consumed = 0;
    out = CborValue();
    size_t pos = 0;
    if (!DecodeItem(data, pos, out, 0)) {
        return false;
    }
    consumed = pos;
    return true;
}

bool DecodeCbor(const std::string& data, CborValue& out)
{
    size_t consumed = 0;
    if (!DecodeCbor(data, out, consumed)) {
        return false;
    }
    return consumed == data.size();
}

} // namespace webauthn
} // namespace blog
