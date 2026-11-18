# 日志 - comment 服务
logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/comment/root.log
  - name: system
    level: ${LOG_LEVEL_SYSTEM}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/comment/system.log
  - name: access
    level: ${LOG_LEVEL_ACCESS}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/comment/access.log
