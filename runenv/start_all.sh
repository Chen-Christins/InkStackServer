#!/usr/bin/env bash
#
# start_all.sh — 启动 hub 与四个业务服务
#
# 用法:
#   ./runenv/start_all.sh          # 使用 runenv/.env.dev
#   ./runenv/start_all.sh prod
#
# 启动顺序: hub -> user -> article -> comment -> platform
#   hub 先起，避免其它服务首次注册失败（失败也只是打 WARN，5 秒后自动重连）
#
# 前提: 先执行过 ./runenv/generate.sh [dev|prod]，且已 make 出各服务的 .so
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RUNENV_DIR="$ROOT_DIR/runenv"

ENV_NAME="${1:-dev}"
case "$ENV_NAME" in
    dev | prod) ;;
    *)
        echo "用法: $0 [dev|prod]" >&2
        exit 1
        ;;
esac

ENV_FILE="$RUNENV_DIR/.env.$ENV_NAME"
if [ ! -f "$ENV_FILE" ]; then
    echo "错误: 找不到环境变量文件 $ENV_FILE" >&2
    exit 1
fi

set -a
# shellcheck disable=SC1090
. "$ENV_FILE"
set +a
export WORK_PATH="${WORK_PATH:-$ROOT_DIR}"

# libchen.so 所在目录：部署目录优先，其次 chen-sdk/lib
LIB_DIR=""
if [ -f "$WORK_PATH/lib/libchen.so" ]; then
    LIB_DIR="$WORK_PATH/lib"
else
    LIB_DIR="$(ls -d "$ROOT_DIR"/chen-sdk-*/lib 2>/dev/null | head -1 || true)"
fi
if [ -z "$LIB_DIR" ]; then
    echo "错误: 找不到 libchen.so，请先 make deps 或检查 $WORK_PATH/lib" >&2
    exit 1
fi

mkdir -p "$WORK_PATH/logs" "$WORK_PATH/data"

is_running() {
    local pid_file="$1"
    [ -f "$pid_file" ] || return 1
    local pid
    pid="$(cat "$pid_file" 2>/dev/null || true)"
    [ -n "$pid" ] || return 1
    kill -0 "$pid" 2>/dev/null
}

start_one() {
    local proc="$1"
    local dir="$RUNENV_DIR/$proc"
    local pid_file="$WORK_PATH/runenv/$proc/$proc.pid"

    if [ ! -x "$dir/server" ] || [ ! -f "$dir/conf/system.yml" ]; then
        echo "错误: runenv/$proc 未生成，请先执行 ./runenv/generate.sh $ENV_NAME" >&2
        exit 1
    fi
    if is_running "$pid_file"; then
        echo "  $proc 已在运行 (pid $(cat "$pid_file"))，跳过"
        return 0
    fi
    if [ "$proc" != "hub" ] && [ ! -f "$dir/module/$proc.so" ]; then
        echo "警告: 未找到 runenv/$proc/module/$proc.so，$proc 可能无法加载模块" >&2
    fi

    ( cd "$dir" && LD_LIBRARY_PATH="$LIB_DIR:${LD_LIBRARY_PATH:-}" ./server -d )
    echo "  已启动 $proc"
}

start_one hub
sleep 1

for proc in user article comment platform; do
    start_one "$proc"
done

echo "全部进程已启动 [$ENV_NAME]"
echo "  hub rpc 端口: ${HUB_RPC_PORT:-8092}"
echo "  日志目录: $WORK_PATH/logs"
echo "  停止: ./runenv/stop_all.sh $ENV_NAME"
