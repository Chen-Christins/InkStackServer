# 日志 - article 服务
logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/article-root.log
      - type: StdoutLogAppender
  - name: system
    level: ${LOG_LEVEL_SYSTEM}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/article-system.log
      - type: StdoutLogAppender
  - name: access
    level: ${LOG_LEVEL_ACCESS}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/article-access.log
      - type: StdoutLogAppender
