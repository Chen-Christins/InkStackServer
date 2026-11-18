# 进程与模块 - article 服务
module:
  path: ${WORK_PATH}/runenv/article/module

server:
  work_path: ${WORK_PATH}
  data_path: ${DATA_PATH}
  pid_file: ${WORK_PATH}/runenv/article/article.pid
  process_name: InkStack-article
  # 热重载/关机时留给在途请求的 drain 时间
  drain_timeout_ms: ${DRAIN_TIMEOUT_MS}
