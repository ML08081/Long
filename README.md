# PatrolSystem — 龙芯 2K0300 智能巡检系统（下位机）

龙芯 2K0300 端 C++ 程序。**当前阶段只实现「摄像头 → TCP → 上位机 LongLook」的视频链路测试**，
用于打通并验证下位机与上位机之间的网络视频链路；其余业务模块（传感器、串口、运动控制等）暂为占位，后续补充。

```
┌──────────────────────────┐    WiFi / TCP     ┌──────────────────────────────┐
│ 龙芯 2K0300  (本程序)     │  ───────────────▶ │  LongLook 监控前端 (PC)       │
│ TCP 服务端 :8080          │   0x10 视频帧      │  TCP 客户端                   │
│ V4L2 采集 USB 摄像头(MJPEG)│   0x30 文本/0x01   │  默认连 192.168.1.100:8080    │
└──────────────────────────┘ ◀───────────────  └──────────────────────────────┘
                                0x40 下行命令
```

- **角色**：龙芯端是 **TCP 服务端**，上位机 LongLook 是 **TCP 客户端**（与 `LongLook/protocol.h` 约定一致）。
- **视频来源**：V4L2 直接采集 USB 摄像头的 **MJPEG** 帧，每帧本身就是一张 JPEG，**零编码**直接发送 → 不依赖 OpenCV/libjpeg，交叉编译到 LoongArch 最省事。
- **依赖**：仅 Linux/POSIX + V4L2（内核头）+ pthread，无第三方库。

---

## 一、目录结构

```
PatrolSystem/
├── CMakeLists.txt              # 顶层构建
├── toolchain.cmake            # 龙芯 LoongArch64 交叉编译工具链
├── scripts/build.sh           # 一键构建脚本
├── config/config.json         # 运行参数参考（当前以命令行参数为准）
├── include/
│   ├── app/Application.h
│   └── modules/
│       ├── camera/CameraManager.h     # V4L2 采集
│       ├── network/FrameProtocol.h    # 与 LongLook 对接的帧协议 v2
│       ├── network/TcpServer.h        # TCP 服务端
│       └── logger/Logger.h
└── src/
    ├── main.cpp                       # 入口：参数解析 + 信号处理
    ├── app/Application.cpp            # 主流程：采集→推流
    └── modules/{camera,network,logger}/*.cpp
```

> 其余 `include/business`、`include/core`、`src/business`、`src/core` 等为后续功能占位，当前不参与编译
> （见 `CMakeLists.txt` 的 `PATROL_SOURCES` 列表）。

---

## 二、网络帧协议 (v2)

与上位机 `LongLook/protocol.h` 完全一致。流式，长度前缀，无 CRC（TCP 已保证可靠）：

```
┌──────┬──────┬────────────┬────────────────┐
│ 0xA5 │ TYPE │ LEN(4B LE) │ PAYLOAD(LEN B) │
└──────┴──────┴────────────┴────────────────┘
```

| TYPE | 方向 | 含义 | 本程序 |
|------|------|------|--------|
| `0x01` | 龙芯→前端 | 传感器遥测(40B 小端) | 每秒发一帧占位数据 |
| `0x10` | 龙芯→前端 | 视频帧 (JPEG) | **主要内容**，按摄像头帧率推送 |
| `0x30` | 龙芯→前端 | 文本/日志 (UTF-8) | 连接成功时发一条就绪提示 |
| `0x40` | 前端→龙芯 | 控制命令 `[cmdId][value]` | 仅记录日志（功能留空） |

---

## 三、在 WSL 下构建

### 1. 安装工具链

```bash
sudo apt update
sudo apt install -y build-essential cmake
# 交叉编译龙芯需要 LoongArch64 工具链（按你的实际工具链安装）：
# sudo apt install -y gcc-loongarch64-linux-gnu g++-loongarch64-linux-gnu
```

> Linux 的 V4L2 头文件（`linux/videodev2.h`）随内核头文件提供，Ubuntu/Debian 一般已自带；
> 若缺失：`sudo apt install linux-libc-dev`。

### 2. 本地编译（x86_64，用于联调逻辑）

```bash
./scripts/build.sh
# 产物: build/bin/patrol_system
```

### 3. 交叉编译到龙芯 2K0300 (LoongArch64)

```bash
./scripts/build.sh loong
# 或指定工具链前缀：
./scripts/build.sh loong loongarch64-linux-gnu-
# 产物: build-loong/bin/patrol_system
```

> 若你的板子用的是旧 MIPS 工具链，把前缀换成 `mips64el-linux-gnuabi64-` 即可，
> 或编辑 `toolchain.cmake`。

手动等价命令：

```bash
mkdir -p build-loong && cd build-loong
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake ..
make -j$(nproc)
```

---

## 四、运行

```bash
./patrol_system [选项]
  -d, --device <节点>   摄像头设备 (默认 /dev/video0)
  -W, --width  <像素>   分辨率宽 (默认 1280)
  -H, --height <像素>   分辨率高 (默认 720)
  -f, --fps    <帧率>   期望帧率 (默认 30)
  -p, --port   <端口>   TCP 监听端口 (默认 8080)
  -b, --bind   <地址>   监听地址 (默认 0.0.0.0)
  -v, --verbose         调试日志
```

在龙芯板子上：

```bash
# 1) 确认摄像头设备
ls /dev/video*
v4l2-ctl --list-formats-ext -d /dev/video0   # 可选：确认支持 MJPEG

# 2) 配置网卡 IP（让 LongLook 默认的 192.168.1.100 能连上，或在 LongLook 中填板子实际 IP）
sudo ip addr add 192.168.1.100/24 dev eth0    # 按实际网卡名

# 3) 启动
./patrol_system -d /dev/video0 -W 1280 -H 720 -f 30 -p 8080
```

在 PC 上打开 `LongLook.exe`，填入龙芯 IP（默认 `192.168.1.100`）和端口 `8080`，点「连接」，
中间视频区即可看到画面。也可命令行自动连：`LongLook.exe 192.168.1.100:8080`。

---

## 五、链路验证清单（排查链路问题时按此走）

1. **网络通不通**：PC 上 `ping 192.168.1.100`，龙芯上 `ping <PC_IP>`。
2. **端口监听**：龙芯启动后日志应打印 `TCP 服务端已监听 0.0.0.0:8080`。
3. **防火墙**：确认 PC/龙芯防火墙未拦 8080。
4. **连接建立**：LongLook 点连接后，龙芯日志出现 `上位机已连接: <PC_IP>:xxxx`，LongLook 日志出现就绪文本。
5. **视频流**：龙芯日志每秒打印 `视频推流中: N fps`；LongLook 视频区出现画面、右上角叠加分辨率/FPS。
6. **若画面花屏/无法解码**：多半是摄像头未输出 MJPEG（龙芯日志会告警「非MJPEG」）。换支持 MJPEG 的摄像头，或降低分辨率重试。

---

## 六、不接摄像头也能自测链路（推荐先跑一遍）

`test/` 下有一套链路集成测试：用真实的帧协议代码发帧，再用复刻上位机解析逻辑的
Python 脚本收帧校验，验证“龙芯端发出的字节”与 LongLook 解析器字节级兼容。

```bash
# 1) 开启测试目标编译
cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build -j$(nproc)

# 2) 启动测试服务端（发 文本+传感器+5 个伪视频帧）
./build/bin/link_test 18080 5 &

# 3) Python 收端校验
python3 test/frame_parser_check.py 127.0.0.1 18080 3
# 期望输出：text/sensor/video 计数正常，bad=0，结果「通过 ✅」
```

> 也可以让真正的 `LongLook.exe 127.0.0.1:18080` 连接 `link_test` 直接肉眼看链路是否打通。

---

## 七、关于在 WSL 里直接测试摄像头

WSL2 默认不直通 USB 摄像头。两种办法：

- **推荐**：把交叉编译/本地编译产物拷到真实龙芯板子上跑，用板子自带 USB 摄像头测试。
- 若一定要在 WSL 测：用 [usbipd-win](https://github.com/dorssel/usbipd-win) 把 USB 摄像头 attach 进 WSL，
  并确保 WSL 内核带 UVC 驱动后，`/dev/video0` 才会出现。

---

## 八、后续扩展方向（当前留空）

- 传感器/串口：对接 STM32，填充真实 `0x01` 遥测数据。
- 下行命令 `0x40`：在 `Application::serveClient` 中实现风扇/蜂鸣器/继电器/LED/模式/急停的真实控制。
- 热成像 `0x20`：对接红外测温模块。
- 视频：接入识别（YOLO）叠加框，或非 MJPEG 摄像头的 JPEG 软编码。
- 配置：接入 `config/config.json`（`ConfigManager`），支持文件配置覆盖命令行默认值。
