#!/usr/bin/env bash
#
# stop_all.sh — 停止四个业务服务与 hub
#
# 用法:
#   ./runenv/stop_all.sh           # 使用 runenv/.env.dev
#   ./runenv/stop_all.sh prod
#
# 停止顺序: platform -> comment -> article -> user -> hub
#   业务服务先停，最后停 hub，避免退出过程中还在发起跨服务调用。
#   先发 SIGTERM 走框架的优雅退出，15 秒未退出再 SIGKILL。
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

stop_one() {
    local proc="$1"
    local pid_file="$WORK_PATH/runenv/$proc/$proc.pid"

    if [ ! -f "$pid_file" ]; then
        echo "  $proc 未运行（无 $pid_file）"
        return 0
    fi

    local pid
    pid="$(cat "$pid_file" 2>/dev/null || true)"
    if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
        echo "  $proc 未运行（pid 文件过期）"
        rm -f "$pid_file"
        return 0
    fi

    kill -TERM "$pid" 2>/dev/null || true
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$waited" -ge 15 ]; then
            echo "  $proc 未在 15 秒内退出，强制结束"
            kill -KILL "$pid" 2>/dev/null || true
            break
        fi
        sleep 1
        waited=$((waited + 1))
    done

    rm -f "$pid_file"
    echo "  已停止 $proc (pid $pid)"
}

for proc in platform comment article user hub; do
    stop_one "$proc"
done

echo "全部进程已停止 [$ENV_NAME]"
