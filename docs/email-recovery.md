# 邮箱验证码恢复登录（阶段 3）

## 目标

当用户丢失 Passkey、密码不可用或无法完成常规登录时，通过邮箱验证码恢复账号访问，并获得新的 `SESSIONID` 会话。

## 流程

```
POST /api/v1/user/send_code        type=5
  └─ 发送 6 位验证码到邮箱，Redis 有效期 10 分钟

POST /api/v1/auth/email/recovery
  ├─ 校验 email + auth_code
  ├─ 撤销该用户全部旧会话
  └─ 签发新的 SESSIONID
```

## 发送验证码

复用现有接口：

`POST /api/v1/user/send_code`

| 参数    | 必填 | 说明                         |
|---------|------|------------------------------|
| `email` | 是   | 账号邮箱                     |
| `type`  | 是   | 固定为 `5`（账号恢复）       |
| `agent` | 是   | 现有接口要求非空，可传 `web` |

与注册/重置密码共用 IP 限流：每 IP 每小时最多 5 次。

## 恢复登录

`POST /api/v1/auth/email/recovery`

请求参数：

| 参数        | 必填 | 说明       |
|-------------|------|------------|
| `email`     | 是   | 账号邮箱   |
| `auth_code` | 是   | 邮箱验证码 |

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
    "role": 1
  }
}
```

成功后：

- 删除该邮箱的一次性验证码；
- 清理验证码错误计数；
- 撤销该用户全部旧会话（包括旧设备 token）；
- 签发新的 `SESSIONID` Cookie。

## 安全措施

- 验证码为 6 位随机英数串，10 分钟过期，一次性使用。
- 连续输错 5 次后返回 `EMAIL_CODE_ATTEMPTS_EXCEEDED`，错误计数 10 分钟后自动过期。
- 恢复成功会撤销全部旧会话，避免旧设备继续访问。
- 邮箱恢复作为账号找回通道，不叠加 TOTP；请确保邮箱本身的安全（强密码、二次验证）。
- 恢复登录成功后，用户应尽快在 Passkey 管理页注册新的 Passkey，并删除丢失设备的旧凭据。

## 错误码

| 错误码 | key                            | 说明               |
|--------|--------------------------------|--------------------|
| 2005   | `USER_INVALID_EMAIL`           | 邮箱格式错误       |
| 2011   | `USER_EMAIL_NOT_REGISTER`      | 邮箱未注册         |
| 1103   | `ACCOUNT_INVALID`              | 账号被封禁或删除   |
| 1104   | `AUTH_CODE_INVALID`            | 验证码错误或已过期 |
| 2039   | `EMAIL_CODE_ATTEMPTS_EXCEEDED` | 验证码尝试次数过多 |
| 9008   | `SMTP_NOT_CONFIGURED`          | 邮件服务未配置     |
| 9009   | `REDIS_OPERATION_FAILED`       | 缓存服务操作失败   |

## 前端示例

```js
await fetch('/api/v1/user/send_code', {
    method: 'POST',
    credentials: 'include',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({email, type: '5', agent: 'web'})
});

const result = await fetch('/api/v1/auth/email/recovery', {
    method: 'POST',
    credentials: 'include',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({email, auth_code: code})
}).then(r => r.json());

if (result.errno !== 0) throw new Error(result.msg);
```
