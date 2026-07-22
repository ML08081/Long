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
WAIT_CAMERA_SEC="${PATROL_WAIT_CAMERA_SEC:-3}"

mkdir -p "$LOG_DIR"

# 等待摄像头设备短暂出现。主程序会自行降级处理无摄像头场景，避免阻塞传感器/屏幕启动。
for i in $(seq 1 "$WAIT_CAMERA_SEC"); do
    [ -e "$CAM_DEV" ] && { echo "摄像头 $CAM_DEV 已就绪"; break; }
    echo "等待摄像头 $CAM_DEV ... ($i/$WAIT_CAMERA_SEC)"
    sleep 1
done

echo "启动: $BIN -c $CONFIG --log-dir $LOG_DIR"
exec "$BIN" -c "$CONFIG" --log-dir "$LOG_DIR"
