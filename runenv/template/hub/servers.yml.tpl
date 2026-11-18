# hub 只监听 relay 端口，由 RpcHubRegistry 维护 peer/func/bind 索引
# 注意：hub 只允许单 worker，且不要对它发 SIGHUP 热重载（注册表在 worker 内存里）
servers:
  - address: ["0.0.0.0:${HUB_RPC_PORT}"]
    keepalive: ${RPC_KEEPALIVE}
    timeout: ${RPC_TIMEOUT}
    name: chen-rpc-hub
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: rpc
    args:
      relay: "1"
      relay_timeout: ${RELAY_TIMEOUT_MS}
