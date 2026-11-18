# 跨服务 RPC - comment 服务连 hub，本身不监听 rpc 端口
rpc:
  hub:
    address: ${HUB_RPC_HOST}:${HUB_RPC_PORT}
    call_timeout_ms: ${RPC_CALL_TIMEOUT_MS}
    reconnect_interval_ms: ${RPC_RECONNECT_INTERVAL_MS}
