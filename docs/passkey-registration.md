# Passkey 注册（阶段 2）

## 流程

```
已登录用户
  │
  ├─ POST /api/v1/auth/passkey/register/begin
  │      └─ 返回 PublicKeyCredentialCreationOptions
  │
  ├─ navigator.credentials.create({ publicKey })
  │
  └─ POST /api/v1/auth/passkey/register/finish
         body: PublicKeyCredential.toJSON()
         └─ 校验并写入 user_passkey
```

## 接口

### begin

`POST /api/v1/auth/passkey/register/begin`

请求参数：

| 参数   | 必填 | 说明                            |
|--------|------|---------------------------------|
| `name` | 否   | 凭据名称，默认使用浏览器/系统名 |

响应 `data` 直接是 WebAuthn 的 `PublicKeyCredentialCreationOptions`：

```json
{
  "challenge": "...",
  "rp": { "id": "example.com", "name": "InkStack" },
  "user": { "id": "...", "name": "alice", "displayName": "Alice" },
  "pubKeyCredParams": [
    { "type": "public-key", "alg": -7 },
    { "type": "public-key", "alg": -257 },
    { "type": "public-key", "alg": -8 }
  ],
  "timeout": 60000,
  "attestation": "none",
  "authenticatorSelection": {
    "residentKey": "preferred",
    "userVerification": "preferred"
  },
  "excludeCredentials": [
    { "type": "public-key", "id": "..." }
  ]
}
```

### finish

`POST /api/v1/auth/passkey/register/finish`

请求体兼容浏览器 `PublicKeyCredential.toJSON()`：

```json
{
  "id": "...",
  "rawId": "...",
  "type": "public-key",
  "response": {
    "clientDataJSON": "...",
    "attestationObject": "...",
    "transports": ["internal", "hybrid"]
  },
  "name": "MacBook Touch ID"
}
```

响应：

```json
{
  "errno": 0,
  "msg": "ok",
  "data": {
    "id": 1,
    "name": "MacBook Touch ID",
    "credential_id": "..."
  }
}
```

## 前端示例

```js
const begin = await fetch('/api/v1/auth/passkey/register/begin', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ name: 'MacBook Touch ID' })
}).then(r => r.json());

if (begin.errno !== 0) throw new Error(begin.msg);

const publicKey = PublicKeyCredential.parseCreationOptionsFromJSON
  ? PublicKeyCredential.parseCreationOptionsFromJSON(begin.data)
  : decodeCreationOptions(begin.data); // 兼容旧浏览器：base64url -> ArrayBuffer

const credential = await navigator.credentials.create({ publicKey });

const finish = await fetch('/api/v1/auth/passkey/register/finish', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ ...credential.toJSON(), name: 'MacBook Touch ID' })
}).then(r => r.json());

if (finish.errno !== 0) throw new Error(finish.msg);
```

> 如果浏览器不支持 `parseCreationOptionsFromJSON` / `toJSON`，前端需要自行做 base64url ↔ ArrayBuffer 转换。

## 服务端校验

- `clientDataJSON.type == "webauthn.create"`
- `clientDataJSON.challenge` 必须命中 Redis 中该用户的一次性 challenge
- `clientDataJSON.origin` 必须等于 `webauthn.origin`
- `crossOrigin` 不能为 true
- `authData.rpIdHash == SHA256(webauthn.rp_id)`
- flags 必须包含 `UP`（`user_verification=required` 时还要求 `UV`）
- 必须包含 attested credential data（`AT`）
- `rawId` 必须与 authData 中的 credential ID 一致
- COSE 公钥算法支持 ES256 / RS256 / EdDSA
- 同一 credential ID 不能重复注册

当前请求 `attestation=none`，服务端不建立 attestation 证书链信任，只校验 authData 与公钥。后续如需企业级认证器白名单，再单独引入
attestation 校验。

## 相关配置

见 `CONFIG.md` 的 WebAuthn 章节，重点是 `rp_id`、`origin` 和 `user_handle_secret`。

Passkey 登录、列表与删除见 `docs/passkey-login.md`。
