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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include <linux/lcd.h>

#include "../dsim.h"
#include "rm6d010_param.h"
#include "rm6d010_mipi_lcd.h"
#include "rm6d010_lcd_ctrl.h"
#include "decon_lcd.h"

#define BACKLIGHT_DEV_NAME	"rm6d010-bl"
#define LCD_DEV_NAME		"rm6d010"

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    rm6d010_early_suspend;
#endif

static int panel_id;
int get_panel_id(void)
{
	return panel_id;
}
EXPORT_SYMBOL(get_panel_id);

static int __init panel_id_cmdline(char *mode)
{
	char *pt;

	panel_id = 0;
	if (mode == NULL)
		return 1;

	for (pt = mode; *pt != 0; pt++) {
		panel_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			panel_id += *pt - '0';
		break;
		case 'a' ... 'f':
			panel_id += 10 + *pt - 'a';
		break;
		case 'A' ... 'F':
			panel_id += 10 + *pt - 'A';
		break;
		}
	}

	pr_info("%s: panel_id = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", panel_id_cmdline);

#if 0
static void rm6d010_esd_detect_work(struct work_struct *work)
{
	struct rm6d010 *lcd = container_of(work,
				struct rm6d010, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = {event_string, NULL};

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&lcd->esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev, "%s:Send uevent. ESD DETECTED\n", __func__);
	}
}
#endif

static ssize_t rm6d010_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm6d010 *lcd = dev_get_drvdata(dev);
	int len = 0;

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	switch (lcd->ao_mode) {
	case AO_NODE_OFF:
		len = sprintf(buf, "%s\n", "off");
		break;
	case AO_NODE_ALPM:
		len = sprintf(buf, "%s\n", "on");
		break;
 	default:
		dev_warn(dev, "invalid status.\n");
		break;
	}

	return len;
}

static ssize_t rm6d010_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm6d010 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->ao_mode = AO_NODE_ALPM;
	else if (!strncmp(buf, "off", 3))
		lcd->ao_mode = AO_NODE_OFF;
 	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	return size;
}

static ssize_t rm6d010_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm6d010 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

extern int rm6d010_hbm_on(int id);
static ssize_t rm6d010_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm6d010 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	rm6d010_hbm_on(dsim->id);

	dev_info(lcd->dev, "HBM %s.\n", lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t rm6d010_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "BOE_%06x\n", get_panel_id());
	strcat(buf, temp);

	return strlen(buf);
}

static struct device_attribute rm6d010_dev_attrs[] = {
	__ATTR(alpm, S_IRUGO | S_IWUSR, rm6d010_alpm_show, rm6d010_alpm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, rm6d010_hbm_show, rm6d010_hbm_store),
	__ATTR(lcd_type, S_IRUGO , rm6d010_lcd_type_show, NULL),
};

static int rm6d010_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 29:
		backlightlevel = 0;
		break;
	case 30 ... 34:
		backlightlevel = 1;
		break;
	case 35 ... 39:
		backlightlevel = 2;
		break;
	case 40 ... 44:
		backlightlevel = 3;
		break;
	case 45 ... 49:
		backlightlevel = 4;
		break;
	case 50 ... 54:
		backlightlevel = 5;
		break;
	case 55 ... 64:
		backlightlevel = 6;
		break;
	case 65 ... 74:
		backlightlevel = 7;
		break;
	case 75 ... 83:
		backlightlevel = 8;
		break;
	case 84 ... 93:
		backlightlevel = 9;
		break;
	case 94 ... 103:
		backlightlevel = 10;
		break;
	case 104 ... 113:
		backlightlevel = 11;
		break;
	case 114 ... 122:
		backlightlevel = 12;
		break;
	case 123 ... 132:
		backlightlevel = 13;
		break;
	case 133 ... 142:
		backlightlevel = 14;
		break;
	case 143 ... 152:
		backlightlevel = 15;
		break;
	case 153 ... 162:
		backlightlevel = 16;
		break;
	case 163 ... 171:
		backlightlevel = 17;
		break;
	case 172 ... 181:
		backlightlevel = 18;
		break;
	case 182 ... 191:
		backlightlevel = 19;
		break;
	case 192 ... 201:
		backlightlevel = 20;
		break;
	case 202 ... 210:
		backlightlevel = 21;
		break;
	case 211 ... 220:
		backlightlevel = 22;
		break;
	case 221 ... 230:
		backlightlevel = 23;
		break;
	case 231 ... 240:
		backlightlevel = 24;
		break;
	case 241 ... 250:
		backlightlevel = 25;
		break;
	case 251 ... 255:
		backlightlevel = 26;
		break;
	default:
		backlightlevel = 12;
		break;
	}

	return backlightlevel;
}

static int update_brightness(int brightness)
{
	int backlightlevel;

	backlightlevel = get_backlight_level(brightness);
	return 0;
}

static int rm6d010_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_err("Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 0;
}

static int s6e36w1x01_get_power(struct lcd_device *ld)
{
	struct rm6d010 *lcd = dev_get_drvdata(&ld->dev);

	pr_debug("%s [%d]\n", __func__, lcd->power);

	return lcd->power;
}

static int s6e36w1x01_set_power(struct lcd_device *ld, int power)
{
	struct rm6d010 *lcd = dev_get_drvdata(&ld->dev);
	const char* power_state[FB_BLANK_POWERDOWN+1] = {
	"UNBLANK",
	"NORMAL",
	"V_SUSPEND",
	"H_SUSPEND",
	"POWERDOWN",};

	if (power > FB_BLANK_POWERDOWN) {
		pr_err("%s: invalid power state.[%d]\n", __func__, power);
		return -EINVAL;
	}

	lcd->power = power;

#ifdef CONFIG_SLEEP_MONITOR
	if (power == FB_BLANK_UNBLANK)
		lcd->act_cnt++;
#endif
	pr_info("%s[%s]\n", __func__, power_state[lcd->power]);

	return 0;
}

irqreturn_t rm6d010_esd_interrupt(int irq, void *dev_id)
{
	struct rm6d010 *lcd = dev_id;

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_info(lcd->dev, "%s: add esd schedule_work by irq[%d]]\n",
			__func__, irq);
	}

	return IRQ_HANDLED;
}

static struct lcd_ops rm6d010_lcd_ops = {
	.get_power = s6e36w1x01_get_power,
	.set_power = s6e36w1x01_set_power,
};

static const struct backlight_ops rm6d010_backlight_ops = {
	.get_brightness = rm6d010_get_brightness,
	.update_status = rm6d010_set_brightness,
};

static int rm6d010_probe(struct dsim_device *dsim)
{
//	struct dsim_resources *res = &dsim->res;
	struct rm6d010 *lcd;
	int ret, i;

	pr_info("%s\n", __func__);

	if (get_panel_id() == -1) {
		pr_err("%s: No lcd attached!\n", __func__);
		return -ENODEV;
	}

	lcd = devm_kzalloc(dsim->dev,
			sizeof(struct rm6d010), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate rm6d010 structure.\n", __func__);
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	mutex_init(&lcd->mipi_lock);

	lcd->bd = backlight_device_register(BACKLIGHT_DEV_NAME,
		lcd->dev, lcd, &rm6d010_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device[%d]\n",
			__func__, (int)PTR_ERR(lcd->bd));
		ret = PTR_ERR(lcd->bd);
		goto err_bd;
	}
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim->ld = lcd_device_register(LCD_DEV_NAME,
			lcd->dev, lcd, &rm6d010_lcd_ops);
	if (IS_ERR(dsim->ld)) {
		pr_err("%s: failed to register lcd ops[%d]\n",
			__func__, (int)PTR_ERR(dsim->ld));
		ret = PTR_ERR(lcd->bd);
		goto err_ld;
	}
	lcd->ld = dsim->ld;

	lcd->esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(lcd->esd_class)) {
		dev_err(lcd->dev, "%s: Failed to create esd_class[%d]\n",
			__func__, (int)PTR_ERR(lcd->esd_class));
		ret = PTR_ERR(lcd->esd_class);
		goto err_esd_class;
	}

#if 0
	lcd->esd_dev = device_create(lcd->esd_class, NULL, 0, NULL, "esd");
	if (IS_ERR(lcd->esd_dev)) {
		dev_err(lcd->dev, "Failed to create esd_dev\n");
		goto err_esd_dev;
	}
	INIT_WORK(&lcd->det_work, rm6d010_esd_detect_work);

	if (res->err_fg > 0) {
		lcd->esd_irq = gpio_to_irq(res->err_fg);
		dev_info(lcd->dev, "esd_irq_num [%d]\n", lcd->esd_irq);
		ret = devm_request_irq(lcd->dev, lcd->esd_irq,
					rm6d010_esd_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "err_fg",
					lcd);
		if (ret < 0) {
			dev_err(lcd->dev, "failed to request det irq.\n");
			goto err_err_fg;
		}
	}
#endif
	for (i = 0; i < ARRAY_SIZE(rm6d010_dev_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev, &rm6d010_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(&lcd->ld->dev, &rm6d010_dev_attrs[i]);
			goto err_create_file;
		}
	}

	pr_info("%s done\n", __func__);

	return 0;

err_create_file:
//err_err_fg:
//	device_destroy(lcd->esd_class, lcd->esd_dev->devt);
//err_esd_dev:
//	class_destroy(lcd->esd_class);
err_esd_class:
	lcd_device_unregister(lcd->ld);
err_ld:
	backlight_device_unregister(lcd->bd);
err_bd:
	mutex_destroy(&lcd->mipi_lock);
	devm_kfree(dsim->dev, lcd);
	return ret;
}

static int rm6d010_pre_reset(struct dsim_device *dsim)
{
	struct lcd_device *lcd_dev = dsim->ld;
	struct dsim_resources *res = &dsim->res;
	struct rm69330 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int ret;

	pr_info("%s\n", __func__);

	msleep(10);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		dsim_err("failed to get LCD reset GPIO\n");
		return -EINVAL;
	}
	msleep(20);

	gpio_free(res->lcd_reset);

	return 0;
}

static int rm6d010_displayon(struct dsim_device *dsim)
{
	rm6d010_enable(dsim->id);
	return 0;
}


static int rm6d010_init(struct dsim_device *dsim)
{
	rm6d010_init_ctrl(dsim->id, &dsim->lcd_info);
	return 0;
}

static int rm6d010_suspend(struct dsim_device *dsim)
{
	rm6d010_disable(dsim->id);
	return 0;
}

static int rm6d010_resume(struct dsim_device *dsim)
{
	return 0;
}

static int rm6d010_dump(struct dsim_device *dsim)
{
	return 0;
}

struct dsim_lcd_driver rm6d010_mipi_lcd_driver = {
	.probe		= rm6d010_probe,
	.init		= rm6d010_init,
	.pre_reset	= rm6d010_pre_reset,
	.displayon	= rm6d010_displayon,
 	.suspend		= rm6d010_suspend,
	.resume		= rm6d010_resume,
	.dump		= rm6d010_dump,
};
