/* drivers/video/fbdev/exynos/dpu_9110/panels/rm6d010_lcd_ctrl.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Haowe Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __RM6D010_LCD_CTRL_H__
#define __RM6D010_LCD_CTRL_H__

#include "decon_lcd.h"
#include "mdnie_lite.h"

void rm6d010_init_ctrl(int id, struct decon_lcd *lcd);
void rm6d010_enable(int id);
void rm6d010_disable(int id);
int rm6d010_gamma_ctrl(int id, unsigned int backlightlevel);
int rm6d010_gamma_update(int id);

#endif /*__RM6D010_LCD_CTRL_H__*/
