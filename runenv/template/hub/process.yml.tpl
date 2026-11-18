# 进程 - hub 不加载任何业务模块
module:
  path: ${WORK_PATH}/runenv/hub/module

server:
  work_path: ${WORK_PATH}
  pid_file: ${WORK_PATH}/runenv/hub/hub.pid
  process_name: InkStack-hub
  drain_timeout_ms: ${DRAIN_TIMEOUT_MS}
