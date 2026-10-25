# Passkey 登录（阶段 3）

## 流程

```
未登录用户
  │
  ├─ POST /api/v1/auth/passkey/login/begin
  │      ├─ 不带 auth_id：discoverable credential 登录
  │      └─ 带 auth_id：只允许该账号已注册的 Passkey
  │
  ├─ navigator.credentials.get({ publicKey })
  │
  └─ POST /api/v1/auth/passkey/login/finish
         body: PublicKeyCredential.toJSON()
         └─ 校验签名并签发 SESSIONID
```

## 接口

### begin

`POST /api/v1/auth/passkey/login/begin`

请求参数：

| 参数      | 必填 | 说明                                                |
|-----------|------|-----------------------------------------------------|
| `auth_id` | 否   | 邮箱或账号；不传时使用 discoverable credential 登录 |

响应 `data` 直接是 WebAuthn 的 `PublicKeyCredentialRequestOptions`：

```json
{
  "challenge": "...",
  "rpId": "example.com",
  "timeout": 60000,
  "userVerification": "preferred",
  "allowCredentials": [
    { "type": "public-key", "id": "..." }
  ]
}
```

- 传入 `auth_id` 时，`allowCredentials` 只包含该用户的 Passkey；该账号没有 Passkey 时返回 `PASSKEY_NOT_FOUND`。
- 不传 `auth_id` 时不返回 `allowCredentials`，由浏览器选择该 RP 下的 discoverable credential；这依赖注册时认证器创建了
  resident key。

### finish

`POST /api/v1/auth/passkey/login/finish`

请求体兼容浏览器 `PublicKeyCredential.toJSON()`：

```json
{
  "id": "...",
  "rawId": "...",
  "type": "public-key",
  "response": {
    "clientDataJSON": "...",
    "authenticatorData": "...",
    "signature": "...",
    "userHandle": "..."
  }
}
```

成功响应：

```json
{
  "errno": 0,
  "msg": "ok",
  "data": {
    "id": 1,
    "account": "alice",
    "name": "Alice",
    "avatar": "...",
    "role": 1,
    "user_verified": true
  }
}
```

成功后服务端设置 `SESSIONID` Cookie，前端不需要读取或保存任何 token。

## 服务端校验

1. `clientDataJSON.type == "webauthn.get"`
2. `clientDataJSON.challenge` 必须命中 Redis 中一次性 challenge
3. `clientDataJSON.origin` 必须等于 `webauthn.origin`
4. `crossOrigin` 不能为 true
5. `authData.rpIdHash == SHA256(webauthn.rp_id)`
6. flags 必须包含 `UP`（`user_verification=required` 时还要求 `UV`）
7. `rawId` 必须能匹配数据库中的 Passkey
8. 如果返回 `userHandle`，必须与 `webauthn.user_handle_secret` 生成的稳定 handle 一致
9. 使用保存的 COSE 公钥验证 `authenticatorData || SHA256(clientDataJSON)` 的签名
10. 签名计数器回退时拒绝登录（计数器为 0 的认证器除外）

支持算法：

| COSE alg | 算法  |
|----------|-------|
| `-7`     | ES256 |
| `-257`   | RS256 |
| `-8`     | EdDSA |

## 前端示例

```js
const begin = await fetch('/api/v1/auth/passkey/login/begin', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ auth_id: 'alice' })
}).then(r => r.json());

if (begin.errno !== 0) throw new Error(begin.msg);

const publicKey = PublicKeyCredential.parseRequestOptionsFromJSON
  ? PublicKeyCredential.parseRequestOptionsFromJSON(begin.data)
  : decodeRequestOptions(begin.data); // 兼容旧浏览器：base64url -> ArrayBuffer

const credential = await navigator.credentials.get({ publicKey });

const finish = await fetch('/api/v1/auth/passkey/login/finish', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify(credential.toJSON())
}).then(r => r.json());

if (finish.errno !== 0) throw new Error(finish.msg);
```

> 如果浏览器不支持 `parseRequestOptionsFromJSON` / `toJSON`，前端需要自行做 base64url ↔ ArrayBuffer 转换。

## 与 2FA 的关系

Passkey 通常包含“持有因子 + 本地用户验证”，因此当前 Passkey 登录不再叠加 TOTP。启用 TOTP 的账号仍可使用 Passkey
直接登录；如果希望强制本地验证，请把 `webauthn.user_verification` 配为 `required`。

## 相关错误码

| 错误码 | key                           | 说明                       |
|--------|-------------------------------|----------------------------|
| 2029   | `PASSKEY_NOT_FOUND`           | 凭据不存在或不属于当前用户 |
| 2032   | `WEBAUTHN_NOT_CONFIGURED`     | WebAuthn 未配置            |
| 2033   | `WEBAUTHN_CHALLENGE_INVALID`  | challenge 无效或已过期     |
| 2034   | `WEBAUTHN_ORIGIN_MISMATCH`    | origin 不匹配              |
| 2035   | `WEBAUTHN_RP_ID_MISMATCH`     | rpId 不匹配                |
| 2036   | `WEBAUTHN_INVALID_CREDENTIAL` | 凭据格式或 userHandle 无效 |
| 2037   | `WEBAUTHN_UNSUPPORTED_ALG`    | 公钥算法不支持             |
| 2038   | `WEBAUTHN_VERIFY_FAILED`      | 签名或 flags 校验失败      |

## 凭据管理

- `POST /api/v1/auth/passkey/list`：列出当前用户的 Passkey。
- `POST /api/v1/auth/passkey/delete`：按 `id` 软删除当前用户的 Passkey。

`list` 返回：

```json
{
  "errno": 0,
  "data": {
    "total": 1,
    "list": [
      {
        "id": 1,
        "name": "MacBook Touch ID",
        "alg": -7,
        "aaguid": "...",
        "transports": "internal,hybrid",
        "backup_eligible": 1,
        "backup_state": 0,
        "created_at": 1790000000,
        "last_used_at": 1790000000
      }
    ]
  }
}
```

删除操作只允许操作自己的凭据；传入其他用户的 `id` 返回 `PASSKEY_NOT_FOUND`。删除采用软删除；同一账号重新注册同一 credential
时，服务端会复用旧记录并恢复为有效状态。
