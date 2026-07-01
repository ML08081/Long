# kernel-modules — 龙芯 2K0300 内核模块交叉编译（补 UVC 摄像头驱动）

为 **龙芯 2K0300 / Linux 4.19 / 旧世界(old-world)** 交叉编译缺失的内核模块，
主要目标：**`uvcvideo`（USB 摄像头 UVC 驱动）+ 其依赖的 V4L2 核心**，
编成可加载 `.ko`，打包迁移到板子上 `insmod/modprobe` 加载，
让 `/dev/video0` 出现，供 PatrolSystem 的 CameraManager 使用。

复用工程已装好的**旧世界 GCC 8.3 工具链**（`../scripts/detect_toolchain.sh` 自动定位）。

---

## 已探明的板子事实（来自 D 盘产品资料 loongOS 镜像）

| 项 | 值 |
|----|----|
| 内核版本 | **`4.19.190-rt79-yocto-standard`**（Yocto/poky，**PREEMPT_RT** 实时补丁，LoongArch 旧世界）|
| USB 栈 | 已**内建**（usbcore/xhci-hcd/xhci-pci/dwc2）✔ 不需另编 |
| V4L2/UVC | **完全缺失**（既非内建也无模块）→ 需编 `videodev`+`videobuf2-*`+`uvcvideo` 全套 |
| 内核 build 树 | 镜像里**没有**（rootfs 不含）→ 见下方"关键前提" |
| 厂商模块范式 | `make -C /lib/modules/$(uname -r)/build M=$PWD modules`（在目标机编）|

> ⚠️ 编 `uvcvideo/videodev` 这类**内核自带驱动**必须要**完整内核源码树**（含 `drivers/media/*.c`）；
> 光有 kernel-headers 不够。因此第0步先跑 `00_probe_board.sh` 判断板子上的 `build` 是完整源码还是仅 headers。

---

## ⚠️ 能否成功的前提（务必先读）

交叉编译的 `.ko` 要能被板子加载，必须与板子上**正在运行的内核**匹配：

1. **内核版本串**：`uname -r` 必须一致（vermagic 第一段）。
2. **vermagic**：由 内核版本 + `CONFIG_SMP` / `CONFIG_PREEMPT` / 模块布局 + 编译器 决定，
   必须一致。→ 所以要用**和板子相同大版本的编译器**（旧世界 GCC 8.3）和**相同 `.config`**。
3. **Module.symvers**：若板子内核开了 `CONFIG_MODVERSIONS=y`，符号 CRC 必须一致，
   需要板子内核构建时的 `Module.symvers`。缺了它，`modprobe` 会因符号 CRC 不符而拒绝加载。

**结论**：不能凭空编。必须先拿到 ① 板子的 `uname -r`、② `.config`、③（可能的）`Module.symvers`，
再据此获取"完全匹配"的内核源码。这就是 `00_probe_board.sh` 的作用。

---

## 流水线

```
第0步  在板子上采集内核信息
        scp scripts/00_probe_board.sh root@<板子IP>:/tmp/
        ssh root@<板子IP> 'sh /tmp/00_probe_board.sh'
        scp root@<板子IP>:/tmp/board-kinfo.tar.gz .
        # 把 board-kinfo.tar.gz 发给我 → 我据此锁定源码版本

第1步  获取匹配的内核源码 (2K0300 BSP / linux-4.19)
        bash scripts/01_fetch_kernel_src.sh     # 需先填好源码地址(见脚本内说明)

第2步  交叉编译模块 (uvcvideo + V4L2 依赖)
        KERNEL_SRC=/path/to/linux-4.19 \
          bash scripts/02_build_modules.sh

第3步  打包
        bash scripts/03_make_package.sh
        # 生成 build/uvc_modules_loong.tar.gz

第4步  部署到板子
        scp build/uvc_modules_loong.tar.gz root@<板子IP>:/tmp/
        ssh root@<板子IP> 'cd /tmp && tar xzf uvc_modules_loong.tar.gz && cd uvc_pkg && ./install.sh'
```

## 目录结构

```
kernel-modules/
├── config/uvc.config          # 启用 UVC + V4L2 的 .config 片段(编成 =m)
├── scripts/
│   ├── 00_probe_board.sh      # 【板子上跑】采集内核身份信息
│   ├── 01_fetch_kernel_src.sh # 【主机】下载匹配内核源码
│   ├── 02_build_modules.sh    # 【主机】交叉编译 .ko
│   └── 03_make_package.sh     # 【主机】打包
├── target/
│   ├── install.sh             # 【板子上跑】depmod+modprobe 安装
│   └── load_uvc.sh            # 【板子上跑】手动按依赖顺序 insmod(兜底)
└── build/                     # 产物(gitignore)
```
