/* drivers/video/fbdev/sunxi/disp2/disp/lcd/mipi_p050h031.c
 *
 * Copyright (c) 2017 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * mipi_p050h031 panel driver  (P050H031-V2-IPS)
 * 5.0 inch IPS, 720x1280, MIPI-DSI, 4 lane, LCM IC ST7703.
 *
 * Init table = vendor code "5.0HSD +ST7703 MIPI 4L 20240312.txt",
 * byte-exact: 17 register writes + 0x11 (sleep out, 250 ms) +
 * 0x29 (display on, 50 ms).  Table layout is the Allwinner panel
 * style {type, delay_ms, len, cmd, data...}; delay follows the write.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "mipi_p050h031.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init1(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define power_en(sel, val) sunxi_lcd_gpio_set_value(sel, 1, val)

static void lcd_cfg_panel_info(struct panel_extend_para *info)
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
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1])
				 * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16)
				+ (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8)
				   + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 50);
	LCD_OPEN_FUNC(sel, lcd_panel_init1, 50);
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

#define ROWS 19
#define COLS 68

static u8 data[ROWS][COLS] = {
	{0x39,0x00,0x04,0xB9,0xF1,0x12,0x83},
	{0x39,0x00,0x1C,0xBA,0x33,0x81,0x05,0xF9,0x0E,0x0E,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x25,0x00,0x91,0x0A,0x00,0x00,0x02,0x4F,0xD1,0x00,0x00,0x37},
	{0x39,0x00,0x03,0xB8,0x25,0x22},
	{0x39,0x00,0x04,0xBF,0x02,0x10,0x00},
	{0x39,0x00,0x0B,0xB3,0x07,0x0B,0x1E,0x1E,0x03,0xFF,0x00,0x00,0x00,0x00},
	{0x39,0x00,0x0A,0xC0,0x73,0x73,0x50,0x50,0x00,0x00,0x08,0x70,0x00},
	{0x39,0x00,0x02,0xBC,0x46},
	{0x39,0x00,0x02,0xCC,0x0B},
	{0x39,0x00,0x02,0xB4,0x80},
	{0x39,0x00,0x04,0xB2,0xC8,0x02,0x30},
	{0x39,0x00,0x0F,0xE3,0x07,0x07,0x0B,0x0B,0x03,0x0B,0x00,0x00,0x00,0x00,0xFF,0x80,0xC0,0x14},
	{0x39,0x00,0x07,0xC1,0x53,0x00,0x1E,0x1E,0x77,0xF1},
	{0x39,0x00,0x03,0xB5,0x09,0x09},
	{0x39,0x00,0x03,0xB6,0xB2,0xB2},
	{0x39,0x00,0x40,0xE9,0xC2,0x10,0x09,0x00,0x00,0x08,0xEA,0x12,0x30,0x00,0x27,0x85,0x08,0xEA,0x27,0x18,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0xF8,0xBA,0x46,0x02,0x08,0x28,0x88,0x88,0x88,0x88,0x88,0xF8,0xBA,0x57,0x13,0x18,0x38,0x88,0x88,0x88,0x88,0x88,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x39,0x00,0x3E,0xEA,0x07,0x12,0x01,0x01,0x02,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x8F,0xBA,0x31,0x75,0x38,0x18,0x88,0x88,0x88,0x88,0x88,0x8F,0xBA,0x20,0x64,0x28,0x08,0x88,0x88,0x88,0x88,0x88,0x23,0x10,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x39,0x00,0x23,0xE0,0x00,0x05,0x09,0x15,0x0F,0x3F,0x2F,0x2D,0x05,0x0A,0x0C,0x11,0x13,0x10,0x10,0x1C,0x1F,0x00,0x03,0x03,0x17,0x11,0x3F,0x2F,0x2D,0x05,0x08,0x0E,0x0F,0x13,0x12,0x16,0x18,0x1D},
	{0x39,0xFA,0x02,0x11,0x00},
	{0x39,0x32,0x02,0x29,0x00},
};
static void lcd_panel_init1(u32 sel)
{
    int i;
    u8 command; 
    u8 *para;
    u32 para_num;
    u8 *tmp;
	printk(KERN_ERR"--kernel!! mipi p050h031 init\n");
	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(100);
    for(i = 0; i < ROWS; i++) {
        tmp = data[i];
        if (tmp[0] == 0x05) {
            sunxi_lcd_dsi_dcs_write_0para(sel, tmp[3]);
        } else {
            command = tmp[3];
            para_num = tmp[2] - 1;
            para = tmp + 4;
            sunxi_lcd_dsi_dcs_write(sel, command, para, para_num);
        }
        if (tmp[1] != 0x0)
            sunxi_lcd_delay_ms(tmp[1]);
    }
	
}


static void lcd_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(1);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t mipi_p050h031_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "mipi_p050h031",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
			.cfg_open_flow = lcd_open_flow,
			.cfg_close_flow = lcd_close_flow,
			.lcd_user_defined_func = lcd_user_defined_func,
	},
};