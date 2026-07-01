#!/usr/bin/env bash
# =============================================================================
#  detect_toolchain.sh -- 定位龙芯【旧世界 old-world】GCC 8.3 交叉工具链 bin 目录
#  成功: 将 bin 目录打印到 stdout 并返回 0
#  失败: 返回 1
#
#  定位优先级:
#    1) $LOONG_TOOLCHAIN_DIR              (bin 目录)
#    2) $LOONGARCH_TOOLCHAIN              (工具链根，自动补 /bin；env.sh 已导出)
#    3) $HOME/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin
#    4) PATH 中的 loongarch64-linux-gnu-gcc
#  仅当该 gcc 的主版本为 8.x（旧世界）时才接受。
# =============================================================================
set -u

_accept() {
    local d="$1"
    [ -x "$d/loongarch64-linux-gnu-gcc" ] || return 1
    local v
    v="$("$d/loongarch64-linux-gnu-gcc" -dumpversion 2>/dev/null)" || return 1
    case "$v" in
        8.*) printf '%s\n' "$d"; return 0 ;;
        *)   return 1 ;;
    esac
}

# 1) 显式 bin 目录
if [ -n "${LOONG_TOOLCHAIN_DIR:-}" ]; then
    _accept "$LOONG_TOOLCHAIN_DIR" && exit 0
fi

# 2) 工具链根
if [ -n "${LOONGARCH_TOOLCHAIN:-}" ]; then
    _accept "$LOONGARCH_TOOLCHAIN/bin" && exit 0
fi

# 3) 扫描 ~/toolchains（取版本最高的一个）
_best=""
for d in $(ls -d "$HOME"/toolchains/loongson-gnu-toolchain-8.3-*-loongarch64-linux-gnu-*/bin 2>/dev/null | sort -V); do
    _best="$d"
done
if [ -n "$_best" ]; then
    _accept "$_best" && exit 0
fi

# 4) PATH 回退
if command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
    _accept "$(dirname "$(command -v loongarch64-linux-gnu-gcc)")" && exit 0
fi

exit 1
