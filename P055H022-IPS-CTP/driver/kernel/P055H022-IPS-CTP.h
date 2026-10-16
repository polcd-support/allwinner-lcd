/* P055H022-IPS-CTP.h - sunxi disp2 LCD panel driver header
 *
 * P055H022-IPS-CTP (5.5 inch, 720x1280, IPS, HX8394F, MIPI-DSI) panel driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _P055H022_IPS_CTP_H
#define _P055H022_IPS_CTP_H

#include "panels.h"

extern struct __lcd_panel p055h022_ips_ctp_panel;

extern s32 bsp_disp_get_panel_info(u32 screen_id, struct disp_panel_para *info);

#endif /*End of file*/
