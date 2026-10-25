#include "cose.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>

#include "cbor.h"

namespace blog {
namespace webauthn {

// SDK 只封装了 RSA-SHA256(PEM) 验签；WebAuthn 还需要 ES256/EdDSA 和 COSE 原始公钥，
// 因此这里直接使用 OpenSSL，避免把 EVP_PKEY 再转成 PEM。
namespace {

constexpr int32_t kCoseKtyOkp = 1;
constexpr int32_t kCoseKtyEc2 = 2;
constexpr int32_t kCoseKtyRsa = 3;
constexpr int32_t kCoseAlgEs256 = -7;
constexpr int32_t kCoseAlgEdDsa = -8;
constexpr int32_t kCoseAlgRs256 = -257;
constexpr int32_t kCoseCrvP256 = 1;
constexpr int32_t kCoseCrvEd25519 = 6;

std::shared_ptr<EVP_PKEY> BuildEcP256Key(const std::string& x, const std::string& y)
{
    if (x.size() != 32 || y.size() != 32) {
        return nullptr;
    }

    EC_KEY* ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec) {
        return nullptr;
    }

    const EC_GROUP* group = EC_KEY_get0_group(ec);
    EC_POINT* point = group ? EC_POINT_new(group) : nullptr;
    BIGNUM* x_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(x.data()), static_cast<int>(x.size()), nullptr);
    BIGNUM* y_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(y.data()), static_cast<int>(y.size()), nullptr);

    bool ok = group && point && x_bn && y_bn && EC_POINT_set_affine_coordinates(group, point, x_bn, y_bn, nullptr) == 1
        && EC_POINT_is_on_curve(group, point, nullptr) == 1 && EC_KEY_set_public_key(ec, point) == 1;

    BN_free(x_bn);
    BN_free(y_bn);
    EC_POINT_free(point);

    if (!ok) {
        EC_KEY_free(ec);
        return nullptr;
    }

    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, ec) != 1) {
        EVP_PKEY_free(pkey);
        EC_KEY_free(ec);
        return nullptr;
    }
    return std::shared_ptr<EVP_PKEY>(pkey, EVP_PKEY_free);
}

std::shared_ptr<EVP_PKEY> BuildRsaKey(const std::string& n, const std::string& e)
{
    if (n.size() < 256 || e.empty()) {
        return nullptr;
    }

    RSA* rsa = RSA_new();
    if (!rsa) {
        return nullptr;
    }

    BIGNUM* n_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(n.data()), static_cast<int>(n.size()), nullptr);
    BIGNUM* e_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(e.data()), static_cast<int>(e.size()), nullptr);
    if (!n_bn || !e_bn || RSA_set0_key(rsa, n_bn, e_bn, nullptr) != 1) {
        BN_free(n_bn);
        BN_free(e_bn);
        RSA_free(rsa);
        return nullptr;
    }

    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey || EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
        EVP_PKEY_free(pkey);
        RSA_free(rsa);
        return nullptr;
    }
    return std::shared_ptr<EVP_PKEY>(pkey, EVP_PKEY_free);
}

std::shared_ptr<EVP_PKEY> BuildEd25519Key(const std::string& x)
{
    if (x.size() != 32) {
        return nullptr;
    }
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char*>(x.data()), x.size());
    if (!pkey) {
        return nullptr;
    }
    return std::shared_ptr<EVP_PKEY>(pkey, EVP_PKEY_free);
}

} // namespace

bool ParseCoseKey(const std::string& cbor, CoseKey& out)
{
    out = CoseKey();

    CborValue root;
    if (!DecodeCbor(cbor, root) || root.type != CborType::Map) {
        return false;
    }

    const CborValue* kty = root.Get(1);
    const CborValue* alg = root.Get(3);
    const CborValue* crv = root.Get(-1);
    const CborValue* x = root.Get(-2);
    const CborValue* y = root.Get(-3);
    const CborValue* n = root.Get(-1);
    const CborValue* e = root.Get(-2);

    if (!kty || !kty->IsInteger() || !alg || !alg->IsInteger()) {
        return false;
    }

    out.kty = static_cast<int32_t>(kty->int_value);
    out.alg = static_cast<int32_t>(alg->int_value);

    switch (out.kty) {
    case kCoseKtyEc2: {
        if (out.alg != kCoseAlgEs256 || !crv || !crv->IsInteger() || crv->int_value != kCoseCrvP256 || !x || !y) {
            return false;
        }
        if (x->type != CborType::ByteString || y->type != CborType::ByteString) {
            return false;
        }
        out.crv = static_cast<int32_t>(crv->int_value);
        out.pkey = BuildEcP256Key(x->bytes, y->bytes);
        return out.pkey != nullptr;
    }
    case kCoseKtyRsa: {
        if (out.alg != kCoseAlgRs256 || !n || !e) {
            return false;
        }
        if (n->type != CborType::ByteString || e->type != CborType::ByteString) {
            return false;
        }
        out.pkey = BuildRsaKey(n->bytes, e->bytes);
        return out.pkey != nullptr;
    }
    case kCoseKtyOkp: {
        if (out.alg != kCoseAlgEdDsa || !crv || !crv->IsInteger() || crv->int_value != kCoseCrvEd25519 || !x) {
            return false;
        }
        if (x->type != CborType::ByteString) {
            return false;
        }
        out.crv = static_cast<int32_t>(crv->int_value);
        out.pkey = BuildEd25519Key(x->bytes);
        return out.pkey != nullptr;
    }
    default:
        return false;
    }
}

bool VerifySignature(const CoseKey& key, const std::string& data, const std::string& signature)
{
    if (!key.pkey || signature.empty()) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    EVP_PKEY_CTX* pctx = nullptr;
    int init_ret = 0;
    if (key.kty == kCoseKtyOkp) {
        // Ed25519 是纯签名算法，不能传摘要。
        init_ret = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key.pkey.get());
    } else {
        init_ret = EVP_DigestVerifyInit(ctx, &pctx, EVP_sha256(), nullptr, key.pkey.get());
        if (init_ret == 1 && key.kty == kCoseKtyRsa && pctx) {
            init_ret = EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING);
        }
    }

    int verify_ret = 0;
    if (init_ret == 1) {
        verify_ret = EVP_DigestVerify(
            ctx, reinterpret_cast<const unsigned char*>(signature.data()), signature.size(), reinterpret_cast<const unsigned char*>(data.data()),
            data.size());
    }
    EVP_MD_CTX_free(ctx);
    return verify_ret == 1;
}

} // namespace webauthn
} // namespace blog
