/* P055H022-IPS-CTP.c - sunxi disp2 LCD panel driver
 *   kernel: drivers/video/fbdev/sunxi/disp2/disp/lcd/P055H022-IPS-CTP.c
 *   u-boot: drivers/video/sunxi/disp2/disp/lcd/P055H022-IPS-CTP.c
 *   （内核版与 u-boot 版只差类型名：struct panel_extend_para/struct __lcd_panel
 *     vs panel_extend_para/__lcd_panel_t，其余保持同步）
 *
 * P055H022-IPS-CTP panel driver
 * 5.5 inch IPS, 720x1280, LCM IC HX8394F, MIPI-DSI, 4 lane, video mode.
 *
 * Ported from the A133 (kickpi-k5c) driver under which this panel lights:
 *   A133 kernel/linux-4.9 .../disp2/disp/lcd/mipi_p055h022.c
 *     -> init table, lcd_power_on/off sequence, open/close flow timing
 *   A133 arch/arm64/boot/dts/sunxi/lcd-mipi-p055h022.dtsi
 *     -> dts timing: dclk 62, ht 784 / hbp 44 / hspw 4, vt 1316 / vbp 27 / vspw 2
 * Init table is byte-identical to the vendor txt
 * "新_HSD055BHW6-C11HX8394F+HSD5.45.txt" (17 writes) + 0x11 / 0x36=0x11 / 0x29.
 * Every entry is {register, data count, {data...}}, the register is
 * transfered as a MIPI DCS (command 2) write.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "P055H022-IPS-CTP.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define power_en(sel, val) sunxi_lcd_gpio_set_value(sel, 1, val)

static void lcd_cfg_panel_info(panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
	{
		{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		},
		{
		{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i+1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i+1][1] - lcd_gamma_tbl[i][1])
				* j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
							(value<<16)
							+ (value<<8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items-1][1]<<16) +
					(lcd_gamma_tbl[items-1][1]<<8)
					+ lcd_gamma_tbl[items-1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 50);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 50);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);

	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 1);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 0);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	/* A133 (working) sequence: keep reset low while power comes up,
	 * then one reset pulse */
	panel_reset(sel, 0);
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_power_enable(sel, 1);

	power_en(sel, 1);
	sunxi_lcd_delay_ms(200);

	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(20);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(120);
}

static void lcd_power_off(u32 sel)
{
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(1);
	power_en(sel, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_power_disable(sel, 1);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_power_disable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 0);
}

static void lcd_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

struct LCM_setting_table {
	u8 cmd;
	u32 count;
	u8 para_list[64];
};

#define REGFLAG_DELAY         0x00
#define REGFLAG_END_OF_TABLE  0xFE   /* END OF REGISTERS MARKER */

static struct LCM_setting_table lcm_initialization_setting[] = {
	/* 厂家给的面板时序: Vsync=2  VBP=16  VFP=9  H=45 */
	/* CMD2 BKx enable */
	{0xB9, 3, {0xFF, 0x83, 0x94} },
	{0xB1, 10, {0x48, 0x11, 0x71, 0x09, 0x32, 0x34, 0x71, 0x51, 0x55, 0x30} },
	{0xBA, 6, {0x63, 0x03, 0x68, 0x6B, 0xB2, 0xC0} },
	{0xB2, 5, {0x00, 0x80, 0x78, 0x0C, 0x07} },
	/* D3: 2C/NW/IS/GS/SS/SB/BP/B4/B3/DF2/DF1/PB */
	{0xB4, 21, {0x0E, 0x63, 0x0E, 0x63, 0x0E, 0x63, 0x01, 0x0C, 0x7C, 0x55, 0x00, 0x3F, 0x0E, 0x6B, 0x0E, 0x6B, 0x0E, 0x6B, 0x01, 0x0C, 0x7C} },
	{0xD3, 33, {0x00, 0x00, 0x00, 0x00, 0x3C, 0x1C, 0x00, 0x00, 0x32, 0x10, 0x09, 0x00, 0x09, 0x32, 0x15, 0xAD, 0x05, 0xAD, 0x32, 0x00, 0x00, 0x00, 0x00, 0x37, 0x03, 0x0B, 0x0B, 0x37, 0x00, 0x00, 0x00, 0x0C, 0x40} },
	/* GIP: L1/R1/L2/R2 inversion */
	{0xD5, 44, {0x19, 0x19, 0x18, 0x18, 0x1B, 0x1B, 0x1A, 0x1A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x20, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x24, 0x25, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	{0xD6, 44, {0x18, 0x18, 0x19, 0x19, 0x1B, 0x1B, 0x1A, 0x1A, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x25, 0x24, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	/* gamma */
	{0xE0, 58, {0x00, 0x02, 0x07, 0x0D, 0x0D, 0x10, 0x12, 0x10, 0x22, 0x32, 0x42, 0x41, 0x4B, 0x60, 0x66, 0x6D, 0x7B, 0x7F, 0x7C, 0x8C, 0x9A, 0x4C, 0x49, 0x4B, 0x4D, 0x4E, 0x6D, 0x7F, 0x7F, 0x00, 0x02, 0x07, 0x0C, 0x0C, 0x0F, 0x11, 0x10, 0x22, 0x32, 0x42, 0x41, 0x4B, 0x5F, 0x65, 0x6C, 0x7A, 0x7F, 0x7C, 0x8C, 0x9A, 0x4C, 0x49, 0x4B, 0x4D, 0x4E, 0x6D, 0x7F, 0x7F} },
	{0xCC, 1, {0x03} },
	{0xC0, 2, {0x1F, 0x31} },
	{0xB6, 2, {0x75, 0x75} },
	{0xD4, 1, {0x02} },
	{0xBD, 1, {0x01} },
	{0xB1, 1, {0x00} },
	{0xBD, 1, {0x00} },
	{0xC6, 1, {0xED} },

	{0x11, 1, {0x00} },	/* sleep out */
	{REGFLAG_DELAY, 255, {} },
	{0x36, 1, {0x11} },	/* MADCTL */
	{0x29, 1, {0x00} },	/* display on */
	{REGFLAG_DELAY, 255, {} },
	{REGFLAG_END_OF_TABLE, 0, {} },
};

static void lcd_panel_init(u32 sel)
{
	u32 i;
	disp_panel_para pinfo;

	/* 打印驱动实际拿到的参数（用来确认烧进去的 dts 是哪一版） */
	memset(&pinfo, 0, sizeof(pinfo));
	if (bsp_disp_get_panel_info(sel, &pinfo) == 0)
		printk(KERN_ERR "[LCD] panel para: if=%u dsi_if=%u lane=%u fmt=%u eotp=%u dclk=%u %ux%u ht=%u hbp=%u hspw=%u vt=%u vbp=%u vspw=%u\n",
		       (unsigned int)pinfo.lcd_if, (unsigned int)pinfo.lcd_dsi_if,
		       (unsigned int)pinfo.lcd_dsi_lane, (unsigned int)pinfo.lcd_dsi_format,
		       (unsigned int)pinfo.lcd_dsi_eotp, (unsigned int)pinfo.lcd_dclk_freq,
		       (unsigned int)pinfo.lcd_x, (unsigned int)pinfo.lcd_y,
		       (unsigned int)pinfo.lcd_ht, (unsigned int)pinfo.lcd_hbp,
		       (unsigned int)pinfo.lcd_hspw, (unsigned int)pinfo.lcd_vt,
		       (unsigned int)pinfo.lcd_vbp, (unsigned int)pinfo.lcd_vspw);
	else
		printk(KERN_ERR "[LCD] panel para: bsp_disp_get_panel_info(%u) failed\n", sel);

	printk(KERN_ERR "[LCD] P055H022-IPS-CTP panel init start\n");
	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(100);

	for (i = 0; lcm_initialization_setting[i].cmd !=
			REGFLAG_END_OF_TABLE; i++) {
		if (lcm_initialization_setting[i].cmd == REGFLAG_DELAY)
			sunxi_lcd_delay_ms(lcm_initialization_setting[i].count);
		else
			sunxi_lcd_dsi_dcs_write(sel,
					lcm_initialization_setting[i].cmd,
					lcm_initialization_setting[i].para_list,
					lcm_initialization_setting[i].count);
	}
}

static void lcd_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(5);
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t p055h022_ips_ctp_panel = {
	/* panel driver name, must match lcd_driver_name of the lcd node */
	.name = "P055H022-IPS-CTP",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
	},
};
