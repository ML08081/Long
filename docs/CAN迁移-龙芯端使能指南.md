# CAN 迁移 · 龙芯端使能指南（P0 摸底 + 起 can0 + 切换链路）

> 配套代码：`src/modules/can/CanManager.{h,cpp}`、`include/modules/can/CanProtocol.h`
> 依据文档：`方案文档/自主巡检系统实施方案_CAN与PCB.md` §3.7；`方案文档/项目进度与开发交接_2026-07-17.md`
> 本轮范围：**只做 CAN 基础层软件**（传输层 + 类 SDO 客户端 + 链路健康）。ArUco/路线表 Mission 下一轮。
> 硬件路线：**原生 CAN2**（PIN7=GPIO72=CAN2_TX / PIN9=GPIO73=CAN2_RX）；起不来的兜底见第 4 节 Plan B。

---

## 0. 当前软件状态（本轮已落地）

- `CanManager`：SocketCAN 收发，接口对齐 `SerialManager`（上层 `RobotController` 只换持有对象）。
  - 收 F4 上行 6 个 COB-ID：`0x181/0x281/0x381/0x481/0x581/0x701`。
  - 发命令 `0x201`（兼心跳）、`0x301`（巡检/避障/PID测试）。
  - 类 SDO 客户端：`sdoWrite`/`sdoRead`（expedited）、`sdoWriteDomain`（分段，路线表用）。
  - **收 CAN 错误帧**（`CAN_RAW_ERR_FILTER`）→ `Health{errFrames,busOff,tec,rec}`，`statusLine` 打印。
- 一键回退：`config.json` 的 `link.type = "uart" | "can"`。**当前默认 `uart`**，功能与 v1.16 完全一致。
- ⚠️ 停用了龙芯侧 `computeAvoid`（实施方案 R10）：运动决策权统一归 F4 本地快环，龙芯只发"基础前进意图 + 直行"，转向/避障由 F4。

> **本轮不改现网行为**：默认 uart，部署上去和以前一样跑。CAN 要等下面 can0 起来、把 `link.type` 改成 `can` 才启用。

---

## 0.5 ★ 引脚接线（规格书丝印 + 真机实测确定，2026-07-20）

**权威来源 = `2K0300_99PAI_V1_0主板规格.pdf` Page6 表1-3「IO 插针接口定义」**（2×15，2.54mm）：

| 奇 | 信号 | 偶 | 信号 |
|---|---|---|---|
| 1 | GND | 2 | P3V3 |
| 3 | PWM2 | 4 | UART0_TXD |
| 5 | PWM3 | 6 | UART0_RXD |
| **7** | **CAN2_TX**(GPIO72) | 8 | UART1_TXD |
| **9** | **CAN2_RX**(GPIO73) | 10 | UART1_RXD |
| **11** | **CAN3_TX**(GPIO74) | 12 | UART2_TXD |
| **13** | **CAN3_RX**(GPIO75) | 14 | UART2_RXD |
| 15 | I2C1_SCL | 16 | I2C0_SCL |
| 17 | I2C1_SDA | 18 | I2C0_SDA |
| 19 | GND | 20 | GND |
| 21 | SPI2_CLK | 22 | SPI1_CLK |
| 23 | SPI2_CSn | 24 | SPI1_CSn |
| 25 | SPI2_MISO | 26 | SPI1_MISO |
| 27 | SPI2_MOSI | 28 | SPI1_MOSI |
| 29 | GND | 30 | P5V |

> 丝印直接标 **CAN2_TX/RX**(PIN7/9) 与 **CAN3_TX/RX**(PIN11/13)，接线照丝印名即可、不用数 PIN。
> 软件映射（内核 pinmux 实测）：丝印 **CAN2 = SocketCAN can0** = GPIO72/73；丝印 **CAN3 = can1** = GPIO74/75。两个控制器都实测 500kbps 能 up（驱动 `ctu_can_fd`）。
> 备用空闲可复用 GPIO（都在排针上、丝印可查）：**PWM2(PIN3)、PWM3(PIN5)**、SPI2_MISO(PIN25，屏只写不用)。UART1(8/10)=F4控制ttyS1、UART2(12/14)=热成像ttyS2、UART0(4/6)=串口控制台，**这三组勿占**。

**冲突现状**：ST7789 屏当前借用 **DC=CAN2_TX(PIN7) / RST=CAN2_RX(PIN9) / BL=CAN3_TX(PIN11)**——压在 CAN2 全部 + CAN3_TX 上。

### ★ 推荐方案：CAN 用 **can1(CAN3)**，屏只挪 BL 一根线（最小改动）

**实测依据**：`can1` up 时 GPIO72/73（屏 DC/RST）仍可正常翻转（互不影响，因为 can1 用 74/75）。

| 项 | 接丝印脚 | 说明 |
|---|---|---|
| CAN 收发器 D/TXD ←(MCU发) | **CAN3_TX**（PIN11 = GPIO74）| 原屏 BL 信号脚，BL 让出给 CAN |
| CAN 收发器 R/RXD →(MCU收) | **CAN3_RX**（PIN13 = GPIO75）| |
| CAN 收发器 VCC | **3.3V 共享节点**（并到屏 VCC）| ↓ 见"供电"，不占新脚 |
| CAN 收发器 GND | **GND**（PIN1/19/29 任一）| 可与屏 GND 并接 |
| CANH / CANL | → F4 侧收发器 | 双绞线，两端各一个 120Ω |
| **屏 DC** | **CAN2_TX**（PIN7 = GPIO72，不动）| |
| **屏 RST** | **CAN2_RX**（PIN9 = GPIO73，不动）| |
| **屏 SCL/SDA/CS** | **SPI2_CLK/MOSI/CSn**（PIN21/27/23，不动）| |
| **屏 BL** | **并到屏 VCC 的 3.3V 节点**（常亮）| BL 高电平点亮；`gpio_bl=-1` 跳过背光 GPIO |

**★ 供电（3.3V 只有 IO_PIN2 + ADC_PIN1 两脚且被屏占——用并联解决）**：
3.3V 是电源轨，**可一分多并联**，不必每器件独占排针脚。把 **屏 VCC + 屏 BL + CAN 收发器 VCC** 三路全并到**同一个 3.3V 脚**（如 IO 排针 PIN2），用 Y 型杜邦线/接线端子/洞洞板并接即可。板载 LDO 带这三路（屏 ~30mA + BL ~30mA + SN65HVD230 ~10mA）绰绰有余。这样只用 1 个 3.3V 脚，ADC_PIN1 的 3.3V 空出备用。GND 同理并接（PIN1/19/29 有三个）。
> 换言之：**CAN 收发器不需要"第二个空闲 3.3V 脚"，蹭屏 VCC 的 3.3V 并联即可**。屏 BL 与 VCC 短接常亮，既不占 GPIO 也不占额外 3.3V。

**config 改动**（切 CAN 时）：
```jsonc
"link":    { "type": "can", "can_if": "can1", ... },   // ★ can1 而非 can0
"display": { ..., "gpio_bl": -1, ... }                 // BL 接 3V3 常亮，驱动跳过背光脚
```

> **唯一约束**：**绝不能 `ip link set can0 up`**——can0(CAN2) 会抢回 72/73，屏 DC/RST 立刻失效。我们用 can1，天然不碰 can0。
> **`can0.service` 里的 `can0` 全部改成 `can1`** 再启用（或复制成 `can1.service`）。

### 备选方案：CAN 用 can0(CAN2)，屏迁到 PWM 脚

若坚持用 can0（CAN2_TX/RX = PIN7/9），屏 DC/RST 要让出 72/73。可迁到排针上的 **PWM2(PIN3)/PWM3(PIN5)**（规格书注明"可复用为 GPIO"，在排针上、丝印可查）：屏 DC→PWM2(PIN3)、RST→PWM3(PIN5)、BL→P3V3 常亮。需实测 PWM2/PWM3 对应 GPIO 号并 sysfs 翻转验证后填 config。比推荐方案多改两根屏线，故非首选。

> ⚠️ **UART0(PIN4/6) 不可用**：它是串口调试控制台（`console=ttyS0,115200`），占用会废掉调试串口。**UART1(PIN8/10)=F4控制、UART2(PIN12/14)=热成像**，也勿占。

---

## 1. 第一步：CAN 能力摸底（P0，动手前必做，实施方案 §3.7.1）

在龙芯板上跑（`root@192.168.3.100`）：

```bash
# 1) 内核有没有 CAN 子系统
ls /lib/modules/$(uname -r)/kernel/net/can/ 2>/dev/null
zcat /proc/config.gz 2>/dev/null | grep -iE "CONFIG_CAN(_RAW|_DEV)?="
modprobe can && modprobe can-raw && modprobe can-dev && echo "CAN 子系统 OK"

# 2) 有没有 CAN 网络设备
ip link show | grep -i can ; ls /sys/class/net/

# 3) 设备树里 CAN 节点状态（原生 CAN2 路线的关键）
find /proc/device-tree -iname "*can*" -maxdepth 4
for f in /proc/device-tree/soc/can@*/status; do echo -n "$f: "; cat "$f" 2>/dev/null | tr -d '\0'; echo; done

# 4) Plan B 可行性：USB-CAN 驱动在不在
modinfo gs_usb 2>/dev/null && echo "USB-CAN(Plan B) 可用"

# 5) 内核日志
dmesg | grep -iE "can|bxcan|dcan"
```

**判读**：
- 有 `can0` 且能 up → 直接跳第 3 节。
- 无 `can0` 但设备树有 `can@...`（`status=disabled`）→ 走第 2 节原生 CAN2 使能。
- 内核根本没 CAN 子系统 / 折腾超时 → **硬性时间盒：2 小时起不来立即转第 4 节 USB-CAN（Plan B）**，上层软件零改动（实施方案 R13）。

---

## 2. 原生 CAN2 使能（设备树 + pinmux + 屏脚让位）

1. **设备树放开 CAN2**：把 `can@...`（对应 CAN2）节点 `status="disabled"` 改 `status="okay"`，重编 dtb。
2. **pinmux**：把 GPIO72/73 从 GPIO 功能改为 CAN2 功能（TX/RX）。
3. **★ 屏 DC/RST 让位（实施方案 R3）**：GPIO72/73 当前是 ST7789 屏的 DC/RST。
   - 把屏的 DC/RST **迁到另两个空闲 GPIO**（屏是纯 GPIO 时序，随便挪两个可用脚）。
   - 同步改 `config.json` 的 `display.gpio_dc` / `display.gpio_rst` 为新脚号（见 `_can2_conflict` 注）。
   - 若嫌麻烦，也可临时 `display.enabled=0` 关屏先把 CAN 跑通，屏后面再迁。
4. 重启后 `ip link show` 应出现 `can0`。

> **不想动设备树/屏脚** → 直接第 4 节 Plan B（USB-CAN），零内核改动。

---

## 3. 起总线 + 开机自启

**手动起（调试）**：
```bash
ip link set can0 type can bitrate 500000 restart-ms 100
ip link set can0 up
ip -details -statistics link show can0     # 看到 state ERROR-ACTIVE / bitrate 500000 即 OK
```

**开机自启**：本仓库已带 `deploy/systemd/can0.service`（可选服务）。
```bash
# 拷到板子后
install -m 644 can0.service /etc/systemd/system/can0.service
systemctl daemon-reload
systemctl enable --now can0.service
```
> `restart-ms 100` = bus-off 后内核 100ms 自动重上线（对应 F4 的 `ABOM=ENABLE`）。
> 默认 `link.type=uart` 时**不要**给 `patrol.service` 加 `Requires=can0.service`，否则 can0 起不来会拖垮主服务。切到 `can` 后再按 `can0.service` 头部注释加依赖。

---

## 4. Plan B：USB-CAN（兜底，零内核改动）

插 CANable / candleLight（`gs_usb` 驱动）：
```bash
modprobe gs_usb
ip link set can0 type can bitrate 500000 restart-ms 100
ip link set can0 up
```
得到的同样是 SocketCAN `can0`，**`CanManager` 完全无感**。代价：占一个 USB 口 + 屏脚不用动。

---

## 5. 切到 CAN 链路

1. 改 `config.json`：
   ```jsonc
   "link": { "type": "can", "can_if": "can0", "bitrate": 500000, "node_id": 1 }
   ```
2. 重启 `patrol_system`，看启动日志：
   - `RobotController 就绪（传输层=CAN, can0 @ 500kbps）`
   - `[健康] 下位机链路: 正常 (CAN can0)`
3. 状态行（每 2s）会多出 `[CAN] ... CAN[err=.. boff=.. tec=.. rec=..] hb=ok`。

**一键回退**：任何异常，把 `link.type` 改回 `"uart"` 即恢复原串口链路。

---

## 6. 联调与验收（实施方案 §3.9 / §3.10）

```bash
candump -tz can0                          # 所有帧 + 时间戳
candump can0,181:7FF                      # 只看 TELE_CORE
candump can0,20000000:20000000            # ★ 只看错误帧（EMI 诊断）
cansend can0 601#2F20200164000000         # 手工发 SDO：写 0x2020 sub1 = 0x64（CLEAR_TH 等）
ip -details -statistics link show can0    # ★★ TEC/REC/bus-off 计数 —— 量化 EMI 改善核心
canbusload can0@500000 -r -t              # 总线负载率
```

- **P2 裸链路**：F4 发 `0x181`，龙芯 `candump` 看到；龙芯发 `0x201`，F4 收到驱动电机。
- **P3 过程数据**：`patrol_system` 跑起来，`statusLine` 的 `RX[tele=.. env=..]` 持续上涨、LongLook 数据正常。
- **P4 抗扰**：电机满载 30min soak，`ip -s link show can0` 的 `bus-off`/error 计数**不增长** = EMI 治好（这一步要等 PCB + 供电到位，见交接文档 §4.4 R14）。

---

## 7. 下一轮（本轮未做）

- **类 SDO 服务端在 F4**：本轮龙芯只实现 SDO **客户端**；F4 侧对象字典 + `0x1010` Flash 固化是 P1.5。
- **CAN 下 PID 参数**：走 SDO `0x2000`（P1.5）。本轮 CAN 模式下 PID 参数下发被跳过（只支持 PID 测试 `0x301 sub4`）。
- **PIDT 多帧遥测**：`0x481 sub3` PID 调参遥测为多帧（23B），本轮仅占位。
- **MissionManager（路线表/ArUco 认点）**：依赖 CAN 已通 + ArUco 依赖解决（R1，龙芯无 OpenCV）。
