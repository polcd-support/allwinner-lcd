# P055H022-IPS-CTP 面板驱动（KICKPI K4B / Allwinner T113-S3）

5.5 寸 IPS **720×1280** MIPI-DSI 面板 + 电容触摸（LCM IC：**HX8394F**，4 data lane，video 模式；CTP：**GT911**）的驱动与集成补丁。

| 项    | 值                                                                                                                |
| ---- | ---------------------------------------------------------------------------------------------------------------- |
| 适用平台 | KICKPI K4B（Allwinner T113-S3）；Linux SDK：`kernel/linux-5.4` + `brandy/brandy-2.0/u-boot-2018` + buildroot 2019.02 |
| 面板   | 5.5" IPS，720×1280（RGB Vertical Stripe），MIPI-DSI 4 data lane，video 模式，LCM IC HX8394F                              |
| 触摸   | GT911，TWI1 @ 0x5d，INT PD17 / 唤醒 PD16，`ctp_used = <1>`，`ctp_screen_max_x = <720>`                                 |
| 驱动名  | `P055H022-IPS-CTP`（**必须与设备树 `lcd_driver_name` 逐字符一致**，匹配用 `strcmp`，无兜底）                                          |
| 驱动文件 | 内核与 u-boot 各一份（u-boot 那份用于开机 LOGO），两份除类型名外内容相同                                                                   |
| 版本   | v1.0（2026-10-16）                                                                                                 |

## 1. 文件清单

```
P055H022-IPS-CTP/
├── README.md                                  ← 本文件
├── P055H022-IPS-CTP.pdf                       ← 面板规格书（厂家原件）
├── 新_HSD055BHW6-C11HX8394F+HSD5.45.txt       ← 厂家初始化码（扁平格式 + 时序头）
├── driver/kernel/P055H022-IPS-CTP.c/.h        ← 内核驱动
├── driver/uboot/P055H022-IPS-CTP.c/.h         ← u-boot 驱动
├── dts/board-mipi-lcd.dts                     ← 内核板级 dts
├── dts/uboot-board-mipi-lcd.dts               ← u-boot 板级 dts
├── patches/
│   ├── 0001-kernel-register-panel.patch       ← 内核注册（Makefile + panels.c + panels.h）
│   ├── 0002-uboot-register-panel.patch        ← u-boot 注册（同上）
│   └── 0003-board-dts.patch                   ← 板级 dts 相对 SDK 原始文件的改动
├── refs-a133/                                 ← A133 平台同面板驱动快照（对照/回归用）
├── logo/bootlogo-720x432.bmp                  ← 720×1280 fb 用开机 LOGO（24bpp）
└── tools/init_table.py                        ← 初始化码 ↔ C 表 转换/校验工具（标准/AW/RK/扁平四种格式）
```

## 2. 面板参数（= 设备树取值）

| 项           | 值                                                                                                                        | 来源                             |
| ----------- | ------------------------------------------------------------------------------------------------------------------------ | ------------------------------ |
| LCM IC / 尺寸 | HX8394F / 5.5" IPS，显示区 68.04×120.96 mm                                                                                   | 规格书                            |
| 分辨率         | 720×1280（RGB Vertical Stripe）                                                                                            | 规格书                            |
| 接口          | `lcd_if = <4>`（DSI），`lcd_dsi_if = <0>`（video），`lcd_dsi_format = <0>`（RGB888），`lcd_dsi_lane = <4>`                        | 规格书 + 厂家 RK dtsi               |
| 像素时钟        | `lcd_dclk_freq = <62>`（MHz），DSI 下 PLL = dclk×6 = 372 MHz；下限约束见 §2.2                                                      | 计算值（≈60.7 Hz）                  |
| 水平          | `lcd_hbp = <44>`，`lcd_ht = <784>`，`lcd_hspw = <4>`（hfp = 20）                                                             | HBP 40 + HSW 4 + HFP 20，见 §2.1 |
| 垂直          | `lcd_vbp = <27>`，`lcd_vt = <1316>`，`lcd_vspw = <2>`（vfp = 9）                                                             | VBP 25 + VSW 2 + VFP 9，见 §2.1  |
| 背光          | `lcd_pwm_ch = <7>`，`lcd_pwm_freq = <50000>`，`lcd_pwm_pol = <1>`（LED 14 串，typ 30 mA / 19.6 V，板上背光升压驱动）                    | 规格书 + 板级                       |
| 复位          | `lcd_gpio_0 = <&pio PD 21 GPIO_ACTIVE_HIGH>`（面板 RESET，低有效）                                                               | 板级                             |
| 电源/背光使能     | `lcd_gpio_1 = <&pio PD 18 GPIO_ACTIVE_HIGH>`                                                                             | 板级                             |
| 电源名         | `lcd_pin_power = "dcdc1"`、`lcd_pin_power1 = "eldo3"`、`lcd_power = "dc1sw"`                                               | 沿用 SDK 约定                      |
| 触摸          | GT911，TWI1 @ 0x5d，INT PD17 / 唤醒 PD16，`ctp_used = <1>`、`ctp_screen_max_x = <720>`；`ctp1` 为同地址重复节点，已 `status = "disabled"` | 规格书 + SDK dts                  |

说明：K4B 上没有 `lcd_pin_power*` 所示电源，内核会打印 `using dummy regulator`，无副作用。

### 2.1 消隐参数：`lcd_hbp` / `lcd_vbp` 含同步脉宽

SDK 的 dts 头注释（`board-mipi-lcd.dts` 与 A133 的 `lcd-mipi-p055h022.dtsi` 同）：

```
;lcd_hbp:  hsync back porch(pixel) + hsync plus width(pixel)
;lcd_vbp:  vsync back porch(line)  + vysnc plus width(line)
```

本屏规格 HBP 40 / HFP 20 / HSW 4、VBP 25 / VFP 9 / VSW 2，故：

- `lcd_hbp = 40 + 4 = 44`，`lcd_ht = 720 + 44 + 20 = 784`
- `lcd_vbp = 25 + 2 = 27`，`lcd_vt = 1280 + 27 + 9 = 1316`

若按规格书把 HBP/VBP（40/25）原样填入 `lcd_hbp` / `lcd_vbp`，每帧少 4 列 / 2 行消隐，DSI 视频流与面板不匹配，
现象为只有背光、无画面（此时 DCS 回读仍正常，易误判为初始化码问题）。A133 与已量产配置同样按「含同步脉宽」填写。

### 2.2 DSI 像素时钟下限：`lcd_dclk_freq > 54.17 MHz`

T113 的 combo-DPHY PLL 算法（`de/lowlevel_v2x/de_dsi.c: dsi_comb_dphy_pll_set()`，内核与 u-boot 同一份）：

```c
frq = dclk * 24 / lane;                 /* 每 lane 的 Mbps */
for (m = 0; frq_p <= 1300000000;) { m++; frq_p = frq * m; }   /* VCO 必须 > 1.3GHz */
if (m > 4) { printf("pclk is too small err!!!!"); return -1; }  /* 内核里是 WARN(1, ...) */
```

即要求 `dclk × 6 × m > 1300 MHz` 且 `m ≤ 4`，得出 **`lcd_dclk_freq > 54.17 MHz`**。
厂家 RK3568 参考配置给的 54 MHz 在 RK3568 上合法（DSI PLL 算法不同），在 T113 上低于下限：

- u-boot 串口打印 `pclk is too small err!!!!`；
- `dsi_comb_dphy_pll_set()` 直接返回 -1，PLL 寄存器不编程，DSI 时钟错误，面板不亮；
- 此时 DCS 读 ID 仍可能返回数据，不能据此判断链路正常。

本包取 `lcd_dclk_freq = <62>`（frq = 372 Mbps、m = 4、VCO = 1488 MHz，合法）。

## 3. 集成步骤

### 3.1 拷贝驱动文件

| 本包                                        | 复制到 SDK                                                             |
| ----------------------------------------- | ------------------------------------------------------------------- |
| `driver/kernel/P055H022-IPS-CTP.c` / `.h` | `kernel/linux-5.4/drivers/video/fbdev/sunxi/disp2/disp/lcd/`        |
| `driver/uboot/P055H022-IPS-CTP.c` / `.h`  | `brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/lcd/` |

相对于 A133 平台的差异：仅结构体/类型名（T113 使用 `__lcd_panel` / `__lcd_panel_t` 等），以及板级引脚与 PWM 通道
（K4B：reset PD21、power PD18、pwm ch7；A133 板：reset PE5、power PD21、pwm ch2）。

### 3.2 注册（每棵树 3 处）

```sh
cd <SDK>
patch -p1 < patches/0001-kernel-register-panel.patch
patch -p1 < patches/0002-uboot-register-panel.patch
patch -p1 < patches/0003-board-dts.patch      # 板级 dts；沿用现有 dts 时可跳过
```

手动等价于：

- `.../disp2/disp/Makefile`：`disp-y += lcd/P055H022-IPS-CTP.o`
- `.../lcd/panels.c`：`panel_array[]` 中加入 `&p055h022_ips_ctp_panel,`
- `.../lcd/panels.h`：加入 `extern struct __lcd_panel p055h022_ips_ctp_panel;`（u-boot 用 `extern __lcd_panel_t`）

面板驱动为**无条件编入**（不挂 `CONFIG_LCD_SUPPORT_*`），无需修改 Kconfig 或 config；同一 SDK 内可保留多个面板驱动，
切换时只修改 dts 的 `lcd_driver_name` 与时序。

### 3.3 设备树（内核 + u-boot 各一份）

```dts
&disp {
    fb0_width  = <720>;      /* 必须等于面板分辨率 */
    fb0_height = <1280>;
};

&lcd0 {
    lcd_used        = <1>;
    lcd_driver_name = "P055H022-IPS-CTP";
    lcd_if          = <4>;
    lcd_x = <720>;  lcd_y = <1280>;  lcd_dclk_freq = <62>;   /* > 54.17MHz，见 §2.2 */
    lcd_hbp = <44>; lcd_ht = <784>;  lcd_hspw = <4>;         /* hbp/vbp 含同步脉宽，见 §2.1 */
    lcd_vbp = <27>; lcd_vt = <1316>; lcd_vspw = <2>;
    lcd_pwm_ch = <7>; lcd_pwm_freq = <50000>; lcd_pwm_pol = <1>;
    lcd_dsi_if = <0>; lcd_dsi_lane = <4>; lcd_dsi_format = <0>;
    lcd_gpio_0 = <&pio PD 21 GPIO_ACTIVE_HIGH>;   /* 复位 */
    lcd_gpio_1 = <&pio PD 18 GPIO_ACTIVE_HIGH>;   /* 电源/背光使能 */
    pinctrl-0 = <&dsi4lane_pins_a>;
    pinctrl-1 = <&dsi4lane_pins_b>;
};
```

板级 dts 文件：`device/config/chips/t113_s4/configs/evb1_auto/linux-5.4/board-mipi-lcd.dts` 与
`.../evb1_auto/uboot-board-mipi-lcd.dts`（其它 chip/board 目录下同名文件均为软链接，只改一处）。

触摸节点：只启用 `ctp`，并将 `ctp1` 整节 `status = "disabled"` —— 两者为同一 I2C 地址（0x5d）的重复节点，
仅写 `ctp_used = <0>` 不够，i2c 核心仍会注册该节点并打印
`i2c-1: Failed to register i2c client goodix at 0x5d (-16)`。

### 3.4 开机 LOGO

u-boot 打包用的 `bootlogo.bmp`（`device/config/chips/t113/configs/evb1_auto_nand/bootlogo.bmp`）
必须满足 **宽 ≤ fb 宽、高 ≤ fb 高、bpp ∈ {16,24,32}**，否则串口打印
`no support big size bmp[800x480] on fb[720x1280]`，开机不显示 LOGO。本包可直接覆盖：

```sh
cp logo/bootlogo-720x432.bmp <SDK>/device/config/chips/t113/configs/evb1_auto_nand/bootlogo.bmp
```

## 4. 编译与烧写

```sh
cd <SDK>
mkdir -p /tmp/shim && ln -sf /bin/bzip2 /tmp/shim/lbzip2 && export PATH=/tmp/shim:$PATH   # 宿主缺 lbzip2 时
./build.sh lunch     # buildroot / t113 / 256MB / mipi-lcd
./build.sh
```

产物：`out/t113/evb1_auto_nand/buildroot/update-t113-kickpi-k4b-buildroot-mipi-lcd-sdboot-<YYMMDDHH>.img`
（文件名含小时时间戳，取最新文件）。

| 镜像                       | 用途                                         |
| ------------------------ | ------------------------------------------ |
| `...-sdboot-...img`      | 仅用于 **SD 卡启动**（PhoenixCard → SD Boot Card） |
| `...-256MB/512MB-...img` | 烧写 NAND（PhoenixSuit/USB 或 SD 安装卡）          |

将 sdboot 镜像烧入 NAND 会报 `ubi volume total size is larger than mtd size` / `download mbr err`。

### 4.1 构建产物核对

| 检查项                   | 结果                                                                                                                                                                                                                                                |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 全流程                   | `build OK` + `pack finish`，EXIT=0                                                                                                                                                                                                                 |
| 内核符号                  | `nm out/t113/kernel/build/vmlinux \| grep p055h022` → `p055h022_ips_ctp_panel`（编入内核，非模块）                                                                                                                                                          |
| u-boot 目标             | `.../u-boot-2018/.../lcd/P055H022-IPS-CTP.o` 已生成                                                                                                                                                                                                  |
| 内核 dtb                | `dtc -I dtb -O dts out/t113/kernel/build/arch/arm/boot/dts/board-mipi-lcd.dtb` → `lcd_driver_name = "P055H022-IPS-CTP"`、`lcd_hbp = <0x2c>`(44)、`lcd_vbp = <0x1b>`(27)、`lcd_dclk_freq = <0x3e>`(62)、`fb0_width = <0x2d0>`(720)、`lcd_gpio_1 = PD18` |
| u-boot dts（pack 实际使用） | `out/t113/evb1_auto_nand/pack_out/.uboot.dts`，字段同上                                                                                                                                                                                                |
| 镜像                    | `...-256MB-<戳>.img`、`...-sdboot-<戳>.img`                                                                                                                                                                                                          |

## 5. 验证

1. **u-boot LOGO**：上电即显示（最快，u-boot 与内核使用同一份初始化码）。
2. **开机彩条**：把 `CONFIG_DISP2_SUNXI_BOOT_COLORBAR=y` 加入**生成的** `out/t113/kernel/build/.config` 后重编
   （不要修改 tracked 的 `config-5.4`，下次 `lunch` 会覆盖）。出彩条表示 面板/DSI/DE 整条链路通。
3. **雪花**：串口执行 `dd if=/dev/urandom of=/dev/fb0 bs=1M count=4`。
4. **日志**：`dmesg | grep -iE "\[LCD\]|disp|lcd"`。

驱动内置诊断打印（HX8394F 为 Himax IC，除 RDDID 外还读 RDID1/2/3）：

```
[LCD] P055H022-IPS-CTP panel init start
[LCD] RDDID(04) ret=0 num=1 data=83 00 00   ← 有字节返回即表示 DSI 命令链路与面板均有响应
[LCD] RDDPM(0a) ret=0 num=1 data=08        ← 初始化前状态（本屏初始化后为 0x1c）
[LCD] RDDSDR(c) ret=0 num=1 data=??        ← 仅内核版打印；该命令实为 RDCOLMOD(Get Pixel Format)
```

判读：

- `RDDPM` 在初始化前后由 0x08 变为 0x1c，表示 DCS 写通路与初始化码已生效；此后面板不亮则应从视频流
  （模式/时序）排查，不必再修改初始化码。
- `RDMADCTL` 仅供参考：写 0x11 回读 0x01，说明该 IC 的 MADCTL 不保留 ML 位（HX8394F 仅保留自身支持的位），
  不能据此判断写是否生效。
- `RDDST` 的位定义各厂实现不一致，比较**变化**比比较绝对值可靠。

`lcd_clk_config` 打印的 `clk real: ... dclk(93000000) dsi_rate(150000000)` **不是错误**：DSI 下
`pll = dclk × dsi_div(6)`（dclk 62 MHz → PLL 372 MHz，正确），`dsi_rate = 150 MHz` 为 SoC 表固定值，
`dclk_rate_set` 由 `tcon_div` 估算、随后由 `tcon_clk_div_ajust` 补偿（厂商 8"/10" 面板同样打印该行）。

## 6. 常见问题

| 现象                                                                                                   | 原因 / 处理                                                                                                                                                                                              |
| ---------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 只有背光，串口无 `[LCD] panel init start`                                                                    | 驱动名未匹配（`lcd_driver_name` 与 `.name` 必须逐字符相同）或 `lcd_used` 不为 1                                                                                                                                         |
| 有 `[LCD]`，但 ID 回读全 `num=0`                                                                           | DSI 链路或面板供电异常：查 dclk / dsi_rate、PD0–PD9 pinmux、复位脚 PD21 时序、转接板供电（IOVCC 1.8V / VDD 2.8V）                                                                                                              |
| 有 `[LCD]` 且 ID 有数据，面板仍不亮                                                                             | 先核对 `lcd_hbp` / `lcd_vbp` 是否已含 hspw / vspw（本屏应为 44 / 27，见 §2.1）；再看 `RDDPM(0a)` 初始化前后是否变化（本屏 0x08 → 0x1c）；再按时序/模式排查：`lcd_dsi_if` 在 0(video) / 2(burst) 间试，核对 ht / vt / hbp / vbp / hspw / vspw 与 dclk |
| 画面抖动 / 花屏                                                                                            | 微调 `lcd_vbp` / `lcd_vt`（本屏厂家建议 VBP 25 / VFP 9 / VSW 2 → vt 1316，当前取值）                                                                                                                                |
| 开机无 LOGO                                                                                             | LOGO 超出 fb 或 bpp 不符，见 §3.4                                                                                                                                                                           |
| `i2c-1: Failed to register i2c client goodix at 0x5d (-16)` / `of_i2c: Failure registering .../ctp1` | `ctp` 与 `ctp1` 为同一 I2C 地址的重复节点：仅启用 `ctp`，并将 `ctp1` 整节 `status = "disabled"`                                                                                                                          |
| 触摸无反应                                                                                                | 依次检查：CTP 排线、INT / 唤醒脚（PD17 / PD16）、`ctp_screen_max_x`（720）、`goodix,cfg-group0`（可用厂家 RK dtsi 中的配置试）                                                                                                   |
| 黑屏但系统正常启动                                                                                            | 该 rootfs 为 headless（无 fb console、无内核 logo、无 launcher），黑屏不代表驱动未运行，按 §5 验证                                                                                                                             |

## 8. 许可

驱动文件沿用 Allwinner SDK 的 GPL-2.0 头；初始化码来自面板厂提供的资料，仅供本面板适配使用。
