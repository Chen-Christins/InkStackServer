#!/usr/bin/env bash
#
# generate.sh — 渲染 runenv/template 下的模板到各进程的运行目录
#
# 用法:
#   ./runenv/generate.sh          # 使用 runenv/.env.dev
#   ./runenv/generate.sh dev
#   ./runenv/generate.sh prod
#
# 产物（按配置域拆分，Config::LoadFromConfDir 会合并同一目录下所有 .yml）:
#   runenv/<进程>/conf/<域>.yml         <- template/<进程>/<域>.yml.tpl
#                                          servers / logs / workers / process / rpc
#                                          mysql / redis（hub 没有 rpc、mysql、redis）
#   runenv/<进程>/conf/TechHaven.yml    <- template/common/TechHaven.yml.tpl（hub 没有）
#   runenv/<进程>/server                <- $SERVER_BIN 的副本
#
# 每个进程的模板都在自己的目录里，改某个进程就改它自己的文件，互不影响。
#
# 覆盖规则:
#   模板里的每个变量都可以用 <进程>_<变量> 单独覆盖，例如:
#     USER_LOG_LEVEL=debug  ARTICLE_MYSQL_DBNAME=article_db  PLATFORM_IO_THREADS=2
#
# 说明:
#   - 不会删除 runenv/<进程>/module 里的 .so（那是 CMake 的产物）
#   - 渲染前扫描模板里所有 ${VAR}，缺任何一个直接失败
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RUNENV_DIR="$ROOT_DIR/runenv"
TEMPLATE_DIR="$RUNENV_DIR/template"

SERVICES=(user article comment platform)
ALL_PROCS=(hub "${SERVICES[@]}")

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

# 路径类变量允许留空，按仓库根推导
export WORK_PATH="${WORK_PATH:-$ROOT_DIR}"
export DATA_PATH="${DATA_PATH:-$WORK_PATH/data}"
export SEARCH_DICT_PATH="${SEARCH_DICT_PATH:-$WORK_PATH/dict}"
export SEARCH_INDEX_PATH="${SEARCH_INDEX_PATH:-$WORK_PATH/search_index.dat}"
export HUB_RPC_HOST="${HUB_RPC_HOST:-127.0.0.1}"
export SERVER_BIN="${SERVER_BIN:-$ROOT_DIR/bin/server}"

if [ ! -f "$SERVER_BIN" ]; then
    echo "错误: 找不到 server 可执行文件 $SERVER_BIN" >&2
    echo "      请先 make deps && make apply，或设置 SERVER_BIN=<路径>" >&2
    exit 1
fi

# 模板里出现的变量必须在环境里存在（envsubst 会把未定义变量静默替换为空串）
missing=0
while IFS= read -r var; do
    if ! printenv "$var" >/dev/null 2>&1; then
        echo "错误: $ENV_FILE 缺少变量 $var（模板中有引用）" >&2
        missing=1
    fi
done < <(grep -rhoE '\$\{[A-Za-z_][A-Za-z0-9_]*\}' "$TEMPLATE_DIR" \
    | sed -E 's/^\$\{//; s/\}$//' | sort -u)
if [ "$missing" -ne 0 ]; then
    exit 1
fi

# 允许按进程覆盖的变量
OVERRIDABLE=(
    LOG_LEVEL LOG_LEVEL_SYSTEM LOG_LEVEL_ACCESS
    FIBER_STACK_SIZE IO_THREADS ACCEPT_THREADS
    HTTP_KEEPALIVE HTTP_TIMEOUT HTTP_NEGOTIATE_H2
    HTTP_ACCEPT_QPS HTTP_ACCEPT_BURST HTTP_MAX_CONN
    WS_KEEPALIVE WS_TIMEOUT
    RPC_KEEPALIVE RPC_TIMEOUT
    DRAIN_TIMEOUT_MS DATA_PATH
    MYSQL_HOST MYSQL_PORT MYSQL_USER MYSQL_PASSWD MYSQL_DBNAME
    REDIS_HOST REDIS_DBNAME REDIS_TYPE REDIS_POOL REDIS_TIMEOUT_MS
)
# 记录基础值（bash 3.2 也能跑的写法，不用关联数组）
for var in "${OVERRIDABLE[@]}"; do
    eval "BASE_${var}=\"\${${var}-}\""
done

# 应用 <进程>_<变量> 覆盖，未覆盖时回到基础值
apply_overrides() {
    local prefix="$1"
    local var val
    for var in "${OVERRIDABLE[@]}"; do
        eval "val=\${${prefix}_${var}-}"
        if [ -z "$val" ]; then
            eval "val=\${BASE_${var}-}"
        fi
        export "$var=$val"
    done
}

render() {
    local tpl="$1"
    local out="$2"

    if command -v envsubst >/dev/null 2>&1; then
        envsubst < "$tpl" > "$out"
    else
        python3 - "$tpl" "$out" <<'PY'
import os
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
with open(src, encoding="utf-8") as f:
    text = f.read()

text = re.sub(
    r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}",
    lambda m: os.environ.get(m.group(1), m.group(0)),
    text,
)

with open(dst, "w", encoding="utf-8") as f:
    f.write(text)
PY
    fi

    if grep -qE '\$\{[A-Za-z_][A-Za-z0-9_]*\}' "$out"; then
        echo "错误: $out 存在未替换的占位符，请检查 $ENV_FILE" >&2
        exit 1
    fi
}

for proc in "${ALL_PROCS[@]}"; do
    proc_upper="$(printf '%s' "$proc" | tr '[:lower:]' '[:upper:]')"
    apply_overrides "$proc_upper"

    tpl_dir="$TEMPLATE_DIR/$proc"
    out_dir="$RUNENV_DIR/$proc/conf"

    if [ ! -d "$tpl_dir" ]; then
        echo "错误: 缺少模板目录 $tpl_dir" >&2
        exit 1
    fi

    mkdir -p "$out_dir"
    # module 目录由 CMake 写入 .so，这里只保证目录存在
    mkdir -p "$RUNENV_DIR/$proc/module"

    # 1) 进程自身：servers / logs / workers / process / rpc / mysql / redis
    shopt -s nullglob
    tpls=("$tpl_dir"/*.tpl)
    shopt -u nullglob
    if [ ${#tpls[@]} -eq 0 ]; then
        echo "错误: $tpl_dir 下没有 *.tpl" >&2
        exit 1
    fi
    for tpl in "${tpls[@]}"; do
        name="$(basename "$tpl")"
        name="${name%.tpl}"
        case "$name" in
            *.yml | *.yaml) ;;
            *) name="$name.yml" ;;
        esac
        render "$tpl" "$out_dir/$name"
    done

    # 2) 业务服务额外加载共用的业务配置（hub 不加载）
    if [ "$proc" != "hub" ]; then
        render "$TEMPLATE_DIR/common/TechHaven.yml.tpl" "$out_dir/TechHaven.yml"
    fi

    # 3) 每个进程一份 server 副本：保证 conf/ 的解析基准（cwd 或可执行文件目录）都指向本目录
    cp -f "$SERVER_BIN" "$RUNENV_DIR/$proc/server"
    chmod +x "$RUNENV_DIR/$proc/server"

    echo "  已生成 runenv/$proc/conf 与 runenv/$proc/server"
done

echo "配置已生成 [$ENV_NAME]"
echo "  work_path: $WORK_PATH"
echo "  启动: ./runenv/start_all.sh $ENV_NAME"
