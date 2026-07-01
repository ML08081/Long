#!/bin/bash
# =============================================================================
#  start.sh -- PatrolSystem 启动包装脚本（由 patrol.service 调用）
#  等待摄像头就绪后启动主程序，并指定日志目录（自动按版本+时间命名）。
# =============================================================================
set -e
BIN="${PATROL_BIN:-/usr/bin/patrol_system}"
CONFIG="${PATROL_CONFIG:-/etc/patrol/config.json}"
CAM_DEV="${PATROL_CAM:-/dev/video0}"
LOG_DIR="${PATROL_LOG_DIR:-/var/log/patrol}"

mkdir -p "$LOG_DIR"

# 等待摄像头设备出现（最多 30 秒），USB 枚举可能慢于服务启动
for i in $(seq 1 30); do
    [ -e "$CAM_DEV" ] && { echo "摄像头 $CAM_DEV 已就绪"; break; }
    echo "等待摄像头 $CAM_DEV ... ($i/30)"
    sleep 1
done

echo "启动: $BIN -c $CONFIG --log-dir $LOG_DIR"
exec "$BIN" -c "$CONFIG" --log-dir "$LOG_DIR"
