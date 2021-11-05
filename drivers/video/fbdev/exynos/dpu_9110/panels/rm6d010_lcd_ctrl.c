/* rm6d010_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "rm6d010_param.h"
#include "rm6d010_lcd_ctrl.h"
#include "rm6d010_mipi_lcd.h"

#include "../dsim.h"
#include <video/mipi_display.h>

void rm6d010_testkey_enable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm6d010 *lcd = dev_get_drvdata(&panel->dev);

	mutex_lock(&lcd->mipi_lock);
}

void rm6d010_testkey_disable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm6d010 *lcd = dev_get_drvdata(&panel->dev);

	mutex_unlock(&lcd->mipi_lock);
}

int rm6d010_read_mtp_reg(int id, u32 addr, char* buffer, u32 size)
{
	int ret = 0;

	rm6d010_testkey_enable(id);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		addr, size, buffer) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n", __func__, addr);
		ret = -EIO;
	}

	rm6d010_testkey_disable(id);

	return ret;
}

void rm6d010_init_ctrl(int id, struct decon_lcd * lcd)
{
	/* Test Key Enable */
	rm6d010_testkey_enable(id);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_1,
		ARRAY_SIZE(INIT_1)) < 0)
		dsim_err("failed to send INIT_1.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_2,
		ARRAY_SIZE(INIT_2)) < 0)
		dsim_err("failed to send INIT_2.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_3,
		ARRAY_SIZE(INIT_3)) < 0)
		dsim_err("failed to send INIT_3.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_4,
		ARRAY_SIZE(INIT_4)) < 0)
		dsim_err("failed to send INIT_4.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_5,
		ARRAY_SIZE(INIT_5)) < 0)
		dsim_err("failed to send INIT_5.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_6,
		ARRAY_SIZE(INIT_6)) < 0)
		dsim_err("failed to send INIT_6.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_7,
		ARRAY_SIZE(INIT_7)) < 0)
		dsim_err("failed to send INIT_7.\n");

	msleep(120);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) INIT_8,
		ARRAY_SIZE(INIT_8)) < 0)
		dsim_err("failed to send INIT_7.\n");

	/* Test key disable */
	rm6d010_testkey_disable(id);
}

void rm6d010_enable(int id)
{
	return;
}

void rm6d010_disable(int id)
{
	return;
}

int rm6d010_gamma_ctrl(int id, u32 backlightlevel)
{
	return 0;
}

int rm6d010_gamma_update(int id)
{
	return 0;
}

void rm6d010_hlpm_ctrl(struct rm6d010 *lcd, bool enable)
{
	struct dsim_device *dsim = lcd->dsim;
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	if (enable) {
		rm6d010_testkey_enable(dsim->id);
		rm6d010_testkey_disable(dsim->id);

		pr_info("%s:on\n", "hlpm_ctrl");
	} else {
		rm6d010_testkey_enable(dsim->id);
		rm6d010_testkey_disable(dsim->id);

		pr_info("%s:off:br[%d]\n", "hlpm_ctrl", brightness);
	}

	return;
}

int rm6d010_hbm_on(int id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *lcd = dsim->ld;
	struct rm6d010 *panel = dev_get_drvdata(&lcd->dev);

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 0;
	}

	rm6d010_testkey_enable(id);

	if (panel->hbm_on) {
		printk("[LCD] %s : HBM ON\n", __func__);
	} else{
		printk("[LCD] %s : HBM OFF\n", __func__);
	}
	rm6d010_testkey_disable(id);

	return 0;
}
