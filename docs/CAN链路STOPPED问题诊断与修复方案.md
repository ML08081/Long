# 龙芯板载 CAN 链路 STOPPED 问题：诊断报告与修复方案

> 日期：2026-07-21　平台：龙芯 2K0300 / LoongOS 内核 4.19.190+　分支：`feat/f4-serial-control`
> 关联记忆：`loong-can-base-layer` / `f4-serial-control` / `dev-state-next-steps`

---

## 0. 一句话结论

龙芯板载 CAN（`can0`/`can1`）`ip link up` 后控制器停在 **`can state STOPPED`**，无法收发（连内部 loopback 都全 drop）。根因在**龙芯闭源 CAN 驱动 blob（`ls,lscanfd`）的 `open` 实现**——它走完“成功路径”却没把 `can.state` 正确置为 `ERROR_ACTIVE`。**与接线、120Ω 终端电阻、对端 F4、DMA 均无关。** 由于驱动只有二进制 `.o`、无源码，软件层无法直接修改；可行修复是**用 mainline 开源 `ctucanfd` 驱动替换 blob 后重编内核**。

---

## 1. 问题现象

- 需要把 F4↔龙芯通信从 UART 迁移到 CAN（500 kbps）。F4 侧 CAN 已就绪，龙芯侧接了 CAN 收发器。
- 龙芯 `ip link set can1 type can bitrate 500000 up` 命令返回 **exit=0**，但：
  - `ip -details link show can1` → `can state STOPPED`（正常应为 `ERROR-ACTIVE`）
  - `candump can1` 收不到任何帧；`cansend` 的帧全部 `TX dropped`（`packets=0`）
  - F4 侧 `[CAN] tx 涨 / TEC=128 / LEC=3(ACK) / drop≈tx`＝总线无对端 ACK（符合龙芯没真正上线）

---

## 2. 诊断过程（分层排除，每步都排掉一类因素）

| 步骤 | 操作 | 结果 | 排除的因素 |
|---|---|---|---|
| 1 | `can1` up 后看 state | `STOPPED` | 控制器没进工作态 |
| 2 | `can0` up 对比 | 也 `STOPPED` | 非 can1/接线特有，是通用现象 |
| 3 | **loopback 内部环回**测试（`loopback on`） | 仍 `STOPPED`，`cansend` 全 `dropped`、自收空 | **终端电阻/接线/收发器/对端 F4 全部排除**（loopback 不经物理总线） |
| 4 | 停 patrol + 释放 gpio74 后再 up | 仍 `STOPPED` | 屏背光 gpio74 占脚**不是**根因 |
| 5 | dmesg 检查 | 驱动 probe 正常；`get cd, can't find gpio chip` 是 **MMC card-detect 噪声**，非 CAN | 排除 dmesg 干扰项 |
| 6 | 设备树 `can@0x16110c00` 属性 | `compatible=ls,lscanfd`、`ls,clock-frequency`=100MHz（dmesg 确认时钟已拿到）、`pinctrl-0`(pin74/75)、`interrupts`、`status=okay` | **时钟/引脚/中断都不缺**，设备树配置正常 |
| 7 | 查驱动源码 | `drivers/net/can/ls_can/` 只有 Kconfig/Makefile + `lscan_platform.elf`/`lscan_olddma.elf`；Makefile 仅 `cp .elf→.o` | **驱动是闭源二进制 blob，无 .c 源码** |
| 8 | 反汇编 blob | 见第 3 节 | 定位到 `open` 未置 state |

> **重要副产物**：F4 只在 patrol 持有串口时才发 UART 数据、patrol 一 `close` 就静默——疑似串口 `close` 拉低 DTR/RTS（HUPCL）导致 F4 复位/停摆。故“停 patrol 抓原始流”不可行（另见 `dev-state-next-steps`）。

---

## 3. 根因定位（反汇编证据）

工具：`loongarch64-linux-gnu-objdump -dr lscan_olddma.elf`（`.elf` 实为 `not stripped` 的 LoongArch relocatable `.o`，保留符号）。

### 3.1 blob = 标准 CTU CAN FD 驱动的龙芯魔改版
核心符号与 mainline `ctucanfd_base.c` 一一对应（厂商把 `ctucan` 改名 `lscanfd` + 加了 DMA 收包）：

```
lscanfd_chip_start   lscanfd_open       lscanfd_close        lscanfd_start_xmit
lscanfd_interrupt    lscanfd_set_bittiming  lscanfd_set_data_bittiming
lscanfd_do_set_mode  lscanfd_get_berr_counter  lscanfd_probe_common  lscanfd_reset
lscanfd_rx_poll      lsdma_pending_rx   lscanfd_dma_rx_poll   (← 厂商加的 DMA 收包)
```

### 3.2 DMA 假设：已证伪
- blob 依赖 DMA 外部符号：`dma_request_slave_channel`、`dma_alloc_from_dev_coherent`、`dma_release_channel`、`loongson_dma_ops`。
- DMA channel 在 `lscanfd_probe_common` 申请：
  ```
  bl dma_request_slave_channel
  st.d r4, priv+0xaf8          # 存 channel
  beqz r4, .L460               # channel==NULL → .L460
  .L460: dev_err(...); r25=-19(-ENODEV); b .L400   # probe 失败返回 -ENODEV
  ```
- **但 `can1` 接口是存在的** ⇒ probe 成功 ⇒ **DMA 申请其实成功**（靠 `loongson_dma_ops` 平台级绑定，不依赖设备树 `dmas` 属性）。
- 结论：**“设备树缺 dmas → DMA 失败 → STOPPED”不成立**。DMA 通路正常。

### 3.3 真因：`lscanfd_open` 走完成功路径却没置 `can.state = ERROR_ACTIVE`
`lscanfd_open` 反汇编流程：
```
__pm_runtime_resume     → 失败跳 .L504(netdev_err)
lscanfd_reset           → 失败跳 .L502
open_candev             → 失败跳 .L505(netdev_warn)
request_threaded_irq    → 失败跳 .L506(netdev_err)
lscanfd_chip_start      → 失败跳 .L507(netdev_err + free_irq)
netdev_info(...)        ← chip_start 成功后打印
ldptr.d r12, priv+0x988 ; andi r12,1 ; beqz r12, .L144717   # 读某标志 bit0
   └─ bit0==0 → .L144717: break 0x1   ← LoongArch 陷阱指令(BUG/trap)
   └─ bit0==1 → 原子操作(amand_db) → 正常返回 0
```
实测：`ip link up` **exit=0** + dmesg **无任何 CAN 的 netdev_err** + state **仍 STOPPED**。
⇒ open 走的是**成功路径、返回 0**，但**没有把 `can.state` 置为 `ERROR_ACTIVE`**（`register_candev` 后默认即 STOPPED）。
⇒ blob 依赖内部标志 `priv->0x988` bit0，疑似在本板配置下未满足，走了“不真正上线控制器”的分支（甚至可能撞 `break 0x1`）。

**这是闭源 blob 特有的实现缺陷；无源码，软件层无法修改。**

---

## 4. 修复方案

### ⭐ 方案 A：backport mainline 开源 `ctucanfd` 驱动替换 blob（治本，推荐）

**为什么可行/利好：**
- blob 已确认是标准 CTU CAN FD IP，寄存器接口一致，开源驱动能驱动同一硬件。
- STOPPED 是 blob 特有实现问题；mainline `ctucanfd` 的 `open`/`chip_start` 逻辑正确（会设 `CAN_STATE_ERROR_ACTIVE` + `netif_start_queue`），换上去**大概率直接修好**。
- mainline 开源版**纯中断/PIO 收发、不用 DMA**，连 `dma_request` 那套都不需要，比 blob 更简单、少一堆平台绑定坑。

**建议先做低风险预研（不动主内核、不换 /boot）：**
1. 取 mainline `ctucanfd`（`drivers/net/can/ctucanfd/` 的 `ctucanfd_base.c` + `ctucanfd_platform.c` + 头文件；v5.6+ 引入，选与 4.19 CAN 子系统较近的稳定版）。
2. backport 到 4.19：适配 CAN 子系统 API 差异（`can_priv`/`alloc_candev`/`can_rx_offload`/`netdev` 相关；4.19 无 `ctucanfd` 所需的部分新 helper，需回填或改写）。
3. 改 `compatible` 匹配设备树的 `ls,lscanfd`（或临时改设备树 compatible）。
4. **编成内核模块**（`CONFIG_CAN_CTUCANFD_PLATFORM=m`），在板子 `insmod` 试：
   - 起来且 `can state` 变 `ERROR-ACTIVE`、loopback 能自收 → 方案成立。
   - 起不来也**不影响现有系统**（主内核不变）。
5. 预研通过后，再决定编进内核 / 替换 blob / 换 `/boot`。

**风险与前置：**
- 换 `/boot` 内核有**变砖风险** → 必须先确保板子有**串口 console 兜底恢复**（ttyS0=console）。
- 内核树**无 `.config`** → 先 `make loongson_2k300_defconfig`（`arch/loongarch/configs/` 下有）。
- 需交叉工具链（已在，见第 5 节）。
- 工作量：中大型（数天，含 backport 调试）。

### 方案 B：USB-CAN 适配器（最快兜底，不碰内核）
- 用 `gs_usb`/CANable 之类 USB-CAN 棒接龙芯 USB 口，完全绕开板载 CAN blob。
- 不占屏脚、不重编内核、零变砖风险；硬件到手即用。
- **前置确认**：板子是否有 `gs_usb`/`slcan` 驱动（2026-07-21 `kallsyms` 未命中 `gs_usb`，需再确认或补驱动）。

### 方案 C：暂回退纯 UART（不阻塞业务）
- CAN 搁置，先用现有 UART 主链路把巡检跑通（`config.json` `link.type=uart`，现网默认即此）。
- F4 侧 CAN 保持并行发送不影响 UART。
- 优先解决三个缺数据的传感器（见 `f4-serial-control`：热成像 MLX90640 I2C3 read fail / 超声波 / 激光）。

---

## 5. 关键环境信息与命令速查

**路径：**
- 内核源码：`/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main`（root 拥有，molim 只读；4.19.190）
- CAN blob：`drivers/net/can/ls_can/{lscan_platform.elf, lscan_olddma.elf, Makefile, Kconfig}`
- defconfig：`arch/loongarch/configs/loongson_2k300_defconfig`（树内无 `.config`，需先生成）
- 交叉工具链：`/home/molim/toolchains/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/`（`loongarch64-linux-gnu-*`）
- 本次反汇编存档：`/home/molim/candis.txt`（`objdump -dr` 全量）、`/home/molim/canund.txt`（UND 外部符号）
- 板子：`root@192.168.3.100`（免密 SSH），`ip` 在 `/sbin/ip`，`candump/cansend` 有（老版），CAN 驱动**内建**（无 lsmod 模块）

**设备树（`/sys/firmware/devicetree/base/soc/`）：**
- `can@0x16110800`=can0=CAN2（pin72/73，与屏 DC/RST 冲突，**禁止 up**）
- `can@0x16110c00`=can1=CAN3（pin74/75，屏 BL 在 pin74，切 CAN 需 `display.gpio_bl=-1`）

**复现/诊断命令：**
```bash
export PATH=$PATH:/sbin
# 起 can1 并看 state（关键：应为 ERROR-ACTIVE，实测 STOPPED）
ip link set can1 down; ip link set can1 type can bitrate 500000; ip link set can1 up
ip -details link show can1 | grep "can state"
# loopback 自测（绕开物理总线，判断是否控制器本身问题）
ip link set can1 down; ip link set can1 type can bitrate 500000 loopback on; ip link set can1 up
( timeout 3 candump can1 & ) ; cansend can1 123#DEADBEEF ; sleep 1
ip -statistics link show can1 | grep -A2 TX:   # TX packets=0 → 控制器没工作
# 反汇编（确认 open 未置 state / 依赖标志）
$TC/loongarch64-linux-gnu-objdump -dr .../ls_can/lscan_olddma.elf > candis.txt
```

**F4 侧 CAN 现状判读（`[CAN]` 日志）：** `TEC=128 / LEC=3(ACK) / drop≈tx` = 总线无对端 ACK，属正常（龙芯没上线）；一旦龙芯 CAN 真正 ERROR-ACTIVE 并接好收发器，TEC 会回落、drop 停止。

---

## 6. 决策建议

- 想**尽快不阻塞** → 方案 B（USB-CAN）或方案 C（回退 UART 推进业务）。
- 想**治本用板载 CAN** → 方案 A，且**先做“编成模块 insmod 试”的低风险预研**再决定是否换内核（把变砖风险降到最低）。
- 三个方案不互斥：可先 C 保证业务推进，并行做 A 的预研。
