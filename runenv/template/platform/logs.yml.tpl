# 日志 - 平台服务
logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/platform/root.log
  - name: system
    level: ${LOG_LEVEL_SYSTEM}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/platform/system.log
  - name: access
    level: ${LOG_LEVEL_ACCESS}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/platform/access.log
