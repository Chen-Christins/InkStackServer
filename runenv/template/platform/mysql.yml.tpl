# MySQL - platform 进程
# 阶段 1 与其它服务共用同一个库；拆库后直接改本文件的 dbname，
# 或用 PLATFORM_MYSQL_DBNAME 等变量在 env 里覆盖
mysql:
  dbs:
    techhaven:
      dbname: ${MYSQL_DBNAME}
      host: ${MYSQL_HOST}
      port: ${MYSQL_PORT}
      user: ${MYSQL_USER}
      passwd: ${MYSQL_PASSWD}
