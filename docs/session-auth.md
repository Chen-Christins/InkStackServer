# 会话认证（阶段 1）

## 目标

把登录态从“设备 token + 双 Cookie”收敛为 **服务端 Session + 单个 HttpOnly Cookie**，为后续 Passkey 登录做准备。

```
登录成功 ──▶ AuthService::IssueSession ──▶ Redis Session
                                              │
浏览器 ◀── SESSIONID (HttpOnly) ◀─────────────┘
```

## Cookie

| 项   | 值                                                    |
|------|-------------------------------------------------------|
| 名称 | `SESSIONID`                                           |
| 值   | 32 字节 CSPRNG 的十六进制串                           |
| 属性 | `Path=/; HttpOnly; SameSite=Lax`，生产环境加 `Secure` |
| 存储 | Redis key 使用 `SHA256(SESSIONID)`，不落原始值        |

配置项：

```yaml
auth:
  cookie_secure: false        # 生产必须 true
  cookie_same_site: Lax       # Lax / Strict / None
  session_absolute_ttl: 604800
  ws_legacy_token: true
```

空闲有效期沿用系统设置里的“会话时长”（默认 24 小时），每次活跃会滑动续期；绝对有效期由 `auth.session_absolute_ttl` 控制。

> 前后端跨域部署时，Cookie 需要 `SameSite=None; Secure`，并让后端 CORS 允许 credentials；同源或经前端代理时保持 `Lax` 即可。

## Redis 结构

| key                       | 类型 | 内容                                                       |
|---------------------------|------|------------------------------------------------------------|
| `auth:sess:<sha256>`      | hash | `uid`、`ip`、`ua`、`created_at`、`last_seen`、`expires_at` |
| `auth:uid:<uid>:sessions` | zset | member 为 `sha256`，score 为过期时间                       |

- `SessionStore::Create` 创建会话。
- `SessionStore::Get` 校验过期时间。
- `SessionStore::Touch` 滑动续期（60 秒节流）。
- `SessionStore::Destroy` / `DestroyAll` 撤销单个或全部会话。
- `SessionStore::List` 供后续“登录设备/会话列表”使用。

## 请求流程

1. `BlogServlet::handlePre` 发现请求带登录 Cookie 时调用 `initLogin`。
2. `AuthService::Resolve` 读取 `SESSIONID`，查 Redis，校验用户存在且状态为 `ACTIVE`、未删除。
3. 校验通过后把 `uid` 映射到框架 `SessionData`，兼容 2FA 临时密钥等旧逻辑。
4. `BlogLoginedServlet` 要求解析成功；`BlogAdminServlet` 再校验 `ADMIN`。
5. `getUserId(request)` 统一走 `AuthService::Resolve`，不再信任内存中的旧 uid。

## 签发与撤销

| 场景                       | 调用                                              |
|----------------------------|---------------------------------------------------|
| 密码登录                   | `AuthService::IssueSession`                       |
| Passkey 登录               | `AuthService::IssueSession`                       |
| 2FA 验证 / 恢复码登录      | `AuthService::IssueSession`                       |
| 邮箱验证码恢复             | `AuthService::RevokeAllSessions` + `IssueSession` |
| GitHub OAuth 回调          | `AuthService::IssueSession`                       |
| 登出                       | `AuthService::RevokeSession`                      |
| 改密                       | `AuthService::RevokeAllSessions`                  |
| 后台重置密码 / 封禁 / 删除 | `AuthService::RevokeAllSessions`                  |

`IssueSession` 会生成新的 `SESSIONID`，删除旧会话，从而避免会话固定。新登录不再签发 `S_UID` / `S_TOKEN` / `S_TOKEN_TIME` /
`DEVICE_ID`。

## 迁移兼容

- 旧 Cookie 仍可自动登录：`AuthService::UpgradeLegacySession` 校验 `S_UID + S_TOKEN + S_TOKEN_TIME`，成功后换成新的
  `SESSIONID`。
- WebSocket 优先使用握手 Cookie；`auth.ws_legacy_token=true` 时兼容旧 `uid/token/token_time` query。
- `user_device_list` / `user_device_kick` 已切换为会话列表，并保留旧设备 token 的兜底逻辑。
- `user_refresh_token` 在新会话下直接返回成功（无需刷新 token），旧 token 仍可正常轮换。
- 迁移期结束后：
    - 关闭 `auth.ws_legacy_token`；
    - 删除旧 token / 设备表相关代码；
    - 删除旧 `session:` Redis 会话。

## 后续

- 阶段 2：Passkey 注册（已完成）。
- 阶段 3：Passkey 登录、邮箱验证码恢复、Passkey 管理（已完成）；会话列表/下线沿用阶段 1 的
  `user/device/list`、`user/device/kick`。
  - `docs/passkey-login.md`
  - `docs/email-recovery.md`
- 阶段 4：关闭密码登录、移除旧 token/设备体系。
