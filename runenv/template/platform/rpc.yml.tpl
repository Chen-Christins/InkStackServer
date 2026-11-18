# 跨服务 RPC - 平台服务连 hub（2000~2005 的提供方）
# 注意：PLATFORM_RPC_PORT 上的 RpcServer 只服务 GitHub bot，不走 hub
rpc:
  hub:
    address: ${HUB_RPC_HOST}:${HUB_RPC_PORT}
    call_timeout_ms: ${RPC_CALL_TIMEOUT_MS}
    reconnect_interval_ms: ${RPC_RECONNECT_INTERVAL_MS}
