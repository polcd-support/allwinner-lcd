/* drivers/video/fbdev/sunxi/disp2/disp/lcd/P050H031-V2-IPS.h
 *
 * P050H031-V2-IPS (5.0 inch, 720x1280, IPS, MIPI-DSI) panel driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _P050H031_V2_IPS_H
#define _P050H031_V2_IPS_H

#include "panels.h"

extern struct __lcd_panel p050h031_v2_ips_panel;

extern s32 bsp_disp_get_panel_info(u32 screen_id, struct disp_panel_para *info);

#endif /*End of file*/
