/* rm6d010_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * SeungBeom, Park <sb1.parki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __RM6D010_MIPI_LCD_H__
#define __RM6D010_MIPI_LCD_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>

#include <video/mipi_display.h>
#include <linux/platform_device.h>

enum {
	AO_NODE_OFF = 0,
	AO_NODE_ALPM = 1,
};

struct rm6d010 {
	struct device		*dev;
	struct device		*esd_dev;
	struct class		*esd_class;
	struct dsim_device	*dsim;
	struct lcd_device		*ld;
	struct backlight_device	*bd;
	struct regulator		*vdd3;
	struct regulator		*vci;
	struct work_struct		det_work;
	struct mutex		mipi_lock;
	unsigned char		*br_map;
	unsigned int		reset_gpio;
	unsigned int		te_gpio;
	unsigned int		det_gpio;
	unsigned int		err_gpio;
	unsigned int		esd_irq;
	unsigned int		err_irq;
	unsigned int		power;
	unsigned int		acl;
	unsigned int		refresh;
	unsigned char		default_hbm;
	unsigned int		dbg_cnt;
	unsigned int		ao_mode;
	unsigned int		temp_stage;
	bool			alpm_on;
	bool			lp_mode;
	bool			boot_power_on;
	bool			br_ctl;
	bool			scm_on;
	bool			aod_enable;
	bool			irq_on;
	bool			hbm_on;
#ifdef CONFIG_SLEEP_MONITOR
	unsigned int		act_cnt;
#endif
};
#endif
