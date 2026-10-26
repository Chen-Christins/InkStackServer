#include "cose.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
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
constexpr const char* kEcGroupName = "prime256v1";

bool CheckPublicKey(EVP_PKEY* pkey)
{
    if (!pkey) {
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
    if (!ctx) {
        return false;
    }
    bool ok = EVP_PKEY_public_check(ctx) == 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

std::shared_ptr<EVP_PKEY> BuildEcP256Key(const std::string& x, const std::string& y)
{
    if (x.size() != 32 || y.size() != 32) {
        return nullptr;
    }

    // WebAuthn COSE EC2 公钥使用未压缩点格式：0x04 || x || y。
    std::string point;
    point.reserve(1 + x.size() + y.size());
    point.push_back(static_cast<char>(0x04));
    point.append(x);
    point.append(y);

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM* params = nullptr;
    EVP_PKEY_CTX* ctx = nullptr;
    EVP_PKEY* pkey = nullptr;
    bool ok = bld != nullptr;

    if (ok) {
        ok = OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME, kEcGroupName, 0) == 1
            && OSSL_PARAM_BLD_push_octet_string(bld, OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size()) == 1;
    }
    if (ok) {
        params = OSSL_PARAM_BLD_to_param(bld);
        ok = params != nullptr;
    }
    if (ok) {
        ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
        ok = ctx != nullptr;
    }
    if (ok) {
        ok = EVP_PKEY_fromdata_init(ctx) == 1 && EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1;
    }
    if (ok) {
        ok = CheckPublicKey(pkey);
    }

    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    EVP_PKEY_CTX_free(ctx);

    if (!ok) {
        EVP_PKEY_free(pkey);
        return nullptr;
    }
    return std::shared_ptr<EVP_PKEY>(pkey, EVP_PKEY_free);
}

std::shared_ptr<EVP_PKEY> BuildRsaKey(const std::string& n, const std::string& e)
{
    if (n.size() < 256 || e.empty()) {
        return nullptr;
    }

    BIGNUM* n_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(n.data()), static_cast<int>(n.size()), nullptr);
    BIGNUM* e_bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(e.data()), static_cast<int>(e.size()), nullptr);
    if (!n_bn || !e_bn) {
        BN_free(n_bn);
        BN_free(e_bn);
        return nullptr;
    }

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM* params = nullptr;
    EVP_PKEY_CTX* ctx = nullptr;
    EVP_PKEY* pkey = nullptr;
    bool ok = bld != nullptr;

    if (ok) {
        ok = OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, n_bn) == 1 && OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, e_bn) == 1;
    }
    if (ok) {
        params = OSSL_PARAM_BLD_to_param(bld);
        ok = params != nullptr;
    }
    if (ok) {
        ctx = EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);
        ok = ctx != nullptr;
    }
    if (ok) {
        ok = EVP_PKEY_fromdata_init(ctx) == 1 && EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1;
    }
    if (ok) {
        ok = CheckPublicKey(pkey);
    }

    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    EVP_PKEY_CTX_free(ctx);
    BN_free(n_bn);
    BN_free(e_bn);

    if (!ok) {
        EVP_PKEY_free(pkey);
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
