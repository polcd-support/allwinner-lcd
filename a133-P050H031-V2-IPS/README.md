# P050H031-V2-IPS 面板驱动（KICKPI K5/K5C · Allwinner A133）

5.0 寸 IPS **720×1280** MIPI-DSI 面板（LCM IC **ST7703**，4 data lane，video 模式）在 Allwinner A133
（KICKPI K5/K5C，`sun50iw10p1`）Liunx SDK 上的驱动与集成补丁。

| 项 | 值 |
|---|---|
| 适用平台 | KICKPI K5/K5C（Allwinner A133） |
| SDK | `kernel/linux-4.9`（`disp2/disp`）+ `brandy/brandy-2.0/u-boot-2018` + ubuntu16.04 rootfs |
| 面板 | 5.0" IPS，720×1280（RGB Vertical Stripe），MIPI-DSI 4 data lane，video 模式，LCM IC ST7703 |
| 驱动名 | `mipi_p050h031`（**必须与设备树 `lcd_driver_name` 逐字符一致**，匹配用 `strcmp`，无兜底） |
| 驱动文件 | 内核与 u-boot 各一份（u-boot 那份用于开机 LOGO）；除类型名外两份保持同步 |
| 触摸 | 本包未启用（dtsi 内 ctp 节点 `ctp_used = <0>` 且 `status = "disabled"`；需要时见 §5） |
| 版本 | v1.0（2026-10-16） |

## 1. 文件清单

```
P050H031-V2-IPS/
├── README.md                                        ← 本文件
├── driver/kernel/mipi_p050h031.c/.h                 ← 内核驱动
├── driver/uboot/mipi_p050h031.c/.h                  ← u-boot 驱动
├── dts/lcd-mipi-p050h031.dtsi                       ← 面板 dtsi（被板级 dts include）
└── patches/
    ├── 0001-kernel-register-panel.patch             ← 内核注册（Makefile + panels.c + panels.h）
    ├── 0002-uboot-register-panel.patch              ← u-boot 注册（同上）
    ├── 0003-board-dts.patch                          ← 板级 dts：把生效的 LCD dtsi 换成 p050
    └── 0004-build-sh-lcd-list.patch                  ← build.sh 的 LCD_DTS 可选列表加入 p050
```

面板规格书与厂家初始化码原件在上一级目录：`../P050H031-V2-IPS/`（`P050H031-V2-IPS-CTP.pdf`、
`5.0HSD +ST7703 MIPI 4L 20240312.txt`，本包内同名的 T113 版本另有完整说明）。

## 2. 面板参数（= 设备树取值）

| 项 | 值 | 说明 |
|---|---|---|
| 接口 | `lcd_if = <4>`（DSI），`lcd_dsi_if = <0>`（video），`lcd_dsi_format = <0>`（RGB888），`lcd_dsi_lane = <4>` | 厂家参考配置 |
| 分辨率 | `lcd_x = <720>`，`lcd_y = <1280>` | 规格书 |
| 显示区 | `lcd_width = <62>`，`lcd_height = <110>`（mm） | 仅信息用 |
| 像素时钟 | `lcd_dclk_freq = <62>`（MHz） | 与 A133/T113 上已点亮的 p055/p050 同一取值；DSI 下 PLL = dclk×6 |
| 水平 | `lcd_hbp = <34>`，`lcd_ht = <784>`，`lcd_hspw = <4>`（hfp = 26） | `lcd_hbp` 含同步脉宽，见 §2.1 |
| 垂直 | `lcd_vbp = <16>`，`lcd_vt = <1316>`，`lcd_vspw = <2>`（vfp = 18） | 同上 |
| 背光 | `lcd_pwm_ch = <2>`，`lcd_pwm_freq = <50000>`，`lcd_pwm_pol = <1>` | K5/K5C 板级（不同板可能不同） |
| 复位 | `lcd_gpio_0 = <&pio PE 5 1 0 3 1>`（RESET） | K5/K5C 板级 |
| 电源/背光使能 | `lcd_gpio_1 = <&pio PD 21 1 0 3 1>` | K5/K5C 板级 |
| 电源名 | `lcd_pin_power = "dcdc1"`、`lcd_pin_power1 = "eldo3"`、`lcd_power = "dc1sw"` | 沿用 SDK 约定 |
| 开机背光 | `lcd_backlight = <255>` | dtsi |

### 2.1 `lcd_hbp` / `lcd_vbp` 含同步脉宽

SDK 的 dts 头注释（`board-mipi-lcd.dts`、`lcd-mipi-p055h022.dtsi` 同）：

```
;lcd_hbp:  hsync back porch(pixel) + hsync plus width(pixel)
;lcd_vbp:  vsync back porch(line)  + vysnc plus width(line)
```

本屏 720×1280 上实测可用的取值是 `hbp 34 / ht 784 / hspw 4`、`vbp 16 / vt 1316 / vspw 2`
（即 hbp 里已含 4 列同步、vbp 里已含 2 行同步）。若把规格书的纯 HBP/VBP 原样填入，每帧少 4 列 / 2 行消隐，
DSI 视频流与面板不匹配，现象为只有背光、无画面。

### 2.2 初始化码来源

`lcm_initialization_setting[]` 是厂家 `5.0HSD +ST7703 MIPI 4L 20240312.txt` 的**逐字节**实现
（17 条寄存器写）+ `0x11`（sleep out，250 ms）+ `0x29`（display on，50 ms）；表内每条为
`{type, delay_ms, len, cmd, data...}`，`type = 0x39` 走 DCS 长写，延时在该条命令之后执行。
同一份表在 A133 与 T113 上均已点亮（T113 侧的详细调试记录见上一级目录的同名包）。

## 3. 集成步骤

### 3.1 拷贝驱动与 dtsi

| 本包 | 复制到 SDK（`<SDK>` = SDK 根目录） |
|---|---|
| `driver/kernel/mipi_p050h031.c` `.h` | `kernel/linux-4.9/drivers/video/fbdev/sunxi/disp2/disp/lcd/` |
| `driver/uboot/mipi_p050h031.c` `.h` | `brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/lcd/` |
| `dts/lcd-mipi-p050h031.dtsi` | `kernel/linux-4.9/arch/arm64/boot/dts/sunxi/` |

### 3.2 注册（每棵树 3 处）与板级 dts

```sh
cd <SDK>
patch -p1 < patches/0001-kernel-register-panel.patch
patch -p1 < patches/0002-uboot-register-panel.patch
patch -p1 < patches/0003-board-dts.patch        # 让 kickpi-k5/k5c 使用本面板
patch -p1 < patches/0004-build-sh-lcd-list.patch
```

四个补丁均以 SDK 原始 `HEAD` 为基准（已在干净副本上用 `patch -p1 --dry-run` 校验通过），
只包含本面板的改动；若你的树已有其它面板改动，可改用下面的手工改法。

手工等价：

- `.../disp2/disp/Makefile`：`disp-objs` 追加 `lcd/mipi_p050h031.o`
- `.../lcd/panels.c`：`panel_array[]` 加入 `&mipi_p050h031_panel,`（放在 `NULL` 之前）
- `.../lcd/panels.h`：加入 `extern struct __lcd_panel mipi_p050h031_panel;`（u-boot 用 `extern __lcd_panel_t`）
- 板级 dts（`device/config/chips/a133/configs/c3/kickpi-k5.dts` 与 `kickpi-k5c.dts`，只允许一个 LCD dtsi 生效）：

```diff
-#include "lcd-lvds-7-1024-600.dtsi"
+#include "lcd-mipi-p050h031.dtsi"
```

- `build.sh` 的 `LCD_DTS=(...)` 数组加入 `"lcd-mipi-p050h031.dtsi"`（决定可选项与出图文件名）

面板驱动是**无条件编入**内核/u-boot 的（不挂 `CONFIG_LCD_SUPPORT_*`），
所以 p055 与 p050 可以同时在树里，只靠改 `lcd_driver_name` / include 选择。

## 4. 编译与出图（本次已实测）

```sh
cd <SDK>
./build.sh            # 全量：u-boot → kernel/rootfs → pack（当前 DTS_NAME=kickpi-k5c）
```

产物：`out/update-a133-kickpi-k5c-ubuntu16.04-mipi-p050h031-<YYMMDDHH>.img`
（文件名含小时时间戳，同一小时内重编会覆盖；只精确到小时）。
本次构建产出：

```
out/update-a133-kickpi-k5c-ubuntu16.04-mipi-p050h031-2026101610.img
```

### 4.1 构建产物核对（本次实测结果）

| 检查项 | 命令 / 结果 |
|---|---|
| 全流程 | `build ... ok` / `pack finish`，EXIT=0（Dragon `image.cfg SUCCESS`） |
| 内核符号 | `nm out/a133/c3/bsp/vmlinux \| grep p050` → `mipi_p050h031_panel`（另一路 `mipi_p055h022_panel` 也在） |
| u-boot 目标 | `.../u-boot-2018/.../lcd/mipi_p050h031.o` 已生成 |
| 内核 dtb | `out/a133/c3/bsp/dtc -I dtb -O dts out/a133/c3/bsp/sunxi.dtb` → `lcd_driver_name = "mipi_p050h031"`、`lcd_x=720`、`lcd_y=1280`、`lcd_dclk_freq=62`、`lcd_pwm_ch=2`、`lcd_hbp=34`、`lcd_ht=784`、`lcd_vbp=16`、`lcd_vt=1316` |
| 镜像 | 见上（4.4 GB，含 ubuntu16.04 rootfs） |

`out/a133/c3/bsp/sunxi.dtb` 就是打进镜像的那份；核对它才代表“烧进去的是哪一版参数”。

**硬件点亮状态：本面板尚未在 A133 上上板验证**（同平台的 `mipi_p055h022` 已由用户实测点亮，
见 `../P055H022-IPS-CTP/README.md`；两者除初始化码外时序完全一致）。

## 5. 与 P055 互切 / 触摸

- 互切：板级 dts 里只保留一个 `#include "lcd-mipi-*.dtsi"`；`build.sh` 的 `LCD_DTS` 数组决定可选列表。
  本次已把 p050 的 include 以注释形式加在 p055 之后，切换只需对调两行注释。
- 触摸：dtsi 里带了一个 `ctp` 节点（GT911，`&twi0`，地址 0x5D，INT PE3 / 唤醒 PE4），
  当前 `ctp_used = <0>` 且 `status = "disabled"`。本面板在 A133 上未接触摸；
  若需要启用，把 `ctp_used` 改回 `<1>`、`status` 改回 `"okay"` 并按实际接线核对 `ctp_int_port`/`ctp_wakeup`。

## 6. 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| 只有背光，串口无 `[LCD] ... panel init` 打印 | `lcd_driver_name` 与驱动 `.name` 不一致（必须逐字符相同），或 `lcd_used` 不为 1 |
| 出图文件名里的屏名不对 / 变空 | `build.sh` 的 `LCD_DTS` 数组必须包含当前生效的 `lcd-*.dtsi`；`build` 函数据此推导 `LCD_NAME` |
| 改完 dtsi 但整机镜像没更新 | 先 `./build.sh`（kernel/u-boot/dts）再 pack；只改 dtsi 不会自动重打包 |
| 想同时保留多块屏 | 面板驱动无需 Kconfig，全部无条件编入；切换只改 dtsi 的 `lcd_driver_name` 与时序 |
| 无触摸 | 见 §5；本包默认关闭 |

## 7. 许可

驱动文件沿用 Allwinner SDK 的 GPL-2.0 头；初始化码来自面板厂提供的资料，仅供本面板适配使用。
