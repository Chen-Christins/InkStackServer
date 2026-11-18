# 进程与模块 - comment 服务
module:
  path: ${WORK_PATH}/runenv/comment/module

server:
  work_path: ${WORK_PATH}
  data_path: ${DATA_PATH}
  pid_file: ${WORK_PATH}/runenv/comment/comment.pid
  process_name: InkStack-comment
  # 热重载/关机时留给在途请求的 drain 时间
  drain_timeout_ms: ${DRAIN_TIMEOUT_MS}
