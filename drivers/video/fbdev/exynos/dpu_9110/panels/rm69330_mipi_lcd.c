/* rm69330_mipi_lcd.c
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
#include "rm69330_param.h"
#include "rm69330_mipi_lcd.h"
#include "rm69330_lcd_ctrl.h"
#include "decon_lcd.h"

#define DISPLAY_DQA
#define BACKLIGHT_DEV_NAME	"rm69330-bl"
#define LCD_DEV_NAME		"rm69330"
#define	DEBUG_READ_DELAY	40 /* 40 ms */

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    rm69330_early_suspend;
#endif

const char* power_state_str[FB_BLANK_POWERDOWN+1] = {
	"UNBLANK", "NORMAL", "V_SUSPEND", "H_SUSPEND", "POWERDOWN",
};

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

	pr_info("%s:panel_id = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", panel_id_cmdline);

static void rm69330_debug_dwork(struct work_struct *work)
{
	struct rm69330 *lcd = container_of(work,
				struct rm69330, debug_dwork.work);
	int ret;

	cancel_delayed_work(&lcd->debug_dwork);

	if (POWER_IS_OFF(lcd->power)) {
		dev_err(lcd->dev, "%s:panel off.\n", __func__);
		return;
	}

	ret = rm69330_print_debug_reg(lcd);
	if (ret)
		pr_info("%s:failed rm69330_print_debug_reg[%d]\n", __func__, ret);

	return;
}

static void rm69330_send_esd_event(struct rm69330 *lcd)
{
	struct device *esd_dev = lcd->esd_dev;
	char *event_str = "LCD_ESD=ON";
	char *envp[] = {event_str, NULL};
	int ret;

	if (esd_dev == NULL) {
		pr_err("%s: esd_dev is NULL\n", __func__);
		return;
	}

	ret = kobject_uevent_env(&esd_dev->kobj, KOBJ_CHANGE, envp);
	if (ret)
		dev_err(lcd->dev, "%s:kobject_uevent_env fail\n", __func__);
	else
		dev_info(lcd->dev, "%s:event:[%s]\n", __func__, event_str);

	return;
}

#define ESD_RETRY_TIMEOUT		(10*1000) /* 10 seconds */
static void rm69330_esd_dwork(struct work_struct *work)
{
	struct rm69330 *lcd = container_of(work,
				struct rm69330, esd_dwork.work);

	cancel_delayed_work(&lcd->esd_dwork);

	if (POWER_IS_OFF(lcd->power)) {
		dev_err(lcd->dev, "%s:panel off.\n", __func__);
		return;
	}

	/* this dwork should be canceled by panel suspend.*/
	schedule_delayed_work(&lcd->esd_dwork,
			msecs_to_jiffies(ESD_RETRY_TIMEOUT));

	rm69330_send_esd_event(lcd);
	lcd->esd_cnt++;
}

static ssize_t rm69330_br_map_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char buffer[32];
	int ret = 0;
	int i;

	for (i = 0; i <= MAX_BRIGHTNESS; i++) {
		ret += sprintf(buffer, " %3d,", rm69330_br_map[i]);
		strcat(buf, buffer);
		if ((i == 0) || !(i %10)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");
	pr_info("%s:%s\n", __func__, buf);

	return ret;
}

static ssize_t rm69330_lpm_level_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	pr_info("%s:[%d]\n", __func__, lcd->lpm_level);

	return sprintf(buf, "%d\n", lcd->lpm_level);
}

static ssize_t rm69330_lpm_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	unsigned char level;
	int ret;

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s:control before lcd enable\n", __func__);
		return -EPERM;
	}

	ret = kstrtou8(buf, 0, &level);
	if (ret) {
		pr_err("%s:Failed to get lpm level\n", __func__);
		return -EINVAL;
	}

	lcd->lpm_level = level;

	ret = rm69330_lpm_on(lcd);
	if (ret) {
		pr_err("%s:failed LPM ON[%d]\n", __func__, ret);
		return -EIO;
	}

	pr_info("%s:LPM level[%d]\n", __func__, level);

	return size;
}

static ssize_t rm69330_hbm_level_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	pr_info("%s:[%d]\n", __func__, lcd->hbm_level);

	return sprintf(buf, "%d\n", lcd->hbm_level);
}

static ssize_t rm69330_hbm_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	unsigned char level;
	int ret;

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s:control before lcd enable\n", __func__);
		return -EPERM;
	}

	ret = kstrtou8(buf, 0, &level);
	if (ret) {
		pr_err("%s:Failed to get hbm level\n", __func__);
		return -EINVAL;
	}

	lcd->hbm_level = level;

	ret = rm69330_hbm_on(lcd);
	if (ret) {
		pr_err("%s:failed HBM ON[%d]\n", __func__, ret);
		return -EIO;
	}

	pr_info("%s:HBM level[%d]\n", __func__, level);

	return size;
}

static ssize_t rm69330_br_level_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	pr_info("%s:[%d]\n", __func__, lcd->br_level);

	return sprintf(buf, "%d\n", lcd->br_level);
}

static ssize_t rm69330_br_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	unsigned char level;
	int ret;

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s:control before lcd enable\n", __func__);
		return -EPERM;
	}

	ret = kstrtou8(buf, 0, &level);
	if (ret) {
		pr_err("%s:Failed to get br level\n", __func__);
		return -EINVAL;
	}

	ret = rm69330_br_level_ctrl(dsim->id, level);
	if (ret) {
		pr_err("%s:Failed rm69330_br_level_ctrl\n", __func__);
		return -EIO;
	}

	pr_info("%s:level[%d]\n", __func__, level);

	return size;
}

static ssize_t rm69330_lpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	pr_info("%s:[%d]\n", __func__, lcd->lpm_on);

	return sprintf(buf, "%s\n", lcd->lpm_on ? "on" : "off");
}

static ssize_t rm69330_lpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	int ret;
	bool value;

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s:control before lcd enable\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2)) {
		value = true;
	} else if (!strncmp(buf, "off", 3)) {
		value = false;
	} else {
		dev_err(dev, "%s:invalid command(use on or off)\n", __func__);
		return -EINVAL;
	}

	if (value) {
		ret = rm69330_lpm_on(lcd);
		if (ret) {
			pr_err("%s:failed LPM ON[%d]\n", __func__, ret);
			return -EIO;
		}
	} else {
		ret = rm69330_lpm_off(lcd);
		if (ret) {
			pr_err("%s:failed LPM OFF[%d]\n", __func__, ret);
			return -EIO;
		}
	}

	lcd->lpm_on = value;
	pr_info("%s:val[%d]\n", __func__, lcd->lpm_on);

	return size;
}

static ssize_t rm69330_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	pr_info("%s:[%d]\n", __func__, lcd->hbm_on);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

static ssize_t rm69330_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	int ret;

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = false;
	else {
		dev_err(dev, "%s:invalid command(use on or off)\n", __func__);
		return -EINVAL;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s:hbm control before lcd enable\n", __func__);
		return -EPERM;
	}

	if (lcd->lpm_on) {
		dev_err(lcd->dev, "%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->hbm_on) {
		ret = rm69330_hbm_on(lcd);
		if (ret) {
			pr_err("%s:failed HBM ON[%d]\n", __func__, ret);
			return -EIO;
		}
	} else {
		ret = rm69330_hbm_off(lcd);
		if (ret) {
			pr_err("%s:failed HBM OFF[%d]\n", __func__, ret);
			return -EIO;
		}
	}

	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	dev_info(lcd->dev, "%s:HBM[%s]\n", __func__, lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t rm69330_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "BOE_%06x\n", get_panel_id());
	strcat(buf, temp);

	return strlen(buf);
}

#define MAX_READ_SIZE	0xff
static int rdata_length;
static int rdata_addr;
static int rdata_offset;
static ssize_t rm69330_read_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	char read_buf[MAX_READ_SIZE] = {0, };
	char print_buf[MAX_READ_SIZE] = {0, };
	int i, ret = 0;

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%s]\n", __func__, power_state_str[lcd->power]);
		return -EPERM;
	}

	ret = rm69330_read_reg(dsim->id,
		rdata_addr+rdata_offset, &read_buf[0], rdata_length);
	if (ret) {
		pr_err("%s:read failed[%d]\n", __func__, ret);
		return -EIO;
	}

	for ( i = 0; i < rdata_length; i++) {
		if ((i !=0) && !(i%8)) {
			strcat(print_buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(print_buf, "0x%02x", read_buf[i]);
		strcat(buf, print_buf);

		if ( i < (rdata_length-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");

	pr_info("%s:addr[0x%02x] length[0x%02x] offset[0x%02x]\n",
				__func__, rdata_addr, rdata_length, rdata_offset);
	pr_info("%s\n", buf);

	return ret;
}

static ssize_t rm69330_read_reg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	u32 buff[3] = {0, }; //addr, size, offset
	int i;

	sscanf(buf, "0x%x, 0x%x, 0x%x", &buff[0], &buff[1], &buff[2]);

	for (i = 0; i < 3; i++) {
		if (buff[i] > MAX_READ_SIZE) {
			pr_info("%s:Invalid argment. index[%d] value[0x02%x]\n",
					__func__, i, buff[i]);
			return -EINVAL;
		}
	}

	rdata_addr = buff[0];
	rdata_length = buff[1];
	rdata_offset = buff[2];

	pr_info("%s:addr[0x%02x] length[0x%02x] offset[0x%02x]\n",
				__func__, buff[0], buff[1], buff[2]);

	return size;
}

static ssize_t rm69330_esd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	unsigned int esd_cnt = lcd->esd_cnt;

	lcd->esd_cnt = 0;

	return sprintf(buf, "%d\n", esd_cnt);
}

static ssize_t rm69330_esd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	cancel_delayed_work(&lcd->esd_dwork);
	schedule_delayed_work(&lcd->esd_dwork, 0);

	return size;
}

static struct device_attribute rm69330_dev_attrs[] = {
	__ATTR(br_map, S_IRUGO, rm69330_br_map_show, NULL),
	__ATTR(br_level, S_IRUGO | S_IWUSR, rm69330_br_level_show, rm69330_br_level_store),
	__ATTR(hbm_level, S_IRUGO | S_IWUSR, rm69330_hbm_level_show, rm69330_hbm_level_store),
	__ATTR(lpm_level, S_IRUGO | S_IWUSR, rm69330_lpm_level_show, rm69330_lpm_level_store),
	__ATTR(esd, S_IRUGO | S_IWUSR, rm69330_esd_show, rm69330_esd_store),
	__ATTR(lpm, S_IRUGO | S_IWUSR, rm69330_lpm_show, rm69330_lpm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, rm69330_hbm_show, rm69330_hbm_store),
	__ATTR(lcd_type, S_IRUGO , rm69330_lcd_type_show, NULL),
	__ATTR(read_reg, S_IRUGO | S_IWUSR,
			rm69330_read_reg_show, rm69330_read_reg_store),
};

#ifdef DISPLAY_DQA
static ssize_t rm69330_display_model_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct decon_lcd *lcd_info = &dsim->lcd_info;
	char temp[16];

	sprintf(temp, "BOE_%s\n", lcd_info->model_name);
	strcat(buf, temp);

	pr_info("%s:%s\n", __func__, lcd_info->model_name);

	return strlen(buf);
}

static ssize_t rm69330_lcdm_id1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	pr_info("%s:%d\n", __func__, panel->id[0]);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[0]);

	return ret;
}

static ssize_t rm69330_lcdm_id2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	pr_info("%s:%d\n", __func__, panel->id[1]);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[1]);

	return ret;
}

static ssize_t rm69330_lcdm_id3_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	pr_info("%s:%d\n", __func__, panel->id[2]);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[2]);

	return ret;
}

static ssize_t rm69330_pndsie_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int ret;

	pr_info("%s:%d\n", __func__, dsim->comm_err_cnt);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dsim->comm_err_cnt);

	dsim->comm_err_cnt = 0;

	return ret;
}

static ssize_t rm69330_qct_no_te_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int ret;

	pr_info("%s:%d\n", __func__, dsim->decon_timeout_cnt);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dsim->decon_timeout_cnt);
	dsim->decon_timeout_cnt = 0;
	dsim_store_timeout_count(dsim);

	return ret;
}

static ssize_t rm69330_lbhd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__, dqa_data->hbm_time);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dqa_data->hbm_time);
	dqa_data->hbm_time = 0;

	return ret;
}

static ssize_t rm69330_lod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__, dqa_data->disp_time);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dqa_data->disp_time);
	dqa_data->disp_time = 0;

	return ret;
}

static ssize_t rm69330_daod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__,
			(dqa_data->aod_high_time + dqa_data->aod_low_time));

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			(dqa_data->aod_high_time + dqa_data->aod_low_time));

	return ret;
}

static ssize_t rm69330_dahl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__, dqa_data->aod_high_time);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dqa_data->aod_high_time);
	dqa_data->aod_high_time = 0;

	return ret;
}

static ssize_t rm69330_dall_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__, dqa_data->aod_low_time);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dqa_data->aod_low_time);
	dqa_data->aod_low_time = 0;

	return ret;
}

static ssize_t rm69330_locnt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rm69330 *lcd = dev_get_drvdata(dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	int ret;

	pr_info("%s:%d\n", __func__, dqa_data->disp_cnt);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dqa_data->disp_cnt);
	dqa_data->disp_cnt = 0;

	return ret;
}

static struct device_attribute rm69330_dqa_attrs[] = {
	__ATTR(disp_model, S_IRUGO , rm69330_display_model_show, NULL),
	__ATTR(lcdm_id1, S_IRUGO , rm69330_lcdm_id1_show, NULL),
	__ATTR(lcdm_id2, S_IRUGO , rm69330_lcdm_id2_show, NULL),
	__ATTR(lcdm_id3, S_IRUGO , rm69330_lcdm_id3_show, NULL),
	__ATTR(pndsie, S_IRUGO , rm69330_pndsie_show, NULL),
	__ATTR(qct_no_te, S_IRUGO, rm69330_qct_no_te_show, NULL),
	__ATTR(daod, S_IRUGO, rm69330_daod_show, NULL),
	__ATTR(dahl, S_IRUGO, rm69330_dahl_show, NULL),
	__ATTR(dall, S_IRUGO, rm69330_dall_show, NULL),
	__ATTR(lbhd, S_IRUGO , rm69330_lbhd_show, NULL),
	__ATTR(lod, S_IRUGO , rm69330_lod_show, NULL),
	__ATTR(locnt, S_IRUGO , rm69330_locnt_show, NULL),

};
#endif

static int rm69330_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int rm69330_update_brightness(struct rm69330 *lcd)
{
	struct backlight_device *bd = lcd->bd;
	struct dsim_device *dsim = lcd->dsim;
	int brightness = bd->props.brightness, ret = 0;

	if (lcd->hbm_on) {
		ret = rm69330_hbm_on(lcd);
		if (ret) {
			pr_err("%s:failed change_brightness\n", __func__);
			goto out;
		}
	} else {
		ret = rm69330_gamma_ctrl(dsim->id, brightness);
		if (ret) {
			pr_err("%s:failed change_brightness\n", __func__);
			goto out;
		}
	}

	pr_info("%s:br[%d] lv[%d] hbm[%d]\n", __func__,
				brightness, rm69330_br_map[brightness], lcd->hbm_on);

out:
	return ret;
}

static int rm69330_set_brightness(struct backlight_device *bd)
{
	struct rm69330 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness, ret = 0;

	if (lcd == NULL) {
		pr_err("%s:LCD is NULL\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_err("Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	pr_info("%s:br[%d] pwr[%s]\n", __func__,
				brightness, power_state_str[lcd->power]);

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_debug("%s:invalid power[%d]\n", __func__, lcd->power);
		ret = 0;
		goto out;
	}

	if (lcd->lpm_on) {
		pr_err("%s:LPM enabled\n", __func__);
		ret = 0;
		goto out;
	}

	ret = rm69330_update_brightness(lcd);
	if (ret)
		pr_err("%s:failed to update brightness\n", __func__);

out:
	return ret;
}

static int rm69330_aod_ctrl(struct dsim_device *dsim, bool enable)
{
	struct lcd_device *panel = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&panel->dev);

	lcd->lpm_on = enable;

	if (lcd->lpm_on) {
		rm69330_lpm_on(lcd);
		if (lcd->pinctrl && lcd->gpio_aod) {
			if (pinctrl_select_state(lcd->pinctrl, lcd->gpio_aod))
				pr_err("%s:failed to turn on gpio_aod\n", __func__);
		} else
			pr_err("%s:pinctrl or gpio_aod is NULL\n", __func__);
	} else
		rm69330_lpm_off(lcd);

	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	pr_info("%s:enable[%d]lpm_on[%d]\n", __func__, enable, lcd->lpm_on);

	return 0;
}

static int rm69330_pinctrl_configure(struct rm69330 *lcd)
{
	int retval = 0;

	lcd->pinctrl = devm_pinctrl_get(lcd->dev);
	if (IS_ERR(lcd->pinctrl)) {
		if (PTR_ERR(lcd->pinctrl) == -EPROBE_DEFER) {
			pr_err("%s:failed to get pinctrl\n", __func__);
			retval = -ENODEV;
		}
		pr_info("%s:Target does not use pinctrl\n", __func__);
		lcd->pinctrl = NULL;
		goto out;
	}

	if (lcd->pinctrl) {
		lcd->gpio_aod = pinctrl_lookup_state(lcd->pinctrl, "aod_on");
		if (IS_ERR(lcd->gpio_aod)) {
			pr_err("%s:failed to get gpio_aod pin state\n", __func__);
			lcd->gpio_aod = NULL;
		}

		lcd->gpio_off = pinctrl_lookup_state(lcd->pinctrl, "aod_off");
		if (IS_ERR(lcd->gpio_off)) {
			pr_err("%s:failed to get gpio_off pin state\n", __func__);
			lcd->gpio_off = NULL;
		}
	}

	if (lcd->pinctrl && lcd->gpio_off) {
		if (pinctrl_select_state(lcd->pinctrl, lcd->gpio_off))
			pr_err("%s:failed to turn on gpio_off\n", __func__);
	} else
		pr_err("%s:pinctrl or gpio_off is NULL\n", __func__);

out:
	return retval;
}

static int rm69330_get_power(struct lcd_device *ld)
{
	struct rm69330 *lcd = dev_get_drvdata(&ld->dev);

	pr_debug("%s [%d]\n", __func__, lcd->power);

	return lcd->power;
}

static int rm69330_set_power(struct lcd_device *ld, int power)
{
	struct rm69330 *lcd = dev_get_drvdata(&ld->dev);
	if (power > FB_BLANK_POWERDOWN) {
		pr_err("%s:invalid power state.[%d]\n", __func__, power);
		return -EINVAL;
	}

	lcd->power = power;
	wristup_booster_set_lcd_power(power);

	pr_info("%s[%s]\n", __func__, power_state_str[lcd->power]);

	return 0;
}

static struct lcd_ops rm69330_lcd_ops = {
	.get_power = rm69330_get_power,
	.set_power = rm69330_set_power,
};

static const struct backlight_ops rm69330_backlight_ops = {
	.get_brightness = rm69330_get_brightness,
	.update_status = rm69330_set_brightness,
};

static int rm69330_probe(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;
	struct rm69330 *lcd;
	int ret = 0, i;

	pr_info("%s\n", __func__);

	if (get_panel_id() == -1) {
		pr_err("%s:No lcd attached!\n", __func__);
		return -ENODEV;
	}

	lcd = devm_kzalloc(dsim->dev,
			sizeof(struct rm69330), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s:failed to allocate rm69330 structure\n", __func__);
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->hbm_level = HBM_DEFAULT_LEVEL;
	lcd->lpm_level = LPM_DEFAULT_LEVEL;

	panel->id[0] = ((panel_id >> 16) & 0xff);
	panel->id[1] = ((panel_id >> 8) & 0xff);
	panel->id[2] = (panel_id & 0xff);

	mutex_init(&lcd->mipi_lock);

	lcd->bd = backlight_device_register(BACKLIGHT_DEV_NAME,
		lcd->dev, lcd, &rm69330_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s:failed to register backlight device[%d]\n",
			__func__, (int)PTR_ERR(lcd->bd));
		ret = PTR_ERR(lcd->bd);
		goto err_bd;
	}
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim->ld = lcd_device_register(LCD_DEV_NAME,
			lcd->dev, lcd, &rm69330_lcd_ops);
	if (IS_ERR(dsim->ld)) {
		pr_err("%s:failed to register lcd ops[%d]\n",
			__func__, (int)PTR_ERR(dsim->ld));
		ret = PTR_ERR(lcd->bd);
		goto err_ld;
	}
	lcd->ld = dsim->ld;

	lcd->esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(lcd->esd_class)) {
		dev_err(lcd->dev, "%s:Failed to create esd_class[%d]\n",
			__func__, (int)PTR_ERR(lcd->esd_class));
		ret = PTR_ERR(lcd->esd_class);
		goto err_esd_class;
	}

	lcd->esd_dev = device_create(lcd->esd_class, lcd->dev, 0, lcd, "esd");
	if (IS_ERR(lcd->esd_dev)) {
		dev_err(lcd->dev, "%s:Failed to create esd_dev\n", __func__);
		goto err_esd_dev;
	}
	INIT_DELAYED_WORK(&lcd->esd_dwork, rm69330_esd_dwork);

	for (i = 0; i < ARRAY_SIZE(rm69330_dev_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev, &rm69330_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev,
				"%s:failed to add rm69330_dev_attrs\n", __func__);
			for (i--; i >= 0; i--)
				device_remove_file(&lcd->ld->dev, &rm69330_dev_attrs[i]);
			goto err_create_dev_file;
		}
	}

#ifdef DISPLAY_DQA
	for (i = 0; i < ARRAY_SIZE(rm69330_dqa_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev, &rm69330_dqa_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev,
				"%s:failed to add rm69330_dqa_attrs\n", __func__);
			for (i--; i >= 0; i--)
				device_remove_file(&lcd->ld->dev, &rm69330_dqa_attrs[i]);
			goto err_create_dqa_file;
		}
	}
#endif

	ret = rm69330_pinctrl_configure(lcd);
	if (ret)
		dev_err(&lcd->ld->dev,
				"%s:failed rm69330_pinctrl_configure\n", __func__);

	INIT_DELAYED_WORK(&lcd->debug_dwork, rm69330_debug_dwork);
	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	pr_info("%s done\n", __func__);

	return 0;

#ifdef DISPLAY_DQA
err_create_dqa_file:
	for (i--; i >= 0; i--)
		device_remove_file(&lcd->ld->dev, &rm69330_dev_attrs[i]);
#endif
err_create_dev_file:
	device_destroy(lcd->esd_class, lcd->esd_dev->devt);
err_esd_dev:
	class_destroy(lcd->esd_class);
err_esd_class:
	lcd_device_unregister(lcd->ld);
err_ld:
	backlight_device_unregister(lcd->bd);
err_bd:
	mutex_destroy(&lcd->mipi_lock);
	devm_kfree(dsim->dev, lcd);
	return ret;
}

static int rm69330_pre_reset(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	pr_info("%s\n", __func__);

	msleep(5);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		dsim_err("failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	msleep(1);
	gpio_set_value(res->lcd_reset, 0);
	msleep(1);
	gpio_set_value(res->lcd_reset, 1);
	msleep(10);
	gpio_free(res->lcd_reset);

	return 0;
}

static int rm69330_displayon(struct dsim_device *dsim)
{
	struct lcd_device *lcd_dev = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&lcd_dev->dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;

	lcd->power = FB_BLANK_UNBLANK;

	rm69330_update_brightness(lcd);
	rm69330_enable(dsim->id);

	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	dqa_data->disp_stime = ktime_get();
	dqa_data->disp_cnt++;

	pr_info("%s\n", __func__);

	return 0;
}

static int rm69330_init(struct dsim_device *dsim)
{
	usleep_range(1000, 1100);

	rm69330_init_ctrl(dsim->id, &dsim->lcd_info);
	pr_info("%s\n", __func__);
	return 0;
}

static int rm69330_suspend(struct dsim_device *dsim)
{
	struct lcd_device *lcd_dev = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&lcd_dev->dev);
	struct dqa_data_t *dqa_data = &lcd->dqa_data;
	ktime_t now_time;

	cancel_delayed_work_sync(&lcd->debug_dwork);
	cancel_delayed_work_sync(&lcd->esd_dwork);

	rm69330_disable(dsim->id);

	lcd->power = FB_BLANK_POWERDOWN;

	now_time = ktime_get();
	dqa_data->disp_time += ((unsigned int)ktime_ms_delta(now_time,
										dqa_data->disp_stime) / 1000);

	if (lcd->pinctrl && lcd->gpio_off) {
		if (pinctrl_select_state(lcd->pinctrl, lcd->gpio_off))
			pr_err("%s:failed to turn on gpio_off\n", __func__);
	} else
		pr_err("%s:pinctrl or gpio_off is NULL\n", __func__);


	pr_info("%s\n", __func__);
	return 0;
}

static int rm69330_resume(struct dsim_device *dsim)
{
	return 0;
}

static int rm69330_dump(struct dsim_device *dsim)
{
	struct lcd_device *lcd_dev = dsim->ld;
	struct rm69330 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int ret;

	ret = rm69330_print_debug_reg(lcd);
	if (ret)
		pr_info("%s:failed rm69330_print_debug_reg[%d]\n", __func__, ret);

	return 0;
}

struct dsim_lcd_driver rm69330_mipi_lcd_driver = {
	.probe		= rm69330_probe,
	.init		= rm69330_init,
	.displayon	= rm69330_displayon,
	.pre_reset	= rm69330_pre_reset,
 	.suspend	= rm69330_suspend,
	.resume		= rm69330_resume,
	.dump		= rm69330_dump,
	.aod_ctrl	= rm69330_aod_ctrl,
};
