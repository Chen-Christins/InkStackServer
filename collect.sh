#!/usr/bin/env bash
#
# collect.sh — 收集部署所需文件并打包
#
# 用法:
#   ./collect.sh [ENV]
#     ENV  打包用的环境（dev|prod），默认 prod
#
# 说明:
#   - 配置值（含 WORK_PATH 部署路径）全部取自 bin/conf/<ENV>.env，无需手动传参
#
# 产物:
#   dist/                              # 部署目录（与生产结构一致，可检查）
#   techhaven-server-<时间戳>.tar.gz   # 打包产物
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_NAME="${1:-prod}"
STAGE_DIR="$ROOT_DIR/dist"
PKG_NAME="$ROOT_DIR/techhaven-server-$(date +%Y%m%d%H%M%S).tar.gz"

# 查找 chen-sdk 的 libchen.so
CHEN_SDK_LIB=""
for lib in "$ROOT_DIR"/chen-sdk-*/lib/libchen.so; do
    if [ -f "$lib" ]; then
        CHEN_SDK_LIB="$lib"
        break
    fi
done

# 预检
check_file() {
    local path="$1"
    if [ ! -e "$path" ]; then
        echo "错误: 缺少文件 $path" >&2
        exit 1
    fi
}

check_file "$ROOT_DIR/bin/server"
if [ -z "$CHEN_SDK_LIB" ]; then
    echo "错误: 找不到 chen-sdk-*/lib/libchen.so" >&2
    exit 1
fi
check_file "$ROOT_DIR/errors.json"
check_file "$ROOT_DIR/locale/C/LC_MESSAGES/techhaven_en.mo"
check_file "$ROOT_DIR/locale/C/LC_MESSAGES/techhaven_zh_CN.mo"

shopt -s nullglob
module_sos=("$ROOT_DIR"/bin/module/*.so)
shopt -u nullglob
if [ ${#module_sos[@]} -eq 0 ]; then
    echo "错误: bin/module/ 下没有 .so 模块" >&2
    exit 1
fi

# 校验 env 文件（prod 需存在 prod.env）
ENV_FILE="$ROOT_DIR/bin/conf/$ENV_NAME.env"
if [ ! -f "$ENV_FILE" ]; then
    if [ "$ENV_NAME" = "prod" ]; then
        echo "错误: 缺少 $ENV_FILE" >&2
        echo "请先执行: cp bin/conf/prod.env.example bin/conf/prod.env 并填写真实值" >&2
    else
        echo "错误: 缺少 $ENV_FILE" >&2
    fi
    exit 1
fi

echo "==> 打包环境 [$ENV_NAME]"

# 清理并创建部署目录骨架
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/conf" "$STAGE_DIR/module" "$STAGE_DIR/lib"

# 1. 渲染配置到 dist/conf（WORK_PATH 等取自 <ENV>.env）
OUT_CONF_DIR="$STAGE_DIR/conf" "$ROOT_DIR/switch-env.sh" "$ENV_NAME"

# 2. 拷贝无 .tpl 的静态 yml（如 LarkBot.yml）
for f in "$ROOT_DIR"/bin/conf/*.yml; do
    [ -f "$f" ] || continue
    if [ ! -f "$f.tpl" ]; then
        cp "$f" "$STAGE_DIR/conf/"
        echo "  复制静态配置 $(basename "$f")"
    fi
done

# 3. 拷贝二进制与库
cp "$ROOT_DIR/bin/server" "$STAGE_DIR/server"
chmod +x "$STAGE_DIR/server"
for so in "${module_sos[@]}"; do
    cp "$so" "$STAGE_DIR/module/"
done
cp "$CHEN_SDK_LIB" "$STAGE_DIR/lib/"

# 4. 拷贝错误码与国际化目录
cp "$ROOT_DIR/errors.json" "$STAGE_DIR/errors.json"
mkdir -p "$STAGE_DIR/locale/C/LC_MESSAGES"
cp "$ROOT_DIR/locale/C/LC_MESSAGES/"*.mo "$STAGE_DIR/locale/C/LC_MESSAGES/"
echo "  复制国际化目录 locale/"

# 5. 打包
tar czf "$PKG_NAME" -C "$STAGE_DIR" .

echo "==> 打包完成: $PKG_NAME"
echo "==> 部署目录: $STAGE_DIR"
