# 日志 - hub（无业务 access 日志）
logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/hub/root.log
  - name: system
    level: ${LOG_LEVEL_SYSTEM}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/hub/system.log
