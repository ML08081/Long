#!/bin/bash
# =============================================================================
#  health_check.sh -- 启动健康检测
#  由 patrol.service 的 ExecStartPre 在主程序启动前调用；检查关键前置条件并写日志。
#  非致命：始终 exit 0，不阻塞主服务（主程序自身具备重试与自愈能力）。
# =============================================================================
LOG_DIR="${1:-/var/log/patrol}"
mkdir -p "$LOG_DIR"
HLOG="$LOG_DIR/health.log"
CAM="${PATROL_CAM:-/dev/video0}"
PING_HOST="${PATROL_PING:-baidu.com}"
CONFIG="${PATROL_CONFIG:-/etc/patrol/config.json}"

ts(){ date "+%Y-%m-%d %H:%M:%S"; }
rec(){ echo "[$(ts)] [health] $*" | tee -a "$HLOG"; }

rec "==================== 启动健康检测 ===================="

# 程序版本
if command -v patrol_system >/dev/null 2>&1; then
    rec "程序版本: $(patrol_system --version 2>/dev/null)"
fi

# 摄像头
if [ -e "$CAM" ]; then rec "[OK]   摄像头设备 $CAM 存在"
else rec "[WARN] 摄像头 $CAM 不存在（主程序将自动重试）"; fi

# 配置文件
if [ -f "$CONFIG" ]; then rec "[OK]   配置文件 $CONFIG 存在"
else rec "[WARN] 配置文件 $CONFIG 缺失（将使用内置默认值）"; fi

# 本机 IP
IP=$(ip -4 addr show 2>/dev/null | awk '/inet .* scope global/{print $2; exit}')
if [ -n "$IP" ]; then rec "[OK]   本机 IP: $IP  （LongLook 连接此地址）"
else rec "[WARN] 未获取到 IP，请检查 WiFi 连接"; fi

# 外网连通
if ping -c 1 -W 2 "$PING_HOST" >/dev/null 2>&1; then rec "[OK]   外网连通 ($PING_HOST)"
else rec "[WARN] 外网 $PING_HOST 不通（局域网内仍可能可用）"; fi

# 日志分区剩余
DISK=$(df -h "$LOG_DIR" 2>/dev/null | awk 'NR==2{print $4}')
rec "[INFO] 日志分区剩余: ${DISK:-未知}"

# 日志目录文件数（便于按版本整理时了解规模）
CNT=$(ls -1 "$LOG_DIR"/patrol-v*.log 2>/dev/null | wc -l)
rec "[INFO] 历史日志文件数: $CNT"

rec "==================== 检测结束 ===================="
exit 0
