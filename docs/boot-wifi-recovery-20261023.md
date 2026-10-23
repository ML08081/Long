# 龙芯启动、Wi-Fi 驱动重启崩溃排查与修复

日期：2026-10-23。WSL 项目 `/opPJ/PatrolSystem`。应用维持 1.25.4（4ce11a3），本次修复驱动及部署配置。

## 最终核验结论（补齐修复后）

- 当前 Wi-Fi BSP 为第二版，srcversion=`41323637864F6C12665B9AA`，SHA256=`e1beea57267a72075eb3936716ca17795f26ac3f117e138ac5a32a802d93fa73`。下文 C42/476f 开头的记录属于首次修复，已被替代。
- 首次空指针检查不能防止“指针非空但对象已经释放”的情况，延长测试又复现 `aicwf_sdio_release -> __mmc_claim_host -> add_wait_queue` 崩溃。第二版在内核重启、关机、掉电清理阶段直接跳过已移除兄弟 function 的访问；系统正常运行时的原有清理流程保留。
- 第二版保留原85个导入符号CRC、11个导出符号CRC；新增 `system_state` 符号使用匹配内核CRC `0x015af7f4`。ASan/UBSan 增加带非空失效指针的重启/关机/掉电场景，全部通过。
- 第二版冷启动及两次软件重启通过。最后一次使用普通 `/sbin/reboot`，约60.9秒恢复；启动 ID `014c160c-b998-4d60-9088-d005a632e2ba`，MLML地址 `10.52.36.140`，应用 NRestarts=0。
- USB-CAN 已加入自动关机释放：仅在系统 stopping 时、确认单个0c72:000c PCAN USB和目标can2身份后，将其停用并解绑。常规服务重启不解绑，避免下次开机转换器保持无收发的残留状态。真实普通重启后，前一启动日志包含自动释放记录，当前CAN恢复 ERROR-ACTIVE，F4遥测、环境、心跳正常。
- PB8蜂鸣器静音初始化已提前至系统时钟配置之前，F4已刷入并校验。运动/速度算法和热像报警联动策略未修改。
- 当前仅连接CAN和调试串口，屏幕、摄像头、热像专线没有接回。对应实际显示、图像和整车行驶仍待接线验收，不能承诺未经实测的硬件功能全部正常。全程无非零电机指令。

最终证据优先查看：`reboot-final-verification.log/json`、`reboot-can-clean-v2-verification.log/json`、`wifi-driver-build-v2.json`、`final-stationary.json`、`final-f4-validation.json`、`f4-early-buzzer-flash.log/json`。下面保留此前诊断过程，便于追溯。

## 现场事实

- 电脑可正常连接 2.4GHz MLML；初始无法访问龙芯旧地址。
- 通过 USB-TTL 的 UART0（IO1=GND、IO4=TX、IO6=RX，115200 8N1）取得 PMON、Linux、systemd 完整启动输出。
- 龙芯独立供电冷启动能进入 Linux，自动连接 MLML 并取得 `10.52.36.140`，应用 active。
- 普通 `/sbin/reboot` 复现内核崩溃：`aicbsp_sdio_remove+0xcc/0x1a0` 空指针，随后 `Kernel panic - not syncing: Attempted to kill init!`。崩溃发生在文件系统已卸载、内核执行设备关闭的阶段，设备没有重新开机。
- 原厂 BSP 绑定 SDIO function 2，却保存 function 1 的指针；MMC 先移除 function 1，后续 BSP remove 回调仍通过已失效的 `func->card` 读取 host。反汇编故障指令与原厂源码一致。
- 原厂 `/etc/rc.local` 还包含重复 insmod、hostapd、192.168.1.3、udhcpd 的 AP 启动逻辑。本次启动因重复加载模块失败；该入口也存在与 Station 联网竞争的风险。
- 屏幕初始化日志及 GPIO72/73、SPI2 复用状态正常。用户独立排查时断开了屏幕、F4/USB-CAN，因此本次不能把 SPI 发送成功当作实物画面验证，can2 缺失也是当前连接状态的预期结果。

## 修复内容

1. 仅修复 AIC8800 BSP remove 回调的失效 function/card/host 访问；已失效时清除全局旧句柄并返回，正常清理路径保持。
2. 在有 Patrol Station 配置时跳过已确认的原厂 AP rc-local 入口；原文件保留。安装器仅对同时匹配 AIC8800 和原厂 hostapd 命令的 rc.local 安装保护。
3. LoongOS 的 `/var/log` 指向内存目录，早启动 journald 还持有独立挂载命名空间。增加 tmpfiles 链接、journald BindPaths 和启动后 flush，使日志真正保存到根文件系统 `/var/lib/patrol/journal`，上限32MB、保留14天。
4. 部署脚本与打包脚本同步包含这些配置。未修改应用控制算法、速度档位、F4 固件或热点凭据。

## 核验

- 内核 `4.19.190+ SMP mod_unload modversions LOONGARCH 64BIT` 匹配。
- 原驱动与修复驱动的85个导入符号 CRC、11个导出符号 CRC逐项一致。
- 实际回调在 ASan/UBSan 下覆盖空句柄、无bus、无function、已移除card、无host、无SDIO数据、正常清理和重复回调，全部通过。
- 已加载驱动 srcversion=`C42D76595336C9F92C5F9BD`。
- 修复后已完成冷启动及两次普通软件重启；原驱动崩溃未再出现。第二次完整自动核验通过，耗时约 56.5 秒（包含关机及重新启动）。
- 自动连接 MLML、DHCP地址10.52.36.140、应用active且NRestarts=0、SPI/GPIO初始化、AP入口ConditionResult=no均通过。
- `journalctl --list-boots` 已显示重启前后的多次启动，确认日志落在 ext4 根文件系统。
- shell语法、Git补丁格式和9项联网回归通过。没有发送非零电机运动命令。

证据保存在 Windows `diagnostics-20261023`：`reboot-panic.log`、`wifi-driver-build.json`、`aic-remove-validation.log`、`driver-trial-cold-boot.log`、`fixed-driver-confirmed.log`、`reboot-verification.log/json`。

## 驱动与回退

- 原驱动 SHA256：`c5fe8bd9b3f592eab4646c4465a1bd597481c901469e28bd1dc65a3000b6b8a6`。
- 修复驱动 SHA256：`476f43d7fc8e36ab25eb030774e146e1b0eb642c5e726a692e12a196301a4e15`。
- 原驱动保存在板端 `/var/lib/patrol/wifi-driver-trial/aic8800_bsp.original.ko`；配置备份在 `/root/patrol-boot-backup-20261023-legacy-ap`。
- 本次试装设置了五分钟未确认则恢复磁盘旧驱动的 timer；已核验修复驱动并确认，timer 已停止、禁用，保留备份。恢复驱动文件后应断电启动，不能热卸载存在缺陷的旧驱动。
- `/boot`、PMON、内核镜像和设备树未改动。重编仅针对匹配的 BSP 模块，原 WLAN 驱动及固件保留。

构建：`CROSS_COMPILE=/工具链路径/loongarch64-linux-gnu- bash kernel-modules/scripts/04_build_aic_reboot_fix.sh 匹配内核源码目录 匹配内核构建目录`。发布前仍须比较原驱动的 vermagic 和全部符号 CRC，不能将模块用于其它内核。

## 整机验收范围

原先整机首次上电黑屏及不上网的具体供电条件没有完整日志，本次复现并修复的是软件重启崩溃及启动配置冲突。独立运行通过不能替代回装后的供电、真实画面、USB-CAN及F4通信验收。待断电回装、重新上电后继续通过串口和网络核查，保持停车。

### 回装检查进展（2026-10-23）

- 用户已回装整机并上电。14:53 通过 SSH 取得新冷启动 ID `5a16e5c2-e7b1-436d-9ab2-e67c8d2f919d`，修复驱动 `C42D76595336C9F92C5F9BD` 已加载，MLML 连接完成、地址仍为 `10.52.36.140`。
- 应用与联网服务 active，应用 NRestarts=0；USB-CAN can2 500 kbit/s 恢复，F4 心跳及环境遥测持续更新，电机目标/反馈/PWM 均为零，运动故障为零。
- 日志记录屏幕完成初始化并启动显示线程，但用户实物确认仍黑屏，不能据此判定屏幕正常。
- 本次启动前段热像原始帧递增，但尚无完整 EEPROM 及温度解算帧；温湿度、摄像头和热像实时显示尚未完成验收。
- 随后电脑仍连接 MLML、热点网关可达，但龙芯 SSH 连接超时；同网段 SSH 扫描也未发现龙芯，暂不能区分无线断链、系统卡死或供电问题。
- 用户确认回装后未连接 USB-TTL，因此缺失失联时的串口现场。后续需保持故障现场供电、接回 USB-TTL 获取运行及内核日志后继续排查；整机验收仍未通过。没有发送非零电机指令。

证据：`diagnostics-20261023/assembled-preflight.log`。独立启动修复已提交并推送至分支 `feat/local-control-optimization-20261008`，提交 `431ce25fb11650a098e3f58e4a7ed0caad7a0599`；原 `v1.25.4` 标签保留。

### 串口接回后的核查与更正

- 用户最终明确：当前龙芯业务连接只有 CAN，屏幕、摄像头和热像专线都没有接回；另接 USB-TTL 和 Wi-Fi。因此当前黑屏、摄像头未枚举、热像接收字节为零符合接线状态，不能作为这些硬件损坏或验收通过的依据。
- 已取得新启动日志，MLML 联网恢复。无法据此倒推出此前失联一定由供电或另一场内核崩溃引起。
- 实测温湿度约32°C、43～44%，成功采样序号持续增长；CAN 恢复后遥测、环境帧、心跳正常，电机目标、反馈、PWM 为零。
- 进一步发现早启动 journald 的 BindPaths 保留只读根文件系统挂载状态，且被后续 `/var/volatile` 挂载影响。已将持久化单元改为在本地文件系统重新挂载和 tmpfiles 准备完毕后，重新创建 journald 命名空间再 flush。真实软件重启后，日志文件位于 ext4 `/var/lib/patrol/journal`，能读取重启前后的启动记录。
- 上述最终日志/联网重启核验通过，约62.2秒，新启动 ID `cbbf01ee-65c2-471d-aa80-9449a966fc73`。该检查只验证 CAN 接口存在；随后单独检查收发，发现转换器在这次软件重启后未恢复数据。不能将接口 UP 当作 F4 通信正常。

### PB8 低电平蜂鸣器修复

用户确认蜂鸣器控制线为 PB8，并确认之后已不再鸣响。原来的高电平关闭、低电平鸣响配置本身正确；现场读取 PB8 输入/输出也为高电平，软件报警标志关闭。

本次增加 `Buzzer_EarlySilence()`，在 `HAL_Init()` 后、系统时钟及其他外设初始化之前调用：使能 GPIOB 时钟，先预置高电平，再配置推挽输出及上拉，缩短固件启动期间的输入悬空窗口。常规初始化保持幂等，真实报警的鸣响节奏保留。固件开始执行之前的上电电平仍由硬件决定。

- F4 完整编译通过，FLASH占用111520字节、RAM54560字节。
- 已经 ST-Link 烧录、校验成功并复位；后续 PB8 高电平实测通过。
- 刷写前读取128KB备份，与原1.25.4发布二进制前缀逐字节一致。备份 SHA256：`be434df5553f43124e259c1a96d669b7da3cbd25a3089f120a24fdb3d8d6d39a`。
- 蜂鸣器、F4运行控制、运动策略、编码器、循迹、避障意图、I2C恢复七组源码回归全部通过。
- CAN恢复后，100～2000的全部20档设置均获得F4确认并验证保存，最终恢复500档、热像AUTO和急停，无非零运动指令。

### USB-CAN 软件重启后恢复

实测出现：接口 UP、位速率正确，但 CAN 状态 STOPPED，RX和USB接收中断计数为零，F4采样仍在运行。接口重新启停、PCAN重新绑定和USB逻辑复位未恢复；用户仅重新插拔USB-CAN后立即恢复 ERROR-ACTIVE、遥测及档位确认。

已确认重启前释放PCAN连接可以避免本次转换器残留状态，并安装自动处理。普通重启实测通过，详见本文最终核验结论。屏幕、摄像头、热像实际图像及整车落地行驶仍需接回硬件后验收。

新增证据：`reboot-journal-verification.log/json`、`final-f4-validation.json`、`f4-early-buzzer-flash.log/json`、`f4-before-early-buzzer.bin`、`final-stationary.json`。
