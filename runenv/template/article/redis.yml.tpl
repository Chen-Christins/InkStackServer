# Redis - article 进程
# fox_thread 是 DB 异步线程池使用的连接；redis 是业务连接池
fox_thread:
  redis:
    name: redis
    num: ${FOX_REDIS_NUM}
    advance: ${FOX_REDIS_ADVANCE}

redis:
  name: ${REDIS_DBNAME}
  config:
    ${REDIS_DBNAME}:
      host: ${REDIS_HOST}
      type: ${REDIS_TYPE}
      pool: ${REDIS_POOL}
      timeout: ${REDIS_TIMEOUT_MS}
  desc: "type: redis,redis_cluster,fox_redis,fox_redis_cluster"
