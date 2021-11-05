/* drivers/video/fbdev/exynos/dpu_9110/panels/rm69330_lcd_ctrl.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __RM69330_LCD_CTRL_H__
#define __RM69330_LCD_CTRL_H__

#include "decon_lcd.h"
#include "rm69330_mipi_lcd.h"

int rm69330_read_reg(int id, u32 addr, char* buffer, u32 size);
int rm69330_print_debug_reg(struct rm69330 *lcd);
void rm69330_init_ctrl(int id, struct decon_lcd *lcd);
void rm69330_enable(int id);
void rm69330_disable(int id);
int rm69330_br_level_ctrl(int id, u8 level);
int rm69330_gamma_ctrl(int id, u8 backlightlevel);
int rm69330_lpm_on(struct rm69330 *lcd);
int rm69330_lpm_off(struct rm69330 *lcd);
int rm69330_hbm_on(struct rm69330 *lcd);
int rm69330_hbm_off(struct rm69330 *lcd);

#endif /*__rm69330_LCD_CTRL_H__*/
