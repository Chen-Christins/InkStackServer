# 协程栈与工作线程 - comment 进程
# 线程数在 runenv/.env.<env> 里的 COMMENT_IO_THREADS / COMMENT_ACCEPT_THREADS 配置，
# 本文件只影响 comment 进程
fiber:
  stack_size: ${FIBER_STACK_SIZE}

workers:
  io:
    thread_num: ${IO_THREADS}
  accept:
    thread_num: ${ACCEPT_THREADS}
