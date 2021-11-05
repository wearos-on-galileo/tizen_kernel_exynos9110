/* rm69330_lcd_ctrl.c
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

#include "rm69330_param.h"
#include "rm69330_lcd_ctrl.h"
#include "rm69330_mipi_lcd.h"

#include "../dsim.h"
#include <video/mipi_display.h>

int rm69330_read_reg(int id, u32 addr, char* buffer, u32 size)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&panel->dev);
	int ret = 0;

	mutex_lock(&lcd->mipi_lock);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		addr, size, buffer) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n", __func__, addr);
		ret = -EIO;
	}

	mutex_unlock(&lcd->mipi_lock);

	return ret;
}

int rm69330_print_debug_reg(struct rm69330 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	int ret;

	const char panel_state_reg = 0x0a;
	char read_buf[1] = {0, };

	ret = rm69330_read_reg(dsim->id, panel_state_reg, &read_buf[0], ARRAY_SIZE(read_buf));
	if (ret) {
		pr_err("%s:failed to read 0x%02x reg[%d]\n", __func__, panel_state_reg, ret);
		return ret;
	}

	pr_info("%s:0x%02x[0x%02x]\n", __func__, panel_state_reg, read_buf[0]);

	return 0;
}

void rm69330_init_ctrl(int id, struct decon_lcd * lcd)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *ddata = dev_get_drvdata(&panel->dev);

	mutex_lock(&ddata->mipi_lock);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_9,
		ARRAY_SIZE(RM69330_INIT_9)) < 0)
		dsim_err("failed to send RM69330_INIT_9\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_17,
		ARRAY_SIZE(RM69330_INIT_17)) < 0)
		dsim_err("failed to send RM69330_INIT_17\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_1,
		ARRAY_SIZE(RM69330_INIT_1)) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_2,
		ARRAY_SIZE(RM69330_INIT_2)) < 0)
		dsim_err("failed to send RM69330_INIT_2\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_3,
		ARRAY_SIZE(RM69330_INIT_3)) < 0)
		dsim_err("failed to send RM69330_INIT_3\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_4,
		ARRAY_SIZE(RM69330_INIT_4)) < 0)
		dsim_err("failed to send RM69330_INIT_4\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_5,
		ARRAY_SIZE(RM69330_INIT_5)) < 0)
		dsim_err("failed to send RM69330_INIT_5\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_6,
		ARRAY_SIZE(RM69330_INIT_6)) < 0)
		dsim_err("failed to send RM69330_INIT_6\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_DISPLAY_CTRL,
		ARRAY_SIZE(RM69330_DISPLAY_CTRL)) < 0)
		dsim_err("failed to send RM69330_DISPLAY_CTRL\n");

	msleep(60);

	mutex_unlock(&ddata->mipi_lock);
}

void rm69330_enable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_DISP_ON,
		ARRAY_SIZE(RM69330_DISP_ON)) < 0)
		dsim_err("failed to send RM69330_DISP_ON\n");

	return;
}

void rm69330_disable(int id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&panel->dev);

	mutex_lock(&lcd->mipi_lock);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_1,
		ARRAY_SIZE(RM69330_INIT_1)) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_DISP_OFF,
		ARRAY_SIZE(RM69330_DISP_OFF)) < 0)
		dsim_err("failed to send RM69330_DISP_OFF\n");

	msleep(120);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_7,
		ARRAY_SIZE(RM69330_INIT_7)) < 0)
		dsim_err("failed to send RM69330_INIT_7\n");

	msleep(100);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_1,
		ARRAY_SIZE(RM69330_INIT_1)) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_8,
		ARRAY_SIZE(RM69330_INIT_8)) < 0)
		dsim_err("failed to send RM69330_INIT_8\n");

	mutex_unlock(&lcd->mipi_lock);

	msleep(90);

	return;
}

int rm69330_br_level_ctrl(int id, u8 level)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&panel->dev);
	unsigned char gamma_cmd[3] = {0x51, 0x00, 0x00};

	mutex_lock(&lcd->mipi_lock);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_1,
		ARRAY_SIZE(RM69330_INIT_1)) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	gamma_cmd[1] = level;
	lcd->br_level = gamma_cmd[1];

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) gamma_cmd,
		ARRAY_SIZE(gamma_cmd)) < 0)
		dsim_err("failed to send gamma_cmd\n");

	mutex_unlock(&lcd->mipi_lock);

	return 0;
}

static void rm69330_write_gamma(struct rm69330 *lcd, u8 backlightlevel)
{
	struct dsim_device *dsim = lcd->dsim;
	unsigned char gamma_cmd[3] = {0x51, 0x00, 0x00};

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) RM69330_INIT_1,
		ARRAY_SIZE(RM69330_INIT_1)) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	gamma_cmd[1] = rm69330_br_map[backlightlevel];
	lcd->br_level = gamma_cmd[1];

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) gamma_cmd,
		ARRAY_SIZE(gamma_cmd)) < 0)
		dsim_err("failed to send gamma_cmd\n");
}

int rm69330_gamma_ctrl(int id, u8 backlightlevel)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&panel->dev);

	mutex_lock(&lcd->mipi_lock);

	rm69330_write_gamma(lcd, backlightlevel);

	mutex_unlock(&lcd->mipi_lock);

	return 0;
}

extern unsigned int system_rev;
int rm69330_lpm_on(struct rm69330 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	unsigned char RM69330_LPM_LEVEL[2] = {0x51, 0xff};

	mutex_lock(&lcd->mipi_lock);

	if (system_rev > 2) {
		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
			dsim_err("failed to send RM69330_INIT_1\n");

		if (lcd->lpm_level != LPM_DEFAULT_LEVEL)
			RM69330_LPM_LEVEL[1] = lcd->lpm_level;

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_LPM_LEVEL[0], RM69330_LPM_LEVEL[1]) < 0)
			dsim_err("failed to send RM69330_LPM_LEVEL\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
			RM69330_LPM_ON[0], 0) < 0)
			dsim_err("failed to send RM69330_LPM_ON\n");

		msleep(50);

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_9[0], RM69330_INIT_9[1]) < 0)
			dsim_err("failed to send RM69330_INIT_9\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_10[0], RM69330_INIT_10[1]) < 0)
			dsim_err("failed to send RM69330_INIT_10\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
			dsim_err("failed to send RM69330_INIT_1\n");
	} else {
		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_9[0], RM69330_INIT_9[1]) < 0)
			dsim_err("failed to send RM69330_INIT_9\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_12[0], RM69330_INIT_12[1]) < 0)
			dsim_err("failed to send RM69330_INIT_12\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_13[0], RM69330_INIT_13[1]) < 0)
			dsim_err("failed to send RM69330_INIT_13\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_14[0], RM69330_INIT_14[1]) < 0)
			dsim_err("failed to send RM69330_INIT_14\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_15[0], RM69330_INIT_15[1]) < 0)
			dsim_err("failed to send RM69330_INIT_15\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
			dsim_err("failed to send RM69330_INIT_1\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_16[0], RM69330_INIT_16[1]) < 0)
			dsim_err("failed to send RM69330_INIT_16\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
			RM69330_LPM_ON[0], 0) < 0)
			dsim_err("failed to send RM69330_LPM_ON\n");
	}

	mutex_unlock(&lcd->mipi_lock);

	dqa_data->aod_stime = ktime_get_boottime();

	pr_info("%s\n", __func__);

	return 0;
}

int rm69330_lpm_off(struct rm69330 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct backlight_device *bd = lcd->bd;
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int brightness = bd->props.brightness;
	ktime_t now_time;

	mutex_lock(&lcd->mipi_lock);

	if (system_rev > 2) {
		rm69330_write_gamma(lcd, brightness);

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_9[0], RM69330_INIT_9[1]) < 0)
			dsim_err("failed to send RM69330_INIT_9\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_11[0], RM69330_INIT_11[1]) < 0)
			dsim_err("failed to send RM69330_INIT_11\n");

		msleep(50);

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
			dsim_err("failed to send RM69330_INIT_1\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
			RM69330_LPM_OFF[0], 0) < 0)
			dsim_err("failed to send RM69330_LPM_ON\n");
	} else {
		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
			dsim_err("failed to send RM69330_INIT_1\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
			RM69330_LPM_OFF[0], 0) < 0)
			dsim_err("failed to send RM69330_LPM_OFF\n");
	}

	mutex_unlock(&lcd->mipi_lock);

	now_time = ktime_get_boottime();
	dqa_data->aod_high_time += ((unsigned int)ktime_ms_delta(now_time,
									dqa_data->aod_stime) / 1000);

	pr_info("%s\n", __func__);

	return 0;
}

int rm69330_hbm_on(struct rm69330 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	unsigned char RM69330_HBM_LEVEL[2] = { 0x63, HBM_DEFAULT_LEVEL };

	mutex_lock(&lcd->mipi_lock);

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if (lcd->hbm_level != HBM_DEFAULT_LEVEL)
		RM69330_HBM_LEVEL[1] = lcd->hbm_level;

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_HBM_LEVEL[0], RM69330_HBM_LEVEL[1]) < 0)
		dsim_err("failed to send RM69330_HBM_LEVEL\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_9[0], RM69330_INIT_9[1]) < 0)
		dsim_err("failed to send RM69330_INIT_9\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_18[0], RM69330_INIT_18[1]) < 0)
		dsim_err("failed to send RM69330_INIT_18\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_HBM_ON[0], RM69330_HBM_ON[1]) < 0)
		dsim_err("failed to send RM69330_HBM_ON\n");

	mutex_unlock(&lcd->mipi_lock);

	dqa_data->hbm_stime = ktime_get_boottime();

	pr_info("%s\n", __func__);

	return 0;
}

int rm69330_hbm_off(struct rm69330 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct backlight_device *bd = lcd->bd;
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int brightness = bd->props.brightness;
	ktime_t now_time;

	mutex_lock(&lcd->mipi_lock);

	rm69330_write_gamma(lcd, brightness);

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_1[0], RM69330_INIT_1[1]) < 0)
		dsim_err("failed to send RM69330_INIT_1\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_HBM_OFF[0], RM69330_HBM_OFF[1]) < 0)
		dsim_err("failed to send RM69330_HBM_OFF\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_9[0], RM69330_INIT_9[1]) < 0)
		dsim_err("failed to send RM69330_INIT_9\n");

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		RM69330_INIT_19[0], RM69330_INIT_19[1]) < 0)
		dsim_err("failed to send RM69330_INIT_19\n");

	mutex_unlock(&lcd->mipi_lock);

	now_time = ktime_get_boottime();
	dqa_data->hbm_time += ((unsigned int)ktime_ms_delta(now_time,
								dqa_data->hbm_stime) / 1000);

	pr_info("%s\n", __func__);

	return 0;
}
