# 监听端口 - comment 服务（文章评论、评论点赞、评论审核等接口）
servers:
  - address: ["0.0.0.0:${COMMENT_HTTP_PORT}"]
    keepalive: ${HTTP_KEEPALIVE}
    timeout: ${HTTP_TIMEOUT}
    name: chen-http-comment
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: http
    negotiateH2: ${HTTP_NEGOTIATE_H2}
    args:
      accept_qps: ${HTTP_ACCEPT_QPS}      # 每秒最多接受的新连接数
      accept_burst: ${HTTP_ACCEPT_BURST}  # 瞬时突发上限（缺省 = accept_qps）
      max_conn: ${HTTP_MAX_CONN}          # 最大并发连接数
