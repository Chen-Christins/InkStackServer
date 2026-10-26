# Changelog

All notable changes to this project will be documented in this file.

## v1.1.0 (2026-10-26)

本版本完成认证体系升级：登录态统一为服务端会话 + HttpOnly `SESSIONID` Cookie，新增 Passkey / WebAuthn、邮箱验证码恢复与恢复码流程，并移除旧
`S_UID` / `S_TOKEN` 认证链路。同时引入 Redis 对象缓存、崩溃保护和完整国际化。

### 新增

- 新增 Passkey 认证：
  - Passkey 注册（`/api/v1/auth/passkey/register/*`）
  - Passkey 登录（`/api/v1/auth/passkey/login/*`），支持 discoverable credential
  - Passkey 列表与删除（`/api/v1/auth/passkey/list`、`/api/v1/auth/passkey/delete`）
  - 支持 ES256、EdDSA、RS256 COSE 公钥验签
- 新增邮箱验证码恢复登录（`/api/v1/auth/email/recovery`）：
  - 邮箱验证码一次性校验
  - 恢复成功后撤销全部旧会话并签发新 `SESSIONID`
  - 验证码错误次数限制
- 新增服务端会话体系：
  - Redis 会话存储，Cookie 只保存随机 `SESSIONID`
  - 会话列表、当前设备标识与远程下线
  - 会话空闲续期与绝对过期时间
- 新增 Redis 对象缓存，替代进程内 LRU，缓存用户、文章、评论、通知、组织等高频实体。
- 新增国际化支持（`zh_CN` / `en`），覆盖错误码、后端消息和业务枚举文案。
- 新增崩溃处理器，记录崩溃日志；补充空指针、越界和异常数据防护。
- 新增 Passkey、WebAuthn、邮箱恢复相关错误码（`2029` - `2039`）。
- 新增 `user_passkey` 表，用于保存 WebAuthn 凭据、签名计数和备份状态。

### 优化

- 登录、2FA、Passkey、GitHub OAuth、邮箱恢复统一通过 `AuthService::IssueSession` 签发会话。
- 2FA 登录临时凭证改为 Redis 一次性凭证，验证后立即失效。
- GitHub OAuth 登录接入新的服务端会话体系。
- WebSocket 握手改为复用 `SESSIONID` Cookie，不再从 URL query 读取 `uid/token/token_time`。
- 优化管理员账号创建、删除、恢复、重置密码和权限校验逻辑。
- 超级管理员初始密码不再使用默认值，改为从 `ADMIN_PASSWD` 注入。
- WebAuthn COSE 密钥构建迁移到 OpenSSL 3.0 `EVP_PKEY_fromdata` / `OSSL_PARAM` API，移除低层 EC/RSA 弃用接口。
- 优化错误码国际化、语言解析和错误消息输出。
- 优化 RSS 读取、崩溃日志目录创建等运行稳定性细节。

### 修复

- 修复崩溃日志目录不存在时无法写入日志的问题。
- 修复部分 Servlet / Manager 在空指针、空集合和越界数据下的异常风险。
- 修复运行目录未自动创建导致的日志写入等问题。
- 修复 WebSocket 鉴权与连接状态处理中的异常问题。

### 变更

- **不兼容变更**：后端不再接受旧 `S_UID` / `S_TOKEN` / `S_TOKEN_TIME` Cookie，仅接受 `SESSIONID`。
- **不兼容变更**：移除 `/api/v1/user/refresh_token` 接口；会话过期后需重新登录。
- **不兼容变更**：移除 WebSocket 旧 query token 认证和 `auth.ws_legacy_token` 配置。
- **不兼容变更**：旧设备 token 认证不再使用，设备列表与下线统一走服务端会话。
- 移除旧 `session:` Redis 会话回退和 Redis 双写逻辑。
- 移除旧 `EncryptUserId` / `DecryptUserId` 与相关 Cookie 处理代码。
- `chen-sdk` 升级至 `1.7.0-rc1`，使用新版 `EncryptorUtil::SHA256Raw` 等能力。
- `prod.env` 新增 `ADMIN_PASSWD`、`AUTH_COOKIE_SECURE`、`WEBAUTHN_RP_ID`、`WEBAUTHN_RP_NAME`、`WEBAUTHN_ORIGIN`、
  `WEBAUTHN_USER_HANDLE_SECRET` 等配置。

### 升级注意

- 已有 `prod.env` 需要补齐新增的认证和 WebAuthn 占位符，否则环境切换脚本会失败。
- 升级后旧 Cookie 用户会变为未登录，需要重新登录；旧会话不会自动迁移。
- Passkey 依赖 HTTPS 或 `localhost`，且 `webauthn.rp_id` / `webauthn.origin` 必须与实际前端地址匹配。
- 调用旧 `refresh_token` 或使用 WebSocket URL token 的客户端需要同步升级。

## v1.0.0 (2026-10-18)

首个正式版本，提供完整的博客、组织协作与研发管理能力。

### 用户与安全

- 用户注册、登录、权限与账号管理
- 多设备登录、设备列表与远程下线
- 双因素认证（2FA）与恢复码
- GitHub OAuth 登录
- 密码找回、Token 刷新与账号安全设置

### 内容与互动

- 文章创建、编辑、审核、发布与定时发布
- 分类、标签、全文搜索与文章归档
- 点赞、收藏、评论与回复
- 通知、广播与未读消息

### 组织与协作

- 组织创建、加入、成员与角色管理
- GitHub 仓库绑定与 PR 状态同步
- 组织内作业分发与成员协作

### 作业与研发

- 作业创建、分发、提交与评分
- 需求、任务、Bug 管理
- 截止时间、状态流转与研发统计

### 资源与消息

- 文件与资源上传、分片上传
- 私信会话与消息记录
- 用户反馈、帮助中心与 FAQ

### 管理与运维

- 管理后台与数据统计
- 数据备份、导出与过期数据清理
- 日志、配置、定时任务与邮件通知
