# P050H031-V2-IPS 面板驱动（KICKPI K4B / Allwinner T113-S3）

5.0 寸 IPS **720×1280** MIPI-DSI 面板的显示驱动与集成补丁（LCM IC：**ST7703**，4 data lane，video 模式）。

| 项      | 值                                                                                                                |
| ------ | ---------------------------------------------------------------------------------------------------------------- |
| 适用平台   | KICKPI K4B（Allwinner T113-S3）；Linux SDK：`kernel/linux-5.4` + `brandy/brandy-2.0/u-boot-2018` + buildroot 2019.02 |
| 面板     | 5.0" IPS，720×1280，MIPI-DSI 4 data lane，video 模式，LCM IC ST7703                                                    |
| 触摸     | 本包设备树默认关闭（`ctp_used = <0>`）；配触摸屏时按 §3.3 打开                                                                       |
| 驱动名    | `P050H031-V2-IPS`（**必须与设备树 `lcd_driver_name` 逐字符一致**，匹配用 `strcmp`，无兜底）                                           |
| 初始化码来源 | 面板厂原始文件 `5.0HSD +ST7703 MIPI 4L 20240312.txt`，逐字节照搬（见 §7）                                                        |
| 驱动文件   | 内核与 u-boot 各一份（u-boot 那份用于开机 LOGO），两份初始化表一致                                                                      |
| 版本     | v1.0（2026-10-16）                                                                                                 |

## 1. 文件清单

```
P050H031-V2-IPS/
├── README.md                              ← 本文件
├── P050H031-V2-IPS-CTP.pdf                ← 面板规格书（厂家原件）
├── 5.0HSD +ST7703 MIPI 4L 20240312.txt    ← 厂家初始化码（原始文件）
├── driver/
│   ├── kernel/P050H031-V2-IPS.c/.h        ← 内核驱动
│   └── uboot/P050H031-V2-IPS.c/.h         ← u-boot 驱动
├── dts/
│   ├── board-mipi-lcd.dts                 ← 内核板级 dts（&disp / &lcd0 / 触摸节点）
│   └── uboot-board-mipi-lcd.dts           ← u-boot 板级 dts
├── patches/
│   ├── 0001-kernel-register-panel.patch   ← 内核注册（Makefile + panels.c + panels.h，各 1 行）
│   ├── 0002-uboot-register-panel.patch    ← u-boot 注册（同上）
│   └── 0003-board-dts.patch               ← 板级 dts 相对 SDK 原始文件的改动
├── logo/bootlogo-720x432.bmp              ← 720×1280 fb 用开机 LOGO（24bpp）
└── tools/init_table.py                    ← 厂家初始化码 ↔ 驱动 C 表 转换/校验脚本
```

## 2. 面板参数（= 设备树取值）

| 项       | 值                                                                                                 |
| ------- | ------------------------------------------------------------------------------------------------- |
| 接口      | `lcd_if = <4>`（DSI），`lcd_dsi_if = <0>`（video），`lcd_dsi_format = <0>`（RGB888），`lcd_dsi_lane = <4>` |
| 分辨率     | 720×1280                                                                                          |
| 像素时钟    | `lcd_dclk_freq = <62>`（MHz）；DSI 下 PLL = dclk×6 = 372 MHz                                          |
| 水平      | `lcd_hbp = <34>`，`lcd_ht = <784>`，`lcd_hspw = <4>`                                                |
| 垂直      | `lcd_vbp = <16>`，`lcd_vt = <1316>`，`lcd_vspw = <2>`                                               |
| 背光      | `lcd_pwm_ch = <7>`，`lcd_pwm_freq = <50000>`，`lcd_pwm_pol = <1>`                                   |
| 复位      | `lcd_gpio_0 = <&pio PD 21 GPIO_ACTIVE_HIGH>`                                                      |
| 电源/背光使能 | `lcd_gpio_1 = <&pio PD 18 GPIO_ACTIVE_HIGH>`                                                      |
| 电源名     | `lcd_pin_power = "dcdc1"`、`lcd_pin_power1 = "eldo3"`、`lcd_power = "dc1sw"`                        |

说明：

- K4B 上没有上表三路电源，内核会打印 `using dummy regulator`，无副作用。
- Allwinner 的 `lcd_hbp` / `lcd_vbp` **含**同步脉宽（dts 头注释：`hsync back porch + hsync plus width`），本屏取值即 `hbp = 30 + 4`、`vbp = 14 + 2`。
- T113 的 DSI 合并 PLL 要求 `lcd_dclk_freq > 54.17 MHz`（算法见 `P055H022-IPS-CTP` 包说明 §2.2），本屏取 62 MHz。
- 改分辨率时需同步 `&disp` 的 `fb0_width` / `fb0_height`，并按 §3.4 处理开机 LOGO。

## 3. 集成步骤

### 3.1 拷贝驱动文件

| 本包                                       | 复制到 SDK                                                             |
| ---------------------------------------- | ------------------------------------------------------------------- |
| `driver/kernel/P050H031-V2-IPS.c` / `.h` | `kernel/linux-5.4/drivers/video/fbdev/sunxi/disp2/disp/lcd/`        |
| `driver/uboot/P050H031-V2-IPS.c` / `.h`  | `brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/lcd/` |

### 3.2 注册（每棵树 3 处）

```sh
cd <SDK>
patch -p1 < patches/0001-kernel-register-panel.patch
patch -p1 < patches/0002-uboot-register-panel.patch
patch -p1 < patches/0003-board-dts.patch      # 板级 dts；沿用现有 dts 时可跳过
```

手动等价于：

- `.../disp2/disp/Makefile`：`disp-y += lcd/P050H031-V2-IPS.o`
- `.../lcd/panels.c`：`panel_array[]` 中加入 `&p050h031_v2_ips_panel,`
- `.../lcd/panels.h`：加入 `extern struct __lcd_panel p050h031_v2_ips_panel;`（u-boot 用 `extern __lcd_panel_t`）

面板驱动为**无条件编入**（不挂 `CONFIG_LCD_SUPPORT_*`），无需修改 Kconfig 或 config。

### 3.3 设备树（内核 + u-boot 各一份）

```dts
&disp {
    fb0_width  = <720>;      /* 必须等于面板分辨率 */
    fb0_height = <1280>;
};

&lcd0 {
    lcd_used        = <1>;
    lcd_driver_name = "P050H031-V2-IPS";
    lcd_if          = <4>;
    /* x/y/dclk/hbp/ht/hspw/vbp/vt/vspw/pwm/dsi/gpio 按 §2 填写 */
    lcd_gpio_0 = <&pio PD 21 GPIO_ACTIVE_HIGH>;   /* 复位 */
    lcd_gpio_1 = <&pio PD 18 GPIO_ACTIVE_HIGH>;   /* 电源/背光使能 */
    pinctrl-0 = <&dsi4lane_pins_a>;
    pinctrl-1 = <&dsi4lane_pins_b>;
};

/* 不使用触摸时两个节点都关闭：ctp_used = <0>（ctp 与 ctp1 两处）；
   启用触摸时：ctp 节点 ctp_used = <1>、ctp_screen_max_x = <720>，并将 ctp1 整节 status = "disabled" */
ctp_used = <0>;
```

板级 dts 文件：`device/config/chips/t113_s4/configs/evb1_auto/linux-5.4/board-mipi-lcd.dts` 与
`.../evb1_auto/uboot-board-mipi-lcd.dts`（其它 chip/board 目录下同名文件均为软链接，只改一处）。

### 3.4 开机 LOGO

u-boot 打包用的 `bootlogo.bmp`（`device/config/chips/t113/configs/evb1_auto_nand/bootlogo.bmp`）
必须满足 **宽 ≤ fb 宽、高 ≤ fb 高、bpp ∈ {16,24,32}**，否则串口打印
`no support big size bmp[800x480] on fb[720x1280]` 与 `show bmp on fb failed !-1`，开机不显示 LOGO。
本包 `logo/bootlogo-720x432.bmp` 可直接覆盖：

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

## 5. 验证

1. **u-boot LOGO**：上电即显示（最快的通路判据）。
2. **开机彩条**：把 `CONFIG_DISP2_SUNXI_BOOT_COLORBAR=y` 加入**生成的** `out/t113/kernel/build/.config` 后重编
   （不要修改 tracked 的 `config-5.4`，下次 `lunch` 会覆盖）。出彩条表示 面板/DSI/DE 整条链路通。
3. **雪花**：串口执行 `dd if=/dev/urandom of=/dev/fb0 bs=1M count=4`。
4. **日志**：`dmesg | grep -iE "\[LCD\]|disp|lcd"`。

驱动内置诊断打印（内核与 u-boot 各一份）：

```
[LCD] P050H031-V2-IPS panel init start
[LCD] RDDID(04) num=3 data=xx xx xx     ← 有字节返回即表示 DSI 命令链路与面板均有响应
[LCD] RDDPM(0a) num=1 data=xx
```

`lcd_clk_config` 打印的 `clk real: ... dclk(93000000) dsi_rate(150000000)` **不是错误**：DSI 下
`pll = dclk × dsi_div(6)`（PLL 372 MHz 正确），`dsi_rate=150MHz` 为 SoC 表固定值，`dclk_rate_set` 由
`tcon_div` 估算、随后由 `tcon_clk_div_ajust` 补偿（厂商 8"/10" 面板同样打印该行）。

## 6. 常见问题

| 现象                                                      | 原因 / 处理                                                                                                                                      |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| 只有背光，串口无 `[LCD] panel init start`                       | 驱动名未匹配（`lcd_driver_name` 与 `.name` 必须逐字符相同）或 `lcd_used` 不为 1                                                                                 |
| 有 `[LCD]`，但 ID 回读全 `num=0`                              | DSI 链路或面板供电异常：查 dclk / dsi_rate、PD0–PD9 pinmux、复位脚 PD21 时序、转接板供电                                                                             |
| 有 `[LCD]` 且 ID 有数据，面板仍不亮                                | 初始化码/时序不匹配：必须使用面板厂提供的初始化码（同 IC 不同屏的 B2/B6/B8/B3/C0/C1/E3/E9/EA、E0 gamma 均不同）；`lcd_dsi_if` 可在 0(video)/2(burst) 间试；u-boot 侧驱动不得做 DCS 回读（§6.1） |
| `no support big size bmp` / `show bmp on fb failed !-1` | LOGO 超出 fb 或 bpp 不符，见 §3.4                                                                                                                   |
| 串口持续打印 `GTP ... failed`                                 | 触摸节点未关闭：`ctp_used = <0>`（`ctp` 与 `ctp1` 两处）                                                                                                  |
| 黑屏但系统正常启动                                               | 该 rootfs 为 headless（无 fb console、无内核 logo、无 launcher），黑屏不代表驱动未运行，按 §5 验证                                                                     |

## 7. 初始化码与校验

驱动中的 `lcm_initialization_setting[]` 与面板厂文件 `5.0HSD +ST7703 MIPI 4L 20240312.txt` 逐字节一致：

```sh
python3 tools/init_table.py verify "5.0HSD +ST7703 MIPI 4L 20240312.txt" \
        <SDK>/kernel/linux-5.4/drivers/video/fbdev/sunxi/disp2/disp/lcd/P050H031-V2-IPS.c
# -> 逐条一致

# 更换面板时：将厂家码转换为 C 表
python3 tools/init_table.py gen "新的厂家码.txt"
# Allwinner 风格 {type,delay,len,payload} 表加 --aw
```

## 8. 许可

驱动文件沿用 Allwinner SDK 的 GPL-2.0 头（保留原声明）；初始化码来自面板厂提供的资料，仅供本面板适配使用。
