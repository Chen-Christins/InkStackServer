# 统一 WebSocket 会话（`/ws/v1/session`）

## 目标

把通知、在线人数、私信和系统消息收敛到 **每设备一条长连接**：

```
浏览器 ── SESSIONID Cookie ──▶ /ws/v1/session
                                   │
                                   ├── system       系统帧 / 错误 / 连接生命周期
                                   ├── notification 通知与广播
                                   ├── presence     在线人数
                                   └── chat         私信收发与已读回执
```

旧端点 `/ws/v1/notification`、`/ws/v1/presence`、`/ws/v1/messages` 已下线，不再注册。

## 握手与设备标识

- 握手复用 `SESSIONID` Cookie，不接受 query 中的 `uid` / `token`。
- 会话校验走 `AuthService::ResolveWs`，过期、被撤销或用户被禁用时握手失败。
- 设备标识为 `device_id = SHA256(SESSIONID)`，与 `user/device/list` 返回的 `device_id` 一致。
- **一个设备只保留一条连接**：同一 `device_id` 再次连接时，新连接顶掉旧连接。
- 同一用户可以有多个设备同时在线；在线人数按 `uid` 去重。

普通用户也可以连接网关接收通知和在线人数，但不能发送私信帧；私信帧会返回 `ACCESS_DENIED`。

## 帧格式

所有下行帧都在顶层带 `channel` 字段，客户端可以按 `channel` 分发：

```json
{
  "channel": "notification",
  "type": "comment",
  "title": "收到新评论",
  "content": "..."
}
```

### 上行帧

| `type` | 字段                                   | 说明                                             |
|--------|----------------------------------------|--------------------------------------------------|
| `ping` | -                                      | 心跳，服务端回复 `pong`                          |
| `send` | `conversation_id`、`text`、`client_id` | 发送私信；`client_id` 由客户端生成，用于匹配 ACK |
| `read` | `conversation_id`                      | 标记会话已读，并向对端推送已读回执               |

`send` / `read` 仅对非普通用户开放，且会再次校验会话参与者身份。

### 下行帧

#### system

| `type`              | 说明                                                    |
|---------------------|---------------------------------------------------------|
| `connected`         | 握手成功，附带 `uid`、`device_id`、`online_users`       |
| `pong`              | 心跳回复                                                |
| `error`             | 通用错误，附带 `errno`、`errstr`                        |
| `replaced`          | 当前设备被新连接顶掉，`reason` 为 `connected_elsewhere` |
| `session_expired`   | 会话过期或被删除                                        |
| `session_refreshed` | 同一会话重新签发，旧连接失效                            |
| `session_revoked`   | 会话被撤销（登出、改密、封禁、删除等）                  |
| `device_kicked`     | 当前设备被远程下线                                      |

```json
{
  "channel": "system",
  "type": "connected",
  "uid": 1001,
  "device_id": "<sha256>",
  "online_users": 12
}
{
  "channel": "system",
  "type": "replaced",
  "reason": "connected_elsewhere"
}
{
  "channel": "system",
  "type": "error",
  "errno": 1002,
  "errstr": "Invalid frame"
}
```

#### notification

通知推送沿用原通知 JSON 字段，仅额外增加 `channel`：

```json
{
  "channel": "notification",
  "id": 9001,
  "type": "comment",
  "title": "收到新评论",
  "content": "...",
  "article_id": 42,
  "comment_id": 100,
  "is_read": false,
  "create_time": 1762800000
}
```

`NotificationManager::sendToUser` 推给用户所有设备；`broadcast` 推给所有在线设备。

#### presence

用户从离线变为在线（首个设备接入）或从在线变为离线（最后一个设备断开）时广播：

```json
{
  "channel": "presence",
  "type": "online_count",
  "count": 12
}
```

`count` 按 `uid` 去重，不按设备数统计。指标 `blog_online_users` 使用该值，`blog_ws_online_devices` 统计连接数。

#### chat

发送成功后， **只给发起设备**回 ACK：

```json
{
  "channel": "chat",
  "type": "message_ack",
  "client_id": "tab-1-1762800000123",
  "conversation_id": 7,
  "message": {
    "...": "..."
  },
  "recipient_online": true
}
```

接收方所有设备收到：

```json
{
  "channel": "chat",
  "type": "message",
  "conversation_id": 7,
  "message": {
    "...": "..."
  }
}
```

已读回执推给对端所有设备：

```json
{
  "channel": "chat",
  "type": "read",
  "conversation_id": 7,
  "unread": 0
}
```

私信帧失败时返回：

```json
{
  "channel": "chat",
  "type": "send_error",
  "client_id": "tab-1-1762800000123",
  "errno": 12003,
  "errstr": "Message too long"
}
```

`client_id` 只在客户端提供时回传。HTTP 私信发送接口（`conversation/message`）仍可用，接收方同样会收到 `channel:"chat"` 的
`message` 帧。

## 投递语义

| 事件              | 投递范围                        |
|-------------------|---------------------------------|
| 通知 / 广播       | 目标用户所有设备 / 全部在线设备 |
| 私信发送 ACK      | 仅发起设备                      |
| 私信消息          | 接收方所有设备                  |
| 已读回执          | 对端所有设备                    |
| 在线人数          | 全部在线设备                    |
| 会话失效 / 踢设备 | 对应设备                        |

- 同一连接上的发送通过连接级互斥锁串行化，避免多线程写坏 WebSocket 帧。
- `recipient_online` 表示接收方是否有任意设备在线，不保证对方当前角色仍可聊天。
- 服务端先尝试发送关闭原因帧，再关闭连接；客户端应以 `system` 帧为准处理登出和顶号。

## 连接生命周期

| 场景                | 行为                                                   |
|---------------------|--------------------------------------------------------|
| 同一设备新连接      | 旧连接收到 `replaced` 后关闭                           |
| 会话过期 / 失效     | `AuthService::Resolve` 关闭对应设备连接                |
| 重新签发会话        | 旧 `SESSIONID` 对应连接收到 `session_refreshed` 后关闭 |
| 登出 / 撤销单个会话 | 对应连接收到 `session_revoked` 后关闭                  |
| 撤销全部会话        | 该用户所有连接收到 `session_revoked` 后关闭            |
| 远程下线设备        | 对应连接收到 `device_kicked` 后关闭                    |
| 模块停用 / 热重载   | 关闭全部连接                                           |

## 客户端接入示例

```js
const ws = new WebSocket(`wss://example.com/ws/v1/session`);

ws.onmessage = (event) => {
    const frame = JSON.parse(event.data);
    switch (frame.channel) {
        case "system":
            if (frame.type === "replaced") {
                // 当前标签页已被同设备的新连接顶掉，停止重连
            }
            break;
        case "notification":
            // 刷新通知列表 / 未读数
            break;
        case "presence":
            // frame.count
            break;
        case "chat":
            // message / message_ack / read / send_error
            break;
    }
};
```

心跳由客户端定时发送 `{"type":"ping"}`，服务端回复 `{"channel":"system","type":"pong"}`。
