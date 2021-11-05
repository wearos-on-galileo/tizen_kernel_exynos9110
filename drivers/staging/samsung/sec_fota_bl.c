/*
 * sec_fota_bl.c
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/sec_sysfs.h>

#define FOTA_BL_STATUS_MAX_SIZE 15

static char fota_bl_status[FOTA_BL_STATUS_MAX_SIZE];

static int __init sec_fota_bl_status(char *arg)
{
	snprintf(fota_bl_status, FOTA_BL_STATUS_MAX_SIZE, arg);

	return 0;
}
early_param("tizenboot.fota_bl_status", sec_fota_bl_status);

static ssize_t sec_fota_bl_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, FOTA_BL_STATUS_MAX_SIZE, "%s", fota_bl_status);
}

static DEVICE_ATTR(status, S_IRUGO, sec_fota_bl_status_show, NULL);

static int __init sec_fota_bl_init(void)
{
	int ret = 0;
	struct device *sec_fota_bl_dev;

	sec_fota_bl_dev = sec_device_create(NULL, "fota_bl");
	if (IS_ERR(sec_fota_bl_dev)) {
		pr_err("sec_fota_bl_dev create fail\n");
		return -ENODEV;
	}

	ret = device_create_file(sec_fota_bl_dev, &dev_attr_status);
	if (ret < 0) {
		pr_err("dev_attr_fota_bl create fail\n");
		return -ENODEV;
	}

	return 0;
}
device_initcall(sec_fota_bl_init);
