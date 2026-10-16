# P055H022-IPS-CTP 面板驱动（KICKPI K5/K5C · Allwinner A133）

5.5 寸 IPS **720×1280** MIPI-DSI 面板（LCM IC **HX8394F**，4 data lane，video 模式）在 Allwinner A133
（KICKPI K5/K5C，`sun50iw10p1`）Linux SDK 上的驱动与集成补丁。**本面板已在 A133 上实测点亮**
（开机 LOGO 可见），并被移植到 T113 K4B（见上一级目录同名包）。

| 项 | 值 |
|---|---|
| 适用平台 | KICKPI K5/K5C（Allwinner A133） |
| SDK | `kernel/linux-4.9`（`disp2/disp`）+ `brandy/brandy-2.0/u-boot-2018` + ubuntu16.04 rootfs |
| 面板 | 5.5" IPS，720×1280，MIPI-DSI 4 data lane，video 模式，LCM IC HX8394F |
| 触摸 | GT911，`&twi0` @ 0x5D，INT PE3 / 唤醒 PE4，`ctp_used = <1>`、`ctp_screen_max_x = <720>` |
| 驱动名 | `mipi_p055h022`（必须与设备树 `lcd_driver_name` 逐字符一致） |
| 版本 | v1.0（2026-10-16） |

## 1. 文件清单

```
P055H022-IPS-CTP/
├── README.md                                        ← 本文件
├── driver/kernel/mipi_p055h022.c/.h                 ← 内核驱动
├── driver/uboot/mipi_p055h022.c/.h                  ← u-boot 驱动
├── dts/lcd-mipi-p055h022.dtsi                       ← 面板 dtsi（含 GT911 触摸节点）
└── patches/0001..0004                               ← 内核/u-boot 注册、板级 dts、build.sh 列表
```

## 2. 面板参数（= 设备树取值）

| 项 | 值 |
|---|---|
| 接口 | `lcd_if = <4>`（DSI），`lcd_dsi_if = <0>`（video），`lcd_dsi_format = <0>`（RGB888），`lcd_dsi_lane = <4>` |
| 分辨率 / 显示区 | 720×1280；`lcd_width = <68>`，`lcd_height = <120>`（mm） |
| 像素时钟 | `lcd_dclk_freq = <62>`（MHz） |
| 水平 | `lcd_hbp = <44>`（= HBP 40 + HSW 4），`lcd_ht = <784>`，`lcd_hspw = <4>`（hfp 20） |
| 垂直 | `lcd_vbp = <27>`（= VBP 25 + VSW 2），`lcd_vt = <1316>`，`lcd_vspw = <2>`（vfp 9） |
| 背光 | `lcd_pwm_ch = <2>`，`lcd_pwm_freq = <50000>`，`lcd_pwm_pol = <1>` |
| 复位 / 电源使能 | `lcd_gpio_0 = <&pio PE 5 …>`（RESET），`lcd_gpio_1 = <&pio PD 21 …>` |
| 触摸 | GT911：`ctp_used = <1>`，`ctp_screen_max_x = <720>`，`ctp_screen_max_y = <1280>`，INT PE3 / 唤醒 PE4 |

初始化码为厂家码的逐字节实现（17 条寄存器写 + `0x11` → 255 ms → `0x36 = 0x11` → `0x29` → 255 ms），
与 T113 包中同一份表一致；`lcd_hbp`/`lcd_vbp` 含同步脉宽的原因见 P050 包 §2.1。

## 3. 集成与切换

集成步骤、补丁用法、`build.sh` 的 `LCD_DTS` 说明与 P050 包完全相同，见
`../P050H031-V2-IPS/README.md` §3。

**当前 SDK 状态（本次工作结束时）**：板级 dts 默认生效的是 p055，
p050 的 include 以注释形式紧随其后：

```
device/config/chips/a133/configs/c3/kickpi-k5c.dts
  #include "lcd-mipi-p055h022.dtsi"
  //#include "lcd-mipi-p050h031.dtsi"
```

切换面板只改这两行的注释；`build.sh` 的 `LCD_DTS` 数组已同时包含两者。

## 4. 编译与验证

```sh
cd <SDK>
./build.sh
# → out/update-a133-kickpi-k5c-ubuntu16.04-mipi-p055h022-<YYMMDDHH>.img
```

已存在的 p055 整机镜像：`out/update-a133-kickpi-k5c-ubuntu16.04-mipi-p055h022-2025122120.img`。
本次工作另外用 p050 配置完整编过一版并核对产物（见 P050 包 §4.1）；两版镜像共用同一份内核/u-boot
（两个面板的驱动都在 `panel_array[]` 里，最终用哪块屏由 dtsi 决定）。

## 5. 常见问题

与 P050 包 §6 相同（驱动名逐字符匹配、`LCD_DTS` 列表、切换方式）。
触摸侧注意：`ctp` 与 `ctp1` 是同一 I2C 地址的重复节点时只能开一个，
只写 `ctp_used = <0>` 不够，需要把另一个节点整节 `status = "disabled"`。

## 6. 许可

驱动文件沿用 Allwinner SDK 的 GPL-2.0 头；初始化码来自面板厂提供的资料，仅供本面板适配使用。
