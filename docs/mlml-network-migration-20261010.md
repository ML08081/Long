# MLML 热点迁移与回退方案（2026-10-10）

本次网络源码改为只连接手机热点 `MLML`，通过 DHCP 获取地址、网关和 DNS。Wi-Fi 密码由部署时提供，只保存在设备 `/etc/patrol/wifi-password`，权限 `600`，不写进源码、命令参数或日志。上位机和开发板加入同一热点后，LongLook 通过 UDP `8083` 自动发现开发板，TCP `8080` 用于控制与数据。

## 已修复的配置问题

旧 `deploy/network.conf` 把 `B410` 设为高优先级主网络，`MLML` 为低优先级备用，升级安装还会保留设备原配置。因此仅更新程序无法确保切换到手机热点。旧脚本在 DHCP 后仍写入 B410 网关作为 DNS；无线重新关联也没有明确触发 DHCP 重新申请。

- 新源码模板仅含 `MLML`，清空旧固定 IP、静态 SSID、网关和 DNS。
- `setup_network.sh` 先验证外置密码并原子生成权限 `600` 的 WPA 配置，再调整连接；缺失密码时保留现有连接配置。
- `dhcp_hook.sh` 使用热点分配的地址、路由和 DNS；同租约续租保留现有 IP，幂等更新路由与 DNS，避免删除地址造成 TCP 断连。换地址或前缀时先安装新地址，再移除旧地址，只有明确 `deconfig` 才清空地址。
- `wifi_link_action.sh` 在重新关联时通知 udhcpc 续租，处理手机热点重新开启或网段改变。
- `patrol-network.service` 连接失败会自动重试，主程序启动保持独立。
- 安装和打包脚本收录网络 helper；保留旧设备配置，迁移通过明确的可回退步骤执行。

## 设备部署顺序

先保持现有 SSH/串口连接，备份设备原 `/usr/bin/patrol_setup_network` 与 `patrol-network.service`。在完整版本部署或暂存目录中，安装以下脚本和服务；安装文件本身不会切换当前连接。

```sh
install -m 755 deploy/scripts/setup_network.sh /usr/bin/patrol_setup_network
install -m 755 deploy/scripts/dhcp_hook.sh /usr/bin/patrol_dhcp_hook
install -m 755 deploy/scripts/wifi_link_action.sh /usr/bin/patrol_wifi_link_action
install -m 755 deploy/scripts/migrate_network.sh /usr/bin/patrol_network_migrate
install -m 644 deploy/systemd/patrol-network.service /etc/systemd/system/patrol-network.service
systemctl daemon-reload
```

确认 Bash、awk、wpa_supplicant、wpa_cli、udhcpc、ip 和 systemd-run 可用，再执行迁移。设备脚本仅使用现有 shell 工具，避免依赖 LoongOS 裁剪版 Python 缺失的标准库：

```sh
patrol_network_migrate stage --ssid MLML --timeout 300
```

在密码提示中输入用户提供的热点密码，或通过受保护的标准输入提供，避免将密码拼接进命令参数。脚本在设备 `/var/lib/patrol/network-transition/` 中保存权限 `700` 的配置备份目录，启用本地回退 timer，然后延迟 3 秒切换网络。回退 timer 由设备 systemd 管理，不依赖 SSH 会话，并在设备重新启动后恢复倒计时。

设备获得新地址后，在手机的已连接设备列表或 LongLook 自动发现结果中确认开发板地址。使用新地址重新 SSH 登录，并先检查：

```sh
wpa_cli -i wlan0 status
ip -4 addr show dev wlan0
ip -4 route show
systemctl is-active patrol.service
```

`ssid` 必须为 `MLML`，`wpa_state` 为 `COMPLETED`，地址和网关属于手机热点网段；上位机应能通过新地址连接 `8080`。确认成功后取消回退：

```sh
patrol_network_migrate confirm
```

脚本拒绝在仍连接旧 SSID、没有 IPv4 地址或只有 `169.254.*` 自分配地址时确认。300 秒内未确认则自动恢复原网络配置并重新连接。也可通过板端串口主动执行：

```sh
patrol_network_migrate rollback
```

`patrol_network_migrate status` 显示待确认迁移的 SSID、备份路径和状态，不显示密码。成功确认后保留配置备份供人工恢复。切换期间应保持电机停车；Wi-Fi 验证无需启动电机。

## 本地验证结果与实机限制

在 WSL `/opPJ/PatrolSystem` 执行 shell 语法检查和 `tests/local_validation/validate_network_migration.py`。测试直接运行设备实际的 shell 脚本，将路径和外部命令替换为临时目录及 mock，设备端不需要 Python。9 项验证包括原配置精确回退、有效热点地址才确认、回退 timer 启动失败不切网、延迟切网调度失败恢复配置、非法输入被拒绝、DHCP 地址/路由/DNS 与非法掩码验证、密码转义/权限和缺少密码时保留原配置，以及同租约连续 `bound/renew/renew` 不删除 IP、不清空默认路由和跨热点先安装新地址再移除旧地址的回归。

本次按用户要求先在旧网络完成 1.25.4 更新及下次启动的 MLML 配置，随后实机已启动并自动连接 MLML：wpa_state=COMPLETED，DHCP 地址 10.52.36.140，patrol.service 和 patrol-network.service 均 active。旧热点配置及程序保存于 /root/patrol-upgrade-backup-20261010-190550-1254。本次使用下次启动切换，未启动迁移回退计时器；上文在线迁移命令供后续变更时使用。最终同地址续租修复已部署，相关证据见 upgrade-20261010 的网络和设备日志。手机热点地址由 DHCP 分配，不承诺固定地址。若手机开启了客户端隔离，应关闭隔离或允许热点内设备互访。

新源码不再保存 Wi-Fi 密码；已有 Git 历史和旧部署归档可能仍含旧配置，后续正常提交不会删除历史记录。当前任务不改写仓库历史。
