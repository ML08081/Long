# 龙芯启动、Wi-Fi 驱动重启崩溃排查与修复

日期：2026-10-23。WSL 项目 `/opPJ/PatrolSystem`。应用维持 1.25.4（4ce11a3），本次修复驱动及部署配置。

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
