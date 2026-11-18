# 监听端口 - 平台服务
#   PLATFORM_HTTP_PORT  业务 HTTP
#   PLATFORM_WS_PORT    WebSocket 网关（/ws/v1/session）
#   PLATFORM_RPC_PORT   GitHub bot 点对点 RPC（不带 relay，与 hub 无关）
servers:
  - address: ["0.0.0.0:${PLATFORM_HTTP_PORT}"]
    keepalive: ${HTTP_KEEPALIVE}
    timeout: ${HTTP_TIMEOUT}
    name: chen-http-platform
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: http
    negotiateH2: ${HTTP_NEGOTIATE_H2}
    args:
      accept_qps: ${HTTP_ACCEPT_QPS}      # 每秒最多接受的新连接数
      accept_burst: ${HTTP_ACCEPT_BURST}  # 瞬时突发上限（缺省 = accept_qps）
      max_conn: ${HTTP_MAX_CONN}          # 最大并发连接数

  - address: ["0.0.0.0:${PLATFORM_WS_PORT}"]
    keepalive: ${WS_KEEPALIVE}
    timeout: ${WS_TIMEOUT}
    name: chen-ws-platform
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: ws

  - address: ["0.0.0.0:${PLATFORM_RPC_PORT}"]
    keepalive: ${RPC_KEEPALIVE}
    timeout: ${RPC_TIMEOUT}
    name: chen-rpc-platform
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: rpc
