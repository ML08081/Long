# PatrolSystem 龙芯 2K0300 智能巡检系统下位机

PatrolSystem 是运行在龙芯 2K0300 板端的下位机程序，负责采集摄像头视频、监听上位机连接、转发遥测数据，并通过 UART 或 SocketCAN 与 STM32F407 控制板通信。

当前仓库已具备视频链路、网络帧协议、基础配置、下位机链路抽象、SPI 状态屏、热成像解算接口等框架能力。完整巡检业务仍需继续补充任务调度、路线执行、硬件闭环控制和实机验收数据。

## 一、当前能力边界

### 已接入能力

1. 龙芯端作为 TCP 服务端，默认监听 `0.0.0.0:8080`。
2. LongLook 上位机作为 TCP 客户端连接龙芯板。
3. V4L2 采集 USB 摄像头 MJPEG 帧，并按协议发送给上位机。
4. 周期发送传感器遥测帧，当前由 STM32 链路或占位数据填充。
5. 支持 UART 与 SocketCAN 两种下位机通信方式，运行时由 `config/config.json` 配置选择。
6. 支持 SPI ST7789 状态屏显示，默认由配置决定是否启用。
7. 支持 UDP 发现广播，便于上位机发现板端地址。
8. 支持热成像解算接口，具体数据链路仍依赖实机接线与 STM32 数据输入。

### 尚需补充能力

1. 巡检路线与任务状态机尚未形成完整闭环。
2. `config/patrol.json` 中的巡检点配置尚未作为主任务入口执行。
3. 上位机下行命令需要继续完善到真实硬件动作，包括风扇、蜂鸣器、继电器、LED、模式切换和急停。
4. 非 MJPEG 摄像头暂不做软编码。当前程序只接受 MJPEG，避免将 YUYV 等原始帧误当作 JPEG 发给上位机。
5. 需要补充实机验收记录，包括摄像头、网络、CAN/UART、状态屏、热成像和开机自启。

## 二、总体链路

```text
USB Camera(MJPEG)
      |
      v
DragonBoard 2K0300 / PatrolSystem
      |        \
      |         \ UART or SocketCAN
      |          \
      v           v
LongLook PC      STM32F407 Control Board
TCP Client       Sensors / Motion / Actuators
```

网络帧采用流式长度前缀协议：

```text
+------+-------+------------+----------------+
| 0xA5 | TYPE  | LEN(4B LE) | PAYLOAD        |
+------+-------+------------+----------------+
```

主要帧类型：

- `0x01`：传感器遥测，龙芯到上位机。
- `0x10`：视频帧，龙芯到上位机，负载必须是 JPEG 字节流。
- `0x20`：热成像帧，龙芯到上位机。
- `0x30`：文本或日志，龙芯到上位机。
- `0x40`：控制命令，上位机到龙芯。
- `0x41`：手动驱动命令，上位机到龙芯。
- `0x42`：视觉识别结果，上位机到龙芯。

协议常量需要与 `LongLook/protocol.h` 保持一致。修改任一端协议时，必须同步另一端并运行链路测试。

## 三、目录结构

```text
PatrolSystem/
├── CMakeLists.txt
├── toolchain.cmake
├── config/
│   ├── config.json          # 运行配置
│   └── patrol.json          # 巡检路线示例，待接入主任务逻辑
├── deploy/                  # 板端安装、systemd、网络配置
├── include/
├── src/
│   ├── app/                 # 主流程
│   ├── business/            # 下位机控制与业务封装
│   └── modules/             # 摄像头、网络、配置、串口、CAN、显示等模块
├── test/                    # 链路协议自测
├── third_party/             # MLX90640 解算依赖
└── kernel-modules/          # UVC/V4L2 内核模块补充资料与脚本
```

## 四、构建要求

### WSL 或 Linux 本地构建

```bash
sudo apt update
sudo apt install -y build-essential cmake linux-libc-dev

./scripts/build.sh
```

产物位置：

```text
build/bin/patrol_system
```

### 龙芯 2K0300 交叉编译

项目面向龙芯旧世界系统时，需要使用匹配的 LoongArch64 old-world GCC 8.3 工具链。

推荐命令：

```bash
./scripts/build.sh loong
```

或者显式指定工具链 `bin` 目录：

```bash
./scripts/build.sh loong /path/to/loongson-gnu-toolchain-8.3/bin
```

产物位置：

```text
build-loong/bin/patrol_system
```

如果误用 new-world 工具链，程序可能无法在目标板系统上运行。交叉编译完成后应使用 `file` 检查目标架构和动态解释器。

## 五、运行配置

默认配置文件：

```text
/etc/patrol/config.json
```

开发时可指定仓库内配置：

```bash
./build/bin/patrol_system -c config/config.json
```

常用参数：

```bash
./patrol_system [选项]
  -c, --config  <路径>   配置文件，默认 /etc/patrol/config.json
  -d, --device  <节点>   摄像头设备，默认 /dev/video0
  -W, --width   <像素>   分辨率宽，范围 160 到 4096
  -H, --height  <像素>   分辨率高，范围 120 到 2160
  -f, --fps     <帧率>   期望帧率，范围 1 到 120
  -p, --port    <端口>   TCP 监听端口，范围 1 到 65535
  -b, --bind    <地址>   监听地址，默认 0.0.0.0
      --log-dir <目录>   自动生成版本化日志文件
      --log-file<路径>   指定日志文件
  -v, --verbose          调试日志
  -V, --version          显示版本号
  -h, --help             显示帮助
```

参数校验在程序启动前完成。非法端口、非法分辨率、非法帧率、空设备名等会直接返回错误，避免进入不确定运行状态。

## 六、摄像头要求

当前视频链路只支持 MJPEG 摄像头。程序会向 V4L2 请求 `V4L2_PIX_FMT_MJPEG`，如果驱动返回 YUYV、NV12 或其他非 MJPEG 格式，程序会拒绝打开该摄像头。

检查摄像头能力：

```bash
v4l2-ctl --list-formats-ext -d /dev/video0
```

如果摄像头不支持 MJPEG，可采取以下处理方式：

1. 更换支持 MJPEG 的 UVC 摄像头。
2. 降低分辨率或帧率后重试。
3. 后续引入 JPEG 软编码模块，但这会增加 CPU 占用和交叉编译依赖。

## 七、部署说明

生成部署包：

```bash
./scripts/make_package.sh
```

上传到龙芯板：

```bash
scp patrol_deploy_v<版本>.tar.gz root@<龙芯IP>:/tmp/
ssh root@<龙芯IP>
cd /tmp
tar xzf patrol_deploy_v<版本>.tar.gz
cd patrol_deploy
sudo ./install.sh
```

部署脚本会执行以下高影响操作：

1. 安装 `/usr/bin/patrol_system` 及相关启动脚本。
2. 安装 systemd 服务 `patrol.service` 和 `patrol-network.service`。
3. 启用开机自启。
4. 停用并 mask 系统自带 `wifi-autoconnect.service`，避免它占用 `wlan0` 进入 AP 模式。

如果需要恢复系统自带 WiFi 服务：

```bash
sudo systemctl unmask wifi-autoconnect.service
sudo systemctl enable wifi-autoconnect.service
sudo systemctl start wifi-autoconnect.service
```

## 八、链路验证

### 不接摄像头的协议自测

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)

./build/bin/link_test 18080 5 &
python3 test/frame_parser_check.py 127.0.0.1 18080 3
```

期望结果：

```text
text/sensor/video 计数正常
bad=0
结果为通过
```

### 实机视频链路检查

1. 龙芯板上确认摄像头存在：`ls /dev/video*`。
2. 龙芯板上确认摄像头支持 MJPEG：`v4l2-ctl --list-formats-ext -d /dev/video0`。
3. 启动下位机：`./patrol_system -c config/config.json -v`。
4. PC 上确认能 ping 通龙芯板 IP。
5. LongLook 连接 `<龙芯IP>:8080`。
6. 龙芯日志应出现上位机连接、推流帧率和链路状态。
7. LongLook 视频区应显示画面和帧率。

## 九、故障排查

### LongLook 连接不上

1. 确认龙芯日志中存在 `TCP 服务端已监听`。
2. 确认端口为 `8080` 或与 LongLook 配置一致。
3. 确认 PC 和龙芯处于同一网段。
4. 检查防火墙是否拦截 TCP 端口。
5. 检查部署脚本是否修改了板端 WiFi 服务。

### 摄像头打不开

1. 确认 `/dev/video0` 存在。
2. 确认 UVC/V4L2 内核模块已加载。
3. 使用 `v4l2-ctl` 确认支持 MJPEG。
4. 降低到 `640x480@30` 后重试。

### 有连接但无视频

1. 检查日志中是否出现非 MJPEG 拒绝信息。
2. 检查 LongLook 与下位机协议常量是否一致。
3. 使用 `test/frame_parser_check.py` 排除协议解析问题。
4. 检查网络是否丢包严重或延迟过高。

### CAN 或 UART 不工作

1. 检查 `config/config.json` 中 `link.type` 是 `can` 还是 `uart`。
2. CAN 模式下确认接口已 up，例如 `ip link show can1`。
3. UART 模式下确认设备节点和波特率。
4. 检查 STM32 固件协议版本是否与龙芯端一致。

## 十、后续补充计划

优先级建议如下：

1. 完成急停、模式切换和手动控制命令的硬件闭环。
2. 将 `config/patrol.json` 接入任务状态机，实现路线巡检。
3. 建立传感器遥测字段与 STM32 固件协议的版本化文档。
4. 增加实机验收记录，包括网络、视频、CAN/UART、状态屏、热成像和开机自启。
5. 为协议和配置解析增加持续集成检查。
6. 评估是否需要支持非 MJPEG 摄像头的软件 JPEG 编码。
