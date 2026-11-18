# 进程与模块 - 平台服务
module:
  path: ${WORK_PATH}/runenv/platform/module

server:
  work_path: ${WORK_PATH}
  data_path: ${DATA_PATH}
  pid_file: ${WORK_PATH}/runenv/platform/platform.pid
  process_name: InkStack-platform
  # 热重载/关机时留给在途请求的 drain 时间
  drain_timeout_ms: ${DRAIN_TIMEOUT_MS}
