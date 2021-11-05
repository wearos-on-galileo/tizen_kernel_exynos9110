/*
 * max77705-muic.c - MUIC driver for the Maxim 77705
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Insun Choi <insun77.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

 #define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/switch.h>

/* MUIC header file */
#include <linux/mfd/max77705.h>
#include <linux/mfd/max77705-private.h>
#include <linux/muic/muic.h>
#include <linux/muic/max77705-muic.h>
#include <linux/ccic/max77705_usbc.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
#include <linux/usb_notify.h>
#endif

#include <linux/sec_debug.h>
#include <linux/sec_ext.h>

struct max77705_muic_data *g_muic_data;

/* for the bringup, should be fixed */
void __init_usbc_cmd_data(usbc_cmd_data *cmd_data)
{
	pr_warn("%s is not defined!\n", __func__);
}
void __max77705_usbc_opcode_write(struct max77705_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op)
{
	pr_warn("%s is not defined!\n", __func__);
}
void init_usbc_cmd_data(usbc_cmd_data *cmd_data)
	__attribute__((weak, alias("__init_usbc_cmd_data")));
void max77705_usbc_opcode_write(struct max77705_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op)
	__attribute__((weak, alias("__max77705_usbc_opcode_write")));

static bool debug_en_vps;

static void max77705_muic_detect_dev(struct max77705_muic_data *muic_data,
	int irq);

struct max77705_muic_vps_data {
	int				adc;
	int				vbvolt;
	int				chgtyp;
	int				muic_switch;
	const char			*vps_name;
	const muic_attached_dev_t	attached_dev;
};

static int max77705_muic_read_reg
		(struct i2c_client *i2c, u8 reg, u8 *value)
{
	int ret = max77705_read_reg(i2c, reg, value);

	return ret;
}

#if 0
static int max77705_muic_write_reg
		(struct i2c_client *i2c, u8 reg, u8 value, bool debug_en)
{
	int ret = max77705_write_reg(i2c, reg, value);

	if (debug_en)
		pr_info("%s Reg[0x%02x]: 0x%02x\n",
				__func__, reg, value);

	return ret;
}
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
static void max77705_muic_handle_vbus(struct max77705_muic_data *muic_data)
{
	int vbvolt = muic_data->status3 & BC_STATUS_VBUSDET_MASK;
	vbus_status_t status = (vbvolt > 0) ?
		STATUS_VBUS_HIGH : STATUS_VBUS_LOW;

	pr_info("%s <%d>\n", __func__, status);

	vbus_notifier_handle(status);
}
#endif

static const struct max77705_muic_vps_data muic_vps_table[] = {
	{
		.adc		= MAX77705_UIADC_523K,
		.vbvolt		= VB_LOW,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.muic_switch	= COM_UART,
		.vps_name	= "JIG UART OFF",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_523K,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_UART,
		.vps_name	= "JIG UART OFF/VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_619K,
		.vbvolt		= VB_LOW,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.muic_switch	= COM_UART,
		.vps_name	= "JIG UART ON",
		.attached_dev	= ATTACHED_DEV_JIG_UART_ON_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_619K,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_UART,
		.vps_name	= "JIG UART ON/VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_ON_VB_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_255K,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "JIG USB OFF",
		.attached_dev	= ATTACHED_DEV_JIG_USB_OFF_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_301K,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "JIG USB ON",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_USB,
		.muic_switch	= COM_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_TIMEOUT_OPEN,
		.muic_switch	= COM_USB,
		.vps_name	= "DCD Timeout",
		.attached_dev	= ATTACHED_DEV_TIMEOUT_OPEN_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_CDP,
		.muic_switch	= COM_USB,
		.vps_name	= "CDP",
		.attached_dev	= ATTACHED_DEV_CDP_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_GND,
		.vbvolt		= VB_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "OTG",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_UNOFFICIAL_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_TA_MUIC,
	},
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "AFC Charger",
		.attached_dev	= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "AFC Charger",
		.attached_dev	= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "QC Charger",
		.attached_dev	= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	},
	{
		.adc		= MAX77705_UIADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "QC Charger",
		.attached_dev	= ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	},
#endif
};

static int muic_lookup_vps_table(muic_attached_dev_t new_dev,
	struct max77705_muic_data *muic_data)
{
	int i;
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_data;
	const struct max77705_muic_vps_data *tmp_vps;

	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_USBC_STATUS2, &reg_data);
	reg_data = reg_data & USBC_STATUS2_SYSMSG_MASK;
	pr_info("%s Last sysmsg = 0x%02x\n", __func__, reg_data);

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->attached_dev != new_dev)
			continue;

		pr_info("%s (%d) vps table match found at i(%d), %s\n",
				__func__, new_dev, i,
				tmp_vps->vps_name);

		return i;
	}

	pr_info("%s can't find (%d) on vps table\n",
			__func__, new_dev);

	return -ENODEV;
}

static bool muic_check_support_dev(struct max77705_muic_data *muic_data,
			muic_attached_dev_t attached_dev)
{
	bool ret = muic_data->muic_support_list[attached_dev];

	if (debug_en_vps)
		pr_info("%s [%c]\n", __func__, ret ? 'T':'F');

	return ret;
}

static void max77705_switch_path(struct max77705_muic_data *muic_data,
	u8 reg_val)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;

	pr_info("%s value(0x%x)\n", __func__, reg_val);

#if defined(CONFIG_CCIC_NOTIFIER)
	if (muic_data->usbc_pdata->fac_water_enable) {
		pr_info("%s fac_water_enable(%d), skip\n", __func__,
			muic_data->usbc_pdata->fac_water_enable);
		return;
	}
#endif

	init_usbc_cmd_data(&write_data);
	write_data.opcode = COMMAND_CONTROL1_WRITE;
	write_data.write_length = 1;
	write_data.write_data[0] = reg_val;
	write_data.read_length = 0;

	max77705_usbc_opcode_write(usbc_pdata, &write_data);
}

static void com_to_open(struct max77705_muic_data *muic_data)
{
	u8 reg_val;

	pr_info("%s\n", __func__);

	reg_val = COM_OPEN;

	/* write command - switch */
	max77705_switch_path(muic_data, reg_val);
}

static int com_to_usb_ap(struct max77705_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s\n", __func__);

	reg_val = COM_USB;

	/* write command - switch */
	max77705_switch_path(muic_data, reg_val);

	return ret;
}

static int com_to_usb_cp(struct max77705_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s\n", __func__);

	reg_val = COM_USB_CP;

	/* write command - switch */
	max77705_switch_path(muic_data, reg_val);

	return ret;
}

static void com_to_uart_ap(struct max77705_muic_data *muic_data)
{
	u8 reg_val;
#if defined(CONFIG_MUIC_MAX77705_CCIC)
	if ((muic_data->pdata->opmode & OPMODE_MUIC) && muic_data->pdata->rustproof_on)
#else
	if (muic_data->pdata->rustproof_on)
#endif
		reg_val = COM_OPEN;
	else
		reg_val = COM_UART;

	pr_info("%s(%d)\n", __func__, reg_val);

	/* write command - switch */
	max77705_switch_path(muic_data, reg_val);
}

static void com_to_uart_cp(struct max77705_muic_data *muic_data)
{
	u8 reg_val;

#if defined(CONFIG_MUIC_MAX77705_CCIC)
	if ((muic_data->pdata->opmode & OPMODE_MUIC) && muic_data->pdata->rustproof_on)
#else
	if (muic_data->pdata->rustproof_on)
#endif
		reg_val = COM_OPEN;
	else
		reg_val = COM_UART_CP;

	pr_info("%s(%d)\n", __func__, reg_val);

	/* write command - switch */
	max77705_switch_path(muic_data, reg_val);
}

static int write_vps_regs(struct max77705_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	const struct max77705_muic_vps_data *tmp_vps;
	int vps_index;
	u8 prev_switch;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev, muic_data);
	if (vps_index < 0) {
		pr_info("%s: prev cable is none.\n", __func__);
		prev_switch = COM_OPEN;
	} else {
		/* Prev cable information. */
		tmp_vps = &(muic_vps_table[vps_index]);
		prev_switch = tmp_vps->muic_switch;
	}

	if (prev_switch == muic_data->switch_val)
		pr_info("%s Duplicated(0x%02x), just ignore\n",
			__func__, muic_data->switch_val);
#if 0
	else {
		/* write command - switch */
		max77705_switch_path(muic_data, muic_data->switch_val);
	}
#endif

	return 0;
}

/* muic uart path control function */
static int switch_to_ap_uart(struct max77705_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		com_to_uart_ap(muic_data);
		break;
	default:
		pr_warn("%s current attached is (%d) not Jig UART Off\n",
			__func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static int switch_to_cp_uart(struct max77705_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		com_to_uart_cp(muic_data);
		break;
	default:
		pr_warn("%s current attached is (%d) not Jig UART Off\n",
			__func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static void max77705_muic_enable_chgdet(struct max77705_muic_data *muic_data)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data update_data;

	pr_info("%s\n", __func__);

	init_usbc_cmd_data(&update_data);
	update_data.opcode = COMMAND_BC_CTRL1_READ;
	update_data.mask = BC_CTRL1_CHGDetEn_MASK | BC_CTRL1_CHGDetMan_MASK;
	update_data.val = 0xff;

	max77705_usbc_opcode_update(usbc_pdata, &update_data);
}

static void max77705_muic_disable_chgdet(struct max77705_muic_data *muic_data)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data update_data;

	pr_info("%s\n", __func__);

	init_usbc_cmd_data(&update_data);
	update_data.opcode = COMMAND_BC_CTRL1_READ;
	update_data.mask = BC_CTRL1_CHGDetEn_MASK;
	update_data.val = 0x0;

	max77705_usbc_opcode_update(usbc_pdata, &update_data);
}

static u8 max77705_muic_get_adc_value(struct max77705_muic_data *muic_data)
{
	u8 status;
	u8 adc = MAX77705_UIADC_ERROR;
	int ret;

	ret = max77705_muic_read_reg(muic_data->i2c,
			MAX77705_USBC_REG_USBC_STATUS1, &status);
	if (ret)
		pr_err("%s fail to read muic reg(%d)\n",
				__func__, ret);
	else
		adc = status & USBC_STATUS1_UIADC_MASK;

	return adc;
}

static u8 max77705_muic_get_vbadc_value(struct max77705_muic_data *muic_data)
{
	u8 status;
	u8 vbadc = 0;
	int ret;

	ret = max77705_muic_read_reg(muic_data->i2c,
			MAX77705_USBC_REG_USBC_STATUS1, &status);
	if (ret)
		pr_err("%s fail to read muic reg(%d)\n",
				__func__, ret);
	else
		vbadc = (status & USBC_STATUS1_VBADC_MASK) >> USBC_STATUS1_VBADC_SHIFT;

	return vbadc;
}

static ssize_t max77705_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("%s %s", __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77705_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	mutex_lock(&muic_data->muic_mutex);

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data, muic_data->attached_dev);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data, muic_data->attached_dev);
	} else {
		pr_warn("%s invalid value\n", __func__);
	}

	pr_info("%s uart_path(%d)\n", __func__,
			pdata->uart_path);

	mutex_unlock(&muic_data->muic_mutex);

	return count;
}

static ssize_t max77705_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		mode = "PDA\n";
		break;
	case MUIC_PATH_USB_CP:
		mode = "MODEM\n";
		break;
	default:
		break;
	}

	pr_debug("%s %s", __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77705_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3))
		pdata->usb_path = MUIC_PATH_USB_AP;
	else if (!strncasecmp(buf, "MODEM", 5))
		pdata->usb_path = MUIC_PATH_USB_CP;
	else
		pr_warn("%s invalid value\n", __func__);

	pr_info("%s usb_path(%d)\n", __func__,
			pdata->usb_path);

	return count;
}

static ssize_t max77705_muic_show_uart_en(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!pdata->rustproof_on) {
		pr_info("%s UART ENABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s UART DISABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77705_muic_set_uart_en(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "1", 1))
		pdata->rustproof_on = false;
	else if (!strncasecmp(buf, "0", 1))
		pdata->rustproof_on = true;
	else
		pr_warn("%s invalid value\n", __func__);

	pr_info("%s uart_en(%d)\n", __func__,
			!pdata->rustproof_on);

	return count;
}

static ssize_t max77705_muic_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	u8 adc;

	adc = max77705_muic_get_adc_value(muic_data);
	pr_info("%s adc(0x%02x)\n", __func__, adc);

	if (adc == MAX77705_UIADC_ERROR) {
		pr_err("%s fail to read adc value\n",
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	switch (adc) {
	case MAX77705_UIADC_GND:
		adc = 0;
		break;
	case MAX77705_UIADC_255K:
		adc = 0x18;
		break;
	case MAX77705_UIADC_301K:
		adc = 0x19;
		break;
	case MAX77705_UIADC_523K:
		adc = 0x1c;
		break;
	case MAX77705_UIADC_619K:
		adc = 0x1d;
		break;
	case MAX77705_UIADC_OPEN:
		adc = 0x1f;
		break;
	default:
		adc = 0xff;
	}

	return sprintf(buf, "adc: 0x%x\n", adc);
}

static ssize_t max77705_muic_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);

	pr_debug("%s attached_dev(%d)\n", __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t max77705_muic_show_attached_dev(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	const struct max77705_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev, muic_data);
	if (vps_index < 0)
		return sprintf(buf, "No VPS\n");

	tmp_vps = &(muic_vps_table[vps_index]);

	return sprintf(buf, "%s\n", tmp_vps->vps_name);
}

static ssize_t max77705_muic_show_otg_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = -ENODEV;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		if (muic_data->is_otg_test == true)
			ret = 0;
		else
			ret = 1;
		pr_info("%s ret:%d buf:%s\n", __func__, ret, buf);
	}

	return ret;
}

static ssize_t max77705_muic_set_otg_test(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = -ENODEV;

	mutex_lock(&muic_data->muic_mutex);

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		pr_info("%s buf:%s\n", __func__, buf);
		if (!strncmp(buf, "0", 1)) {
			muic_data->is_otg_test = true;
			ret = 0;
		} else if (!strncmp(buf, "1", 1)) {
			muic_data->is_otg_test = false;
			ret = 1;
		} else {
			pr_warn("%s Wrong command\n", __func__);
			mutex_unlock(&muic_data->muic_mutex);
			return count;
		}

		pr_info("%s ret: %d\n", __func__, ret);

		mutex_unlock(&muic_data->muic_mutex);
		return count;
	}

	mutex_unlock(&muic_data->muic_mutex);
	return ret;
}

static ssize_t max77705_muic_show_apo_factory(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s apo factory=%s\n", __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t max77705_muic_set_apo_factory(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
#if defined(CONFIG_SEC_FACTORY)
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
#endif /* CONFIG_SEC_FACTORY */
	const char *mode;

	pr_info("%s buf:%s\n", __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
#if defined(CONFIG_SEC_FACTORY)
		muic_data->is_factory_start = true;
#endif /* CONFIG_SEC_FACTORY */
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s Wrong command\n", __func__);
		return count;
	}

	pr_info("%s apo factory=%s\n", __func__, mode);

	return count;
}

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
static ssize_t max77705_muic_show_afc_disable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (pdata->afc_disable) {
		pr_info("%s AFC DISABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s AFC ENABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77705_muic_set_afc_disable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret;

	if (!strncasecmp(buf, "1", 1)) {
		ret = sec_set_param(CM_OFFSET + 1, '1');
		if (ret < 0)
			pr_err("%s:sec_set_param failed\n", __func__);
		else
			pdata->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		ret = sec_set_param(CM_OFFSET + 1, '0');
		if (ret < 0)
			pr_err("%s:sec_set_param failed\n", __func__);
		else
			pdata->afc_disable = false;
	} else {
		pr_warn("%s invalid value\n", __func__);
	}
#if defined(CONFIG_SEC_FACTORY)
	/* for factory self charging test (AFC-> NORMAL TA) */
	if (muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_9V_MUIC)
		max77705_muic_afc_hv_set(muic_data, 5);
#endif
	pr_info("%s afc_disable(%d)\n", __func__, pdata->afc_disable);

	return count;
}
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */

static ssize_t max77705_muic_show_vbus_value(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77705_muic_data *muic_data = dev_get_drvdata(dev);
	u8 vbadc;

	vbadc = max77705_muic_get_vbadc_value(muic_data);
	pr_info("%s vbadc(0x%02x)\n", __func__, vbadc);

	switch (vbadc) {
	case MAX77705_VBADC_3_8V_UNDER:
		vbadc = 0;
		break;
	case MAX77705_VBADC_3_8V_TO_4_5V ... MAX77705_VBADC_6_5V_TO_7_5V:
		vbadc = 5;
		break;
	case MAX77705_VBADC_7_5V_TO_8_5V ... MAX77705_VBADC_8_5V_TO_9_5V:
		vbadc = 9;
		break;
	default:
		vbadc += 3;
	}

	return sprintf(buf, "%d\n", vbadc);
}

static DEVICE_ATTR(uart_sel, 0664, max77705_muic_show_uart_sel,
		max77705_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, max77705_muic_show_usb_sel,
		max77705_muic_set_usb_sel);
static DEVICE_ATTR(uart_en, 0660, max77705_muic_show_uart_en,
		max77705_muic_set_uart_en);
static DEVICE_ATTR(adc, 0444, max77705_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0444, max77705_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, 0444, max77705_muic_show_attached_dev, NULL);
static DEVICE_ATTR(otg_test, 0664,
		max77705_muic_show_otg_test, max77705_muic_set_otg_test);
static DEVICE_ATTR(apo_factory, 0664,
		max77705_muic_show_apo_factory, max77705_muic_set_apo_factory);
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
static DEVICE_ATTR(afc_disable, 0664,
		max77705_muic_show_afc_disable, max77705_muic_set_afc_disable);
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */
static DEVICE_ATTR(vbus_value, 0444, max77705_muic_show_vbus_value, NULL);
static DEVICE_ATTR(vbus_value_pd, 0444, max77705_muic_show_vbus_value, NULL);

static struct attribute *max77705_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_en.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
	&dev_attr_otg_test.attr,
	&dev_attr_apo_factory.attr,
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	&dev_attr_afc_disable.attr,
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */
	&dev_attr_vbus_value.attr,
	&dev_attr_vbus_value_pd.attr,
	NULL
};

static const struct attribute_group max77705_muic_group = {
	.attrs = max77705_muic_attributes,
};

void max77705_muic_read_register(struct i2c_client *i2c)
{
	const enum max77705_usbc_reg regfile[] = {
		MAX77705_USBC_REG_UIC_HW_REV,
		MAX77705_USBC_REG_USBC_STATUS1,
		MAX77705_USBC_REG_USBC_STATUS2,
		MAX77705_USBC_REG_BC_STATUS,
		MAX77705_USBC_REG_UIC_INT_M,
	};
	u8 val;
	int i, ret;

	pr_info("%s read register--------------\n", __func__);
	for (i = 0; i < ARRAY_SIZE(regfile); i++) {
		ret = max77705_muic_read_reg(i2c, regfile[i], &val);
		if (ret) {
			pr_err("%s fail to read muic reg(0x%02x), ret=%d\n",
					__func__, regfile[i], ret);
			continue;
		}

		pr_info("%s reg(0x%02x)=[0x%02x]\n",
			__func__, regfile[i], val);
	}
	pr_info("%s end register---------------\n", __func__);
}

static int max77705_muic_attach_uart_path(struct max77705_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data, new_dev);
	else if (pdata->uart_path == MUIC_PATH_UART_CP)
		ret = switch_to_cp_uart(muic_data, new_dev);
	else
		pr_warn("%s invalid uart_path\n", __func__);

	return ret;
}

static int max77705_muic_attach_usb_path(struct max77705_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s usb_path=%d\n", __func__, pdata->usb_path);

	if (pdata->usb_path == MUIC_PATH_USB_AP)
		ret = com_to_usb_ap(muic_data);
	else if (pdata->usb_path == MUIC_PATH_USB_CP)
		ret = com_to_usb_cp(muic_data);
	else
		pr_warn("%s invalid usb_path\n", __func__);

	return ret;
}

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
static void max77705_muic_disable_afc_protocol(struct max77705_muic_data *muic_data)
{
	struct i2c_client *pmic_i2c = muic_data->usbc_pdata->max77705->i2c;
	struct i2c_client *debug_i2c = muic_data->usbc_pdata->max77705->debug;

	pr_info("%s\n", __func__);

	/*
	 * MAXIM's request.
	 * This is workaround of D- high during AFC charging issue,
	 * set hidden register.
	 */
	max77705_write_reg(pmic_i2c, 0xFE, 0xC5);
	max77705_write_reg(debug_i2c, 0xB3, 0x3C);
	max77705_write_reg(debug_i2c, 0x0F, 0x04);
	max77705_write_reg(debug_i2c, 0xB3, 0x00);
	max77705_write_reg(pmic_i2c, 0xFE, 0x00);
}
#endif

static int max77705_muic_handle_detach(struct max77705_muic_data *muic_data, int irq)
{
	int ret = 0;
#if defined(CONFIG_MUIC_NOTIFIER)
	bool noti = true;
#endif /* CONFIG_MUIC_NOTIFIER */

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s Duplicated(%d), just ignore\n",
				__func__, muic_data->attached_dev);
		goto out_without_noti;
	}

#if 0
	/* Enable Charger Detection */
	max77705_muic_enable_chgdet(muic_data);
#endif

	muic_lookup_vps_table(muic_data->attached_dev, muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
		break;
	case ATTACHED_DEV_UNOFFICIAL_ID_MUIC:
		goto out_without_noti;
	default:
		break;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	if (muic_data->is_check_hv) {
		muic_data->is_check_hv = false;
		max77705_muic_disable_afc_protocol(muic_data);
	}
#endif
	com_to_open(muic_data);

out_without_noti:
	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int max77705_muic_logically_detach(struct max77705_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	bool noti = true;
#endif /* CONFIG_MUIC_NOTIFIER */
	bool force_path_open = true;
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_ON_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_ON_VB_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
		/* TODO: do something if necessary */
#endif
		break;
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		break;
	case ATTACHED_DEV_NONE_MUIC:
		force_path_open = false;
		goto out;
	default:
		pr_warn("%s try to attach without logically detach\n",
				__func__);
		goto out;
	}

	pr_info("%s attached(%d)!=new(%d), assume detach\n",
			__func__, muic_data->attached_dev, new_dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

out:
	if (force_path_open)
		com_to_open(muic_data);

	return ret;
}

static int max77705_muic_handle_attach(struct max77705_muic_data *muic_data,
		muic_attached_dev_t new_dev, int irq)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	bool logically_notify = false;
#endif /* CONFIG_MUIC_NOTIFIER */
#if defined(CONFIG_CCIC_MAX77705)
	int fw_update_state = muic_data->usbc_pdata->max77705->fw_update_state;
#endif /* CONFIG_CCIC_MAX77705 */
	int ret = 0;

	pr_info("%s\n", __func__);

	if (new_dev == muic_data->attached_dev) {
		pr_info("%s Duplicated(%d), just ignore\n",
				__func__, muic_data->attached_dev);
		return ret;
	}

	ret = max77705_muic_logically_detach(muic_data, new_dev);
	if (ret)
		pr_warn("%s fail to logically detach(%d)\n",
				__func__, ret);

	switch (new_dev) {
	case ATTACHED_DEV_OTG_MUIC:
		ret = com_to_usb_ap(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		ret = max77705_muic_attach_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		ret = max77705_muic_attach_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = max77705_muic_attach_usb_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
#if defined(CONFIG_CCIC_MAX77705)
		if (fw_update_state == FW_UPDATE_END && irq == MUIC_IRQ_INIT_DETECT) {
			pr_info("%s: FW Update dcd timeout, chgdet rerun\n", __func__);
			max77705_muic_enable_chgdet(muic_data);
			goto out;
		}
#endif /* CONFIG_CCIC_MAX77705 */
		ret = max77705_muic_attach_usb_path(muic_data, new_dev);
		break;
	default:
		pr_warn("%s unsupported dev(%d)\n", __func__,
				new_dev);
		ret = -ENODEV;
		goto out;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (logically_notify) {
		pr_info("%s: noti\n", __func__);
	} else {
		muic_notifier_attach_attached_dev(new_dev);
		muic_data->attached_dev = new_dev;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

	muic_data->attached_dev = new_dev;

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	if (max77705_muic_check_is_enable_afc(muic_data, new_dev)) {
		/* Maxim's request, wait 30ms for checking HVDCP */
		pr_info("%s afc work after 30ms\n", __func__);
		cancel_delayed_work_sync(&(muic_data->afc_work));
		schedule_delayed_work(&(muic_data->afc_work), msecs_to_jiffies(30));
	}
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */

out:
	return ret;
}

static bool muic_check_vps_adc
			(const struct max77705_muic_vps_data *tmp_vps, u8 adc)
{
	bool ret = false;

	if (tmp_vps->adc == adc) {
		ret = true;
		goto out;
	}

	if (tmp_vps->adc == MAX77705_UIADC_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s vps(%s) adc(0x%02x) ret(%c)\n",
				__func__, tmp_vps->vps_name,
				adc, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_vbvolt(const struct max77705_muic_vps_data *tmp_vps,
	u8 vbvolt)
{
	bool ret = false;

	if (tmp_vps->vbvolt == vbvolt) {
		ret = true;
		goto out;
	}

	if (tmp_vps->vbvolt == VB_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_debug("%s vps(%s) vbvolt(0x%02x) ret(%c)\n",
				__func__, tmp_vps->vps_name,
				vbvolt, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_chgtyp(const struct max77705_muic_vps_data *tmp_vps,
	u8 chgtyp)
{
	bool ret = false;

	if (tmp_vps->chgtyp == chgtyp) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgtyp == CHGTYP_ANY) {
		if (chgtyp > CHGTYP_NO_VOLTAGE) {
			ret = true;
			goto out;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s vps(%s) chgtyp(0x%02x) ret(%c)\n",
				__func__, tmp_vps->vps_name,
				chgtyp, ret ? 'T' : 'F');
	}

	return ret;
}

#if defined(CONFIG_MUIC_MAX77705_CCIC)
static u8 max77705_muic_update_adc_with_rid(struct max77705_muic_data *muic_data,
	u8 adc)
{
	u8 new_adc = adc;

	if (muic_data->pdata->opmode & OPMODE_CCIC) {
		switch (muic_data->ccic_info_data.ccic_evt_rid) {
		case RID_000K:
			new_adc = MAX77705_UIADC_GND;
			break;
		case RID_255K:
			new_adc = MAX77705_UIADC_255K;
			break;
		case RID_301K:
			new_adc = MAX77705_UIADC_301K;
			break;
		case RID_523K:
			new_adc = MAX77705_UIADC_523K;
			break;
		case RID_619K:
			new_adc = MAX77705_UIADC_619K;
			break;
		default:
			new_adc = MAX77705_UIADC_OPEN;
			break;
		}

		if (muic_data->ccic_info_data.ccic_evt_rprd)
			new_adc = MAX77705_UIADC_GND;

		pr_info("%s: adc(0x%x->0x%x) rid(%d) rprd(%d)\n",
			__func__, adc, new_adc,
			muic_data->ccic_info_data.ccic_evt_rid,
			muic_data->ccic_info_data.ccic_evt_rprd);
	}

	return new_adc;
}
#endif /* CONFIG_MUIC_MAX77705_CCIC */

static u8 max77705_resolve_chgtyp(u8 chgtyp, u8 spchgtyp, u8 dcdtmo)
{
	u8 ret = chgtyp;

	/* Check DCD timeout first */
	if (dcdtmo && chgtyp == CHGTYP_USB) {
		ret = CHGTYP_TIMEOUT_OPEN;
		goto out;
	}

	/* Check Special chgtyp second */
	switch (spchgtyp) {
	case PRCHGTYP_SAMSUNG_2A:
	case PRCHGTYP_APPLE_500MA:
	case PRCHGTYP_APPLE_1A:
	case PRCHGTYP_APPLE_2A:
	case PRCHGTYP_APPLE_12W:
		if (chgtyp == CHGTYP_USB || chgtyp == CHGTYP_CDP) {
			ret = CHGTYP_UNOFFICIAL_CHARGER;
			goto out;
		}
		break;
	default:
		break;
	}

out:
	if (ret != chgtyp)
		pr_info("%s chgtyp(0x%x) spchgtyp(0x%x) dcdtmo(0x%x) -> chgtyp(0x%x)",
				__func__, chgtyp, spchgtyp, dcdtmo, ret);

	return ret;
}

muic_attached_dev_t max77705_muic_check_new_dev(struct max77705_muic_data *muic_data,
	int *intr)
{
	const struct max77705_muic_vps_data *tmp_vps;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	u8 adc = muic_data->status1 & USBC_STATUS1_UIADC_MASK;
	u8 vbvolt = muic_data->status3 & BC_STATUS_VBUSDET_MASK;
	u8 chgtyp = muic_data->status3 & BC_STATUS_CHGTYP_MASK;
	u8 spchgtyp = (muic_data->status3 & BC_STATUS_PRCHGTYP_MASK) >> BC_STATUS_PRCHGTYP_SHIFT;
	u8 dcdtmo = (muic_data->status3 & BC_STATUS_DCDTMO_MASK) >> BC_STATUS_DCDTMO_SHIFT;
	unsigned long i;

	chgtyp = max77705_resolve_chgtyp(chgtyp, spchgtyp, dcdtmo);

#if defined(CONFIG_MUIC_MAX77705_CCIC)
	adc = max77705_muic_update_adc_with_rid(muic_data, adc);
	/* Do not check vbus if CCIC RID is 523K */
	if ((muic_data->pdata->opmode & OPMODE_CCIC) && (adc == MAX77705_UIADC_523K))
		vbvolt = 0;
#endif /* CONFIG_MUIC_MAX77705_CCIC */

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (!(muic_check_vps_adc(tmp_vps, adc)))
			continue;

		if (!(muic_check_vps_vbvolt(tmp_vps, vbvolt)))
			continue;

		if (!(muic_check_vps_chgtyp(tmp_vps, chgtyp)))
			continue;

#if 0 /* TODO: should be modified */
		if (!(muic_check_support_dev(muic_data, tmp_vps->attached_dev)))
			continue;
#endif

		pr_info("%s vps table match found at i(%lu), %s\n",
			__func__, i, tmp_vps->vps_name);

		new_dev = tmp_vps->attached_dev;
		muic_data->switch_val = tmp_vps->muic_switch;

		*intr = MUIC_INTR_ATTACH;
		break;
	}

	pr_info("%s %d->%d switch_val[0x%02x]\n", __func__,
		muic_data->attached_dev, new_dev, muic_data->switch_val);

	return new_dev;
}

static void max77705_muic_detect_dev(struct max77705_muic_data *muic_data,
	int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 status[3];
	u8 adc, vbvolt, chgtyp, spchgtyp, sysmsg, vbadc, dcdtmo;
	int ret;

	ret = max77705_bulk_read(i2c,
		MAX77705_USBC_REG_USBC_STATUS1, 3, status);
	if (ret) {
		pr_err("%s fail to read muic reg(%d)\n",
				__func__, ret);
		return;
	}

	pr_info("%s USBC1:0x%02x, USBC2:0x%02x, BC:0x%02x\n",
			__func__, status[0], status[1], status[2]);

	/* attached status */
	muic_data->status1 = status[0];
	muic_data->status2 = status[1];
	muic_data->status3 = status[2];

	adc = status[0] & USBC_STATUS1_UIADC_MASK;
	sysmsg = status[1] & USBC_STATUS2_SYSMSG_MASK;
	vbvolt = (status[2] & BC_STATUS_VBUSDET_MASK) >> BC_STATUS_VBUSDET_SHIFT;
	chgtyp = status[2] & BC_STATUS_CHGTYP_MASK;
	spchgtyp = (status[2] & BC_STATUS_PRCHGTYP_MASK) >> BC_STATUS_PRCHGTYP_SHIFT;
	vbadc = (status[0] & USBC_STATUS1_VBADC_MASK) >> USBC_STATUS1_VBADC_SHIFT;
	dcdtmo = (status[2] & BC_STATUS_DCDTMO_MASK) >> BC_STATUS_DCDTMO_SHIFT;

	pr_info("%s adc:0x%x vbvolt:0x%x chgtyp:0x%x spchgtyp:0x%x sysmsg:0x%x vbadc:0x%x dcdtmo:0x%x\n",
		__func__, adc, vbvolt, chgtyp, spchgtyp, sysmsg, vbadc, dcdtmo);

	/* Set the fake_vbus charger type */
	muic_data->fake_chgtyp = chgtyp;

	if (irq == muic_data->irq_vbadc) {
		pr_info("%s vbadc irq(%d), return\n",
				__func__, muic_data->irq_vbadc);
		return;
	}

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	if (!muic_data->is_factory_start) {
		if ((irq == MUIC_IRQ_CCIC_HANDLER) &&
				(muic_data->ccic_evt_id == CCIC_NOTIFY_ID_WATER)) {
			/* Disable BC1.2 at water state */
			if (muic_data->afc_water_disable)
				max77705_muic_disable_chgdet(muic_data);
			else
				max77705_muic_enable_chgdet(muic_data);
		}

		if (muic_data->afc_water_disable) {
			pr_info("%s watar hiccup mode, Aux USB path\n", __func__);
			com_to_usb_cp(muic_data);
			max77705_muic_handle_vbus(muic_data);

			return;
		}
	}
#endif

	new_dev = max77705_muic_check_new_dev(muic_data, &intr);

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s ATTACHED\n", __func__);

		ret = max77705_muic_handle_attach(muic_data, new_dev, irq);
		if (ret)
			pr_err("%s cannot handle attach(%d)\n", __func__, ret);
	} else {
		pr_info("%s DETACHED\n", __func__);

		if (vbvolt == 0 && chgtyp == CHGTYP_DEDICATED_CHARGER)
			pr_info("%s catch the Fake Vbus type\n", __func__);

		ret = max77705_muic_handle_detach(muic_data, irq);
		if (ret)
			pr_err("%s cannot handle detach(%d)\n", __func__, ret);
	}

	max77705_muic_handle_vbus(muic_data);
}

static irqreturn_t max77705_muic_irq(int irq, void *data)
{
	struct max77705_muic_data *muic_data = data;
	struct irq_desc *desc = irq_to_desc(irq);

	pr_info("%s irq:%d (%s)\n", __func__, irq, desc->action->name);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77705_muic_detect_dev(muic_data, irq);
	else
		pr_info("%s MUIC is not ready, just return\n", __func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
static void max77705_muic_afc_work(struct work_struct *work)
{
	struct max77705_muic_data *muic_data =
		container_of(work, struct max77705_muic_data, afc_work.work);

	pr_info("%s\n", __func__);

	if (max77705_muic_check_is_enable_afc(muic_data, muic_data->attached_dev)) {
		muic_data->is_check_hv = true;
		max77705_muic_afc_hv_set(muic_data, 9);
	}
}

static int max77705_muic_afc_set_voltage(int voltage)
{
	struct max77705_muic_data *muic_data = g_muic_data;
	int now_voltage = 0;

	switch (voltage) {
	case 5:
	case 9:
		break;
	default:
		pr_err("%s: invalid value %d, return\n", __func__, voltage);
		return -EINVAL;
	}

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		now_voltage = 5;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		now_voltage = 9;
		break;
	default:
		break;
	}

	if (voltage == now_voltage) {
		pr_err("%s: same with current voltage, return\n", __func__);
		return -EINVAL;
	}

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		max77705_muic_afc_hv_set(muic_data, voltage);
		break;
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		max77705_muic_qc_hv_set(muic_data, voltage);
		break;
	default:
		pr_err("%s: not a HV Charger %d, return\n", __func__, muic_data->attached_dev);
		return -EINVAL;
	}

	return 0;
}

static int max77705_muic_hv_charger_init(void)
{
	struct max77705_muic_data *muic_data = g_muic_data;

	if (muic_data->is_charger_ready) {
		pr_info("%s: charger is already ready(%d), return\n",
				__func__, muic_data->is_charger_ready);
		return -EINVAL;
	}

	muic_data->is_charger_ready = true;

	if (max77705_muic_check_is_enable_afc(muic_data, muic_data->attached_dev)) {
		pr_info("%s afc work start\n", __func__);
		cancel_delayed_work_sync(&(muic_data->afc_work));
		schedule_delayed_work(&(muic_data->afc_work), msecs_to_jiffies(0));
	}

	return 0;
}

static void max77705_muic_detect_dev_hv_work(struct work_struct *work)
{
	struct max77705_muic_data *muic_data = container_of(work,
		struct max77705_muic_data, afc_handle_work);
	unsigned char opcode = muic_data->afc_op_dataout[0];

	mutex_lock(&muic_data->muic_mutex);
	if (!max77705_muic_check_is_enable_afc(muic_data, muic_data->attached_dev)) {
		switch (muic_data->attached_dev) {
		case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
			pr_info("%s high voltage value is changed\n", __func__);
			break;
		default:
			pr_info("%s status is changed, return\n", __func__);
			goto out;
		}
	}

	if (opcode == COMMAND_AFC_RESULT_READ)
		max77705_muic_handle_detect_dev_afc(muic_data, muic_data->afc_op_dataout);
	else if (opcode == COMMAND_QC_2_0_SET)
		max77705_muic_handle_detect_dev_qc(muic_data, muic_data->afc_op_dataout);
	else
		pr_info("%s undefined opcode(%d\n)", __func__, opcode);

out:
	mutex_unlock(&muic_data->muic_mutex);
}

void max77705_muic_handle_detect_dev_hv(struct max77705_muic_data *muic_data, unsigned char *data)
{
	int i;

	for (i = 0; i < AFC_OP_OUT_LEN; i++)
		muic_data->afc_op_dataout[i] = data[i];

	schedule_work(&(muic_data->afc_handle_work));
}
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */

static void max77705_muic_print_reg_log(struct work_struct *work)
{
	struct max77705_muic_data *muic_data =
		container_of(work, struct max77705_muic_data, debug_work.work);
	struct i2c_client *i2c = muic_data->i2c;
	struct i2c_client *pmic_i2c = muic_data->usbc_pdata->i2c;
	u8 status[12] = {0, };

	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_USBC_STATUS1, &status[0]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_USBC_STATUS2, &status[1]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_BC_STATUS, &status[2]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_CC_STATUS0, &status[3]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_CC_STATUS1, &status[4]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_PD_STATUS0, &status[5]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_PD_STATUS1, &status[6]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_UIC_INT_M, &status[7]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_CC_INT_M, &status[8]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_PD_INT_M, &status[9]);
	max77705_muic_read_reg(i2c, MAX77705_USBC_REG_VDM_INT_M, &status[10]);
	max77705_muic_read_reg(pmic_i2c, MAX77705_PMIC_REG_INTSRC_MASK, &status[11]);

	pr_info("%s USBC1:0x%02x, USBC2:0x%02x, BC:0x%02x, CC0:0x%x, CC1:0x%x, PD0:0x%x, PD1:0x%x attached_dev:%d\n",
		__func__, status[0], status[1], status[2], status[3], status[4], status[5], status[6],
		muic_data->attached_dev);
	pr_info("%s UIC_INT_M:0x%x, CC_INT_M:0x%x, PD_INT_M:0x%x, VDM_INT_M:0x%x, PMIC_MASK:0x%x, WDT_CNT:%d\n",
		__func__, status[7], status[8], status[9], status[10], status[11],
		muic_data->usbc_pdata->watchdog_count);

	schedule_delayed_work(&(muic_data->debug_work),
		msecs_to_jiffies(60000));
}

#if defined(CONFIG_MUIC_MAX77705_CCIC)
static void max77705_muic_handle_ccic_event(struct work_struct *work)
{
	struct max77705_muic_data *muic_data = container_of(work,
		struct max77705_muic_data, ccic_info_data_work);

	pr_info("%s\n", __func__);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77705_muic_detect_dev(muic_data, MUIC_IRQ_CCIC_HANDLER);
	else
		pr_info("%s MUIC is not ready, just return\n", __func__);
	mutex_unlock(&muic_data->muic_mutex);
}
#endif /* CONFIG_MUIC_MAX77705_CCIC */

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77705_muic_irq,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s Failed to request IRQ #%d: %d\n",		\
			__func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

static int max77705_muic_irq_init(struct max77705_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irq_uiadc = irq_base + MAX77705_USBC_IRQ_UIDADC_INT;
		REQUEST_IRQ(muic_data->irq_uiadc, muic_data, "muic-uiadc");

		muic_data->irq_chgtyp = irq_base + MAX77705_USBC_IRQ_CHGT_INT;
		REQUEST_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");

		muic_data->irq_dcdtmo = irq_base + MAX77705_USBC_IRQ_DCD_INT;
		REQUEST_IRQ(muic_data->irq_dcdtmo, muic_data, "muic-dcdtmo");

		muic_data->irq_vbadc = irq_base + MAX77705_USBC_IRQ_VBADC_INT;
		REQUEST_IRQ(muic_data->irq_vbadc, muic_data, "muic-vbadc");

		muic_data->irq_vbusdet = irq_base + MAX77705_USBC_IRQ_VBUS_INT;
		REQUEST_IRQ(muic_data->irq_vbusdet, muic_data, "muic-vbusdet");
	}

	pr_info("%s uiadc(%d), chgtyp(%d), dcdtmo(%d), vbadc(%d), vbusdet(%d)\n",
			__func__, muic_data->irq_uiadc,
			muic_data->irq_chgtyp, muic_data->irq_dcdtmo,
			muic_data->irq_vbadc, muic_data->irq_vbusdet);
	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s IRQ(%d):%s free done\n",	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void max77705_muic_free_irqs(struct max77705_muic_data *muic_data)
{
	pr_info("%s\n", __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irq_uiadc, muic_data, "muic-uiadc");
	FREE_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");
	FREE_IRQ(muic_data->irq_dcdtmo, muic_data, "muic-dcdtmo");
	FREE_IRQ(muic_data->irq_vbadc, muic_data, "muic-vbadc");
	FREE_IRQ(muic_data->irq_vbusdet, muic_data, "muic-vbusdet");
}

static void max77705_muic_init_detect(struct max77705_muic_data *muic_data)
{
	pr_info("%s\n", __func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_muic_ready = true;

	max77705_muic_detect_dev(muic_data, MUIC_IRQ_INIT_DETECT);

	mutex_unlock(&muic_data->muic_mutex);
}

#if defined(CONFIG_OF)
static int of_max77705_muic_dt(struct max77705_muic_data *muic_data)
{
	struct device_node *np_muic;
	const char *prop_support_list;
	int i, j, prop_num;
	int ret = 0;

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	prop_num = of_property_count_strings(np_muic, "muic,support-list");
	if (prop_num < 0) {
		pr_warn("%s Cannot parse 'muic support list dt node'[%d]\n",
				__func__, prop_num);
		ret = prop_num;
		goto err;
	}

	/* for debug */
	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic,
				"muic,support-list", i, &prop_support_list);
		if (ret) {
			pr_err("%s Cannot find string at [%d], ret[%d]\n",
					__func__, i, ret);
			goto err;
		}

		pr_debug("%s prop_support_list[%d] is %s\n", __func__,
				i, prop_support_list);

		for (j = 0; j < ARRAY_SIZE(muic_vps_table); j++) {
			if (!strcmp(muic_vps_table[j].vps_name, prop_support_list)) {
				muic_data->muic_support_list[(muic_vps_table[j].attached_dev)] = true;
				break;
			}
		}
	}

	/* for debug */
	for (i = 0; i < ATTACHED_DEV_NUM; i++)
		pr_debug("%s pmuic_support_list[%d] = %c\n", __func__,
			i, (muic_data->muic_support_list[i] ? 'T' : 'F'));

err:
	of_node_put(np_muic);

	return ret;
}
#endif /* CONFIG_OF */

static void max77705_muic_clear_interrupt(struct max77705_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 interrupt;
	int ret;

	pr_info("%s\n", __func__);

	ret = max77705_muic_read_reg(i2c,
			MAX77705_USBC_REG_UIC_INT, &interrupt);
	if (ret)
		pr_err("%s fail to read muic INT1 reg(%d)\n",
			__func__, ret);

	pr_info("%s CLEAR!! UIC_INT:0x%02x\n",
		__func__, interrupt);
}

static int max77705_muic_init_regs(struct max77705_muic_data *muic_data)
{
	int ret;

	pr_info("%s\n", __func__);

	max77705_muic_clear_interrupt(muic_data);

	ret = max77705_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s Failed to initialize MUIC irq:%d\n",
				__func__, ret);
		max77705_muic_free_irqs(muic_data);
	}

	return ret;
}

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
static int muic_handle_usb_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct max77705_muic_data *pmuic =
		container_of(nb, struct max77705_muic_data, usb_nb);

	switch (action) {
	/* Abnormal device */
	case EXTERNAL_NOTIFY_3S_NODEVICE:
		pr_info("%s: 3S_NODEVICE(USB HOST Connection timeout)\n",
				__func__);
		if (pmuic->attached_dev == ATTACHED_DEV_HMT_MUIC)
			muic_send_dock_intent(MUIC_DOCK_ABNORMAL);
		break;

	/* Gamepad device connected */
	case EXTERNAL_NOTIFY_DEVICE_CONNECT:
		pr_info("%s: DEVICE_CONNECT(Gamepad)\n", __func__);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}


static void muic_register_usb_notifier(struct max77705_muic_data *pmuic)
{
	int ret = 0;

	pr_info("%s\n", __func__);


	ret = usb_external_notify_register(&pmuic->usb_nb,
		muic_handle_usb_notification, EXTERNAL_NOTIFY_DEV_MUIC);
	if (ret < 0) {
		pr_info("%s: USB Noti. is not ready.\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}

static void muic_unregister_usb_notifier(struct max77705_muic_data *pmuic)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = usb_external_notify_unregister(&pmuic->usb_nb);
	if (ret < 0) {
		pr_info("%s: USB Noti. unregister error.\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}
#else
static void muic_register_usb_notifier(struct max77705_muic_data *pmuic) {}
static void muic_unregister_usb_notifier(struct max77705_muic_data *pmuic) {}
#endif

int max77705_muic_probe(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_platform_data *mfd_pdata = usbc_data->max77705_data;
	struct max77705_muic_data *muic_data;
	int ret = 0;

	pr_info("%s\n", __func__);

	muic_data = kzalloc(sizeof(struct max77705_muic_data), GFP_KERNEL);
	if (!muic_data) {
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}

#if defined(CONFIG_OF)
	ret = of_max77705_muic_dt(muic_data);
	if (ret < 0)
		pr_err("%s not found muic dt! ret[%d]\n", __func__, ret);
#endif /* CONFIG_OF */

	mutex_init(&muic_data->muic_mutex);
	muic_data->i2c = usbc_data->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->usbc_pdata = usbc_data;
	muic_data->pdata = &muic_pdata;
	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
	muic_data->is_muic_ready = false;
	muic_data->is_otg_test = false;
	muic_data->is_factory_start = false;
	muic_data->switch_val = COM_OPEN;

	usbc_data->muic_data = muic_data;
	g_muic_data = muic_data;

	if (muic_data->pdata->init_gpio_cb) {
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
		if (ret) {
			pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
			goto fail;
		}
	}

	mutex_lock(&muic_data->muic_mutex);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &max77705_muic_group);
	if (ret) {
		pr_err("%s: failed to create attribute group\n",
				__func__);
		goto fail_sysfs_create;
	}
	dev_set_drvdata(switch_device, muic_data);

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = max77705_muic_init_regs(muic_data);
	if (ret < 0) {
		pr_err("%s Failed to initialize MUIC irq:%d\n",
				__func__, ret);
		goto fail_init_irq;
	}

	mutex_unlock(&muic_data->muic_mutex);

#if defined(CONFIG_MUIC_MAX77705_CCIC)
	muic_data->pdata->opmode = get_ccic_info() & 0x0f;
	if (muic_data->pdata->opmode & OPMODE_CCIC) {
		max77705_muic_register_ccic_notifier(muic_data);
		INIT_WORK(&(muic_data->ccic_info_data_work),
			max77705_muic_handle_ccic_event);
	}

	muic_data->afc_water_disable = false;
#endif /* CONFIG_MUIC_MAX77705_CCIC */

#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	if (get_afc_mode() == CH_MODE_AFC_DISABLE_VAL) {
		pr_info("%s: afc_disable: AFC disabled\n", __func__);
		muic_data->pdata->afc_disable = true;
	} else {
		pr_info("%s: afc_disable: AFC enabled\n", __func__);
		muic_data->pdata->afc_disable = false;
	}

	INIT_DELAYED_WORK(&(muic_data->afc_work),
		max77705_muic_afc_work);
	INIT_WORK(&(muic_data->afc_handle_work),
		max77705_muic_detect_dev_hv_work);

	/* set MUIC afc voltage switching function */
	muic_data->pdata->muic_afc_set_voltage_cb = max77705_muic_afc_set_voltage;

	/* set MUIC check charger init function */
	muic_data->pdata->muic_hv_charger_init_cb = max77705_muic_hv_charger_init;
	muic_data->is_charger_ready = false;
	muic_data->is_check_hv = false;
#endif /* CONFIG_HV_MUIC_MAX77705_AFC */

	/* initial cable detection */
	max77705_muic_init_detect(muic_data);

	/* register usb external notifier */
	muic_register_usb_notifier(muic_data);

	INIT_DELAYED_WORK(&(muic_data->debug_work),
		max77705_muic_print_reg_log);
	schedule_delayed_work(&(muic_data->debug_work),
		msecs_to_jiffies(10000));

	return 0;

fail_init_irq:
	if (muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();
	sysfs_remove_group(&switch_device->kobj, &max77705_muic_group);
fail_sysfs_create:
	mutex_unlock(&muic_data->muic_mutex);
fail:
	mutex_destroy(&muic_data->muic_mutex);
err_kfree:
	kfree(muic_data);
err_return:
	return ret;
}

int max77705_muic_remove(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_muic_data *muic_data = usbc_data->muic_data;

	sysfs_remove_group(&switch_device->kobj, &max77705_muic_group);

	if (muic_data) {
		pr_info("%s\n", __func__);

		max77705_muic_free_irqs(muic_data);

		if (muic_data->pdata->cleanup_switch_dev_cb)
			muic_data->pdata->cleanup_switch_dev_cb();

#if defined(CONFIG_MUIC_MAX77705_CCIC)
		if (muic_data->pdata->opmode & OPMODE_CCIC) {
			max77705_muic_unregister_ccic_notifier(muic_data);
			cancel_work_sync(&(muic_data->ccic_info_data_work));
		}
#endif
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
		cancel_delayed_work_sync(&(muic_data->afc_work));
		cancel_work_sync(&(muic_data->afc_handle_work));
#endif

		muic_unregister_usb_notifier(muic_data);
		cancel_delayed_work_sync(&(muic_data->debug_work));

		mutex_destroy(&muic_data->muic_mutex);
		kfree(muic_data);
	}

	return 0;
}

void max77705_muic_shutdown(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_muic_data *muic_data = usbc_data->muic_data;
	struct i2c_client *i2c;

	pr_info("%s +\n", __func__);

	sysfs_remove_group(&switch_device->kobj, &max77705_muic_group);

	if (!muic_data) {
		pr_err("%s no drvdata\n", __func__);
		goto out;
	}

	max77705_muic_free_irqs(muic_data);

	i2c = muic_data->i2c;

	if (!i2c) {
		pr_err("%s no muic i2c client\n", __func__);
		goto out_cleanup;
	}

out_cleanup:
	if (muic_data->pdata && muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();

#if defined(CONFIG_MUIC_MAX77705_CCIC)
	if (muic_data->pdata->opmode & OPMODE_CCIC) {
		max77705_muic_unregister_ccic_notifier(muic_data);
		cancel_work_sync(&(muic_data->ccic_info_data_work));
	}
#endif
#if defined(CONFIG_HV_MUIC_MAX77705_AFC)
	cancel_delayed_work_sync(&(muic_data->afc_work));
	cancel_work_sync(&(muic_data->afc_handle_work));
#endif
	muic_unregister_usb_notifier(muic_data);
	cancel_delayed_work_sync(&(muic_data->debug_work));
	mutex_destroy(&muic_data->muic_mutex);
	kfree(muic_data);

out:
	pr_info("%s -\n", __func__);
}

int max77705_muic_suspend(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_muic_data *muic_data = usbc_data->muic_data;

	pr_info("%s\n", __func__);
	cancel_delayed_work(&(muic_data->debug_work));

	return 0;
}

int max77705_muic_resume(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_muic_data *muic_data = usbc_data->muic_data;

	pr_info("%s\n", __func__);
	schedule_delayed_work(&(muic_data->debug_work), msecs_to_jiffies(1000));

	return 0;
}
