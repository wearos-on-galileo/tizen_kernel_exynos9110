/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-cis-2l3.h"
#include "fimc-is-cis-2l3-setA.h"
#include "fimc-is-cis-2l3-setB.h"
#include "fimc-is-cis-2l3-setC.h"

#include "fimc-is-helper-i2c.h"

#define SENSOR_NAME "S5K2L3"
/* #define DEBUG_2L3_PLL */

static const u32 *sensor_2l3_global;
static u32 sensor_2l3_global_size;
static const u32 **sensor_2l3_setfiles;
static const u32 *sensor_2l3_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_2l3_pllinfos;
static u32 sensor_2l3_max_setfile_num;
#ifdef CONFIG_SENSOR_RETENTION_USE
static const u32 **sensor_2l3_retention;
static const u32 *sensor_2l3_retention_size;
static u32 sensor_2l3_max_retention_num;
#endif

#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
static int sensor_2l3_cis_set_mipi_clock(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}
#endif

#ifdef USE_CAMERA_EMBEDDED_HEADER
#define SENSOR_2L3_PAGE_LENGTH 256
#define SENSOR_2L3_VALID_TAG 0x5A
#define SENSOR_2L3_FRAME_ID_PAGE 1
#define SENSOR_2L3_FRAME_ID_OFFSET 190
#define SENSOR_2L3_FLL_MSB_PAGE 1
#define SENSOR_2L3_FLL_MSB_OFFSET 48
#define SENSOR_2L3_FLL_LSB_PAGE 1
#define SENSOR_2L3_FLL_LSB_OFFSET 50
#define SENSOR_2L3_FRAME_COUNT_PAGE 0
#define SENSOR_2L3_FRAME_COUNT_OFFSET 16

#ifdef USE_CAMERA_SSM_TEST
static int record_status;
#endif
static u32 frame_id_idx = (SENSOR_2L3_PAGE_LENGTH * SENSOR_2L3_FRAME_ID_PAGE) + SENSOR_2L3_FRAME_ID_OFFSET;
static u32 fll_msb_idx = (SENSOR_2L3_PAGE_LENGTH * SENSOR_2L3_FLL_MSB_PAGE) + SENSOR_2L3_FLL_MSB_OFFSET;
static u32 fll_lsb_idx = (SENSOR_2L3_PAGE_LENGTH * SENSOR_2L3_FLL_LSB_PAGE) + SENSOR_2L3_FLL_LSB_OFFSET;
static u32 frame_count_idx = (SENSOR_2L3_PAGE_LENGTH * SENSOR_2L3_FRAME_COUNT_PAGE) + SENSOR_2L3_FRAME_COUNT_OFFSET;

static int sensor_2l3_cis_get_frame_id(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (embedded_buf[frame_id_idx-1] == SENSOR_2L3_VALID_TAG) {
		*frame_id = embedded_buf[frame_id_idx];

		dbg_sensor(1, "%s - frame_count(%d)", __func__, embedded_buf[frame_count_idx]);
		dbg_sensor(1, "%s - frame_id(%d)", __func__, *frame_id);
		dbg_sensor(1, "%s - frame length line(%x)",
				__func__, ((embedded_buf[fll_msb_idx]<<8)|embedded_buf[fll_lsb_idx]));

#ifdef USE_CAMERA_SSM_TEST
		if (embedded_buf[frame_count_idx] == 254) {
			switch (record_status) {
			case 0:
				fimc_is_sensor_write8(cis->client, 0x0A52, 0x01);
				record_status++;
				break;
			case 1:
				fimc_is_sensor_write8(cis->client, 0x0A54, 0x01);
				record_status++;
				break;
			case 5:
				fimc_is_sensor_write8(cis->client, 0x0A53, 0x01);
				record_status++;
				break;
			case 10:
				record_status = 0;
				break;
			default:
				record_status++;
				break;
			}
		}
#endif
	} else {
		err("%s : invalid valid tag(%x)", __func__, embedded_buf[frame_id_idx-1]);
		*frame_id = 1;
	}

	return ret;
}
#endif

static void sensor_2l3_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_2L3_4032X3024_30FPS:
		cis_data->max_margin_coarse_integration_time = 0x03;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
#if 0
	case SENSOR_2L3_2016X1134_120FPS:
		cis_data->max_margin_coarse_integration_time = 0x03;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
	case SENSOR_2L3_1008X756_120FPS:
		cis_data->max_margin_coarse_integration_time = 0x03;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
#endif
	default:
		err("[%s] Unsupport 2l3 sensor mode\n", __func__);
		cis_data->max_margin_coarse_integration_time = SENSOR_2L3_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
	}
}

static void sensor_2l3_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
						cis_shared_data *cis_data)
{
	u32 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	WARN_ON(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%d), pclk(%d)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck
					/ (vt_pix_clk_hz / (1000 * 1000)));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif
	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%d) / "
		KERN_CONT "(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info_compact->frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck *
				(u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height *
				(u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Mbps): %d\n", cis_data->pclk / 1000000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_2L3_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_2L3_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_2L3_COARSE_INTEGRATION_TIME_MIN;
}

void sensor_2l3_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode > sensor_2l3_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_2l3_check_rev is fail: ret(%d)", ret);
			return;
		}
	}

	sensor_2l3_cis_data_calculation(sensor_2l3_pllinfos[mode], cis->cis_data);
}

static int sensor_2l3_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	WARN_ON(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

int sensor_2l3_cis_check_rev(struct fimc_is_cis *cis)
{
	int ret = 0;
	u8 rev = 0;
	struct i2c_client *client;

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_read8(client, 0x0002, &rev);
	if (ret < 0) {
		err("fimc_is_sensor_read8 fail, (ret %d)", ret);
		goto p_err;
	}

	cis->cis_data->cis_rev = rev;
	pr_info("%s : Default version 2l3 sensor. Rev. 0x%X\n", __func__, rev);

	switch (rev) {
	case 0xA0:
	case 0xA1: /* EVT0.0 */
		pr_info("%s setfile_A for EVT0.0\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_A_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_A_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_A;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_A_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_A;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_A);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_A_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_A_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_A_retention);
#endif
		break;
	case 0xA2: /* EVT0.1 */
		pr_info("%s setfile_B for EVT0.1\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_B_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_B_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_B;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_B_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_B;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_B);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_B_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_B_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_B_retention);
#endif
		break;
	case 0xA3: /* EVT0.2 */
		pr_info("%s setfile_C for EVT0.2\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_C_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_C_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_C;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_C_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_C;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_C);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_C_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_C_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_C_retention);
#endif
		break;
	default:
		err("Unsupported 2l3 sensor revision(%#x)\n", rev);
		break;
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2l3_cis_set_global_setting_internal(struct v4l2_subdev *subdev);

/* CIS OPS */
int sensor_2l3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
#endif
	setinfo.param = NULL;
	setinfo.return_value = 0;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	WARN_ON(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));
	cis->rev_flag = false;

	ret = sensor_2l3_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
		if (sensor_peri)
			fimc_is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_2l3_check_rev is fail when cis init, ret(%d)", ret);
		cis->rev_flag = true;
		goto p_err;
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_2L3_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_2L3_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->pre_lownoise_mode = FIMC_IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = FIMC_IS_CIS_LNOFF;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
#endif

	sensor_2l3_cis_data_calculation(sensor_2l3_pllinfos[setfile_index], cis->cis_data);
	sensor_2l3_set_integration_max_margin(setfile_index, cis->cis_data);

	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);

	CALL_CISOPS(cis, cis_log_status, subdev);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;
	u64 vt_pix_clk = 0;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	pr_info("[SEN:DUMP] *******************************\n");

	fimc_is_sensor_read16(client, 0x0000, &data16);
	pr_info("[SEN:DUMP] model_id(%x)\n", data16);
	fimc_is_sensor_read8(client, 0x0002, &data8);
	pr_info("[SEN:DUMP] revision_number(%x)\n", data8);
	fimc_is_sensor_read8(client, 0x0005, &data8);
	pr_info("[SEN:DUMP] frame_count(%x)\n", data8);
	fimc_is_sensor_read8(client, 0x0100, &data8);
	pr_info("[SEN:DUMP] mode_select(%x)\n", data8);

	vt_pix_clk = (EXT_CLK_Mhz * 1000 * 1000); /* ext_clk */

	fimc_is_sensor_read16(client, 0x0306, &data16);
	pr_info("[SEN:DUMP] vt_pll_multiplier(%x)\n", data16);
	vt_pix_clk *= data16;

	fimc_is_sensor_read16(client, 0x0304, &data16);
	pr_info("[SEN:DUMP] vt_pre_pll_clk_div(%x)\n", data16);
	vt_pix_clk /= data16;

	fimc_is_sensor_read16(client, 0x0302, &data16);
	pr_info("[SEN:DUMP] vt_sys_clk_div(%x)\n", data16);
	vt_pix_clk /= data16;

	fimc_is_sensor_read16(client, 0x0300, &data16);
	pr_info("[SEN:DUMP] vt_pix_clk_div(%x)\n", data16);
	vt_pix_clk /= data16;

	fimc_is_sensor_read16(client, 0x030C, &data16);
	pr_info("[SEN:DUMP] pll_post_scalar(%x)\n", data16);

	pr_info("[SEN:DUMP] vt_pix_clk(%lld)\n", vt_pix_clk);

	fimc_is_sensor_read16(client, 0x0340, &data16);
	pr_info("[SEN:DUMP] frame_length_lines(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x0342, &data16);
	pr_info("[SEN:DUMP] ine_length_pck(%x)\n", data16);

	fimc_is_sensor_read16(client, 0x0202, &data16);
	pr_info("[SEN:DUMP] coarse_integration_time(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x1004, &data16);
	pr_info("[SEN:DUMP] coarse_integration_time_min(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x1006, &data16);
	pr_info("[SEN:DUMP] coarse_integration_time_max_margin(%x)\n", data16);

	fimc_is_sensor_read16(client, 0x0200, &data16);
	pr_info("[SEN:DUMP] fine_integration_time(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x1008, &data16);
	pr_info("[SEN:DUMP] fine_integration_time_min(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x100A, &data16);
	pr_info("[SEN:DUMP] fine_integration_time_max_margin(%x)\n", data16);

	fimc_is_sensor_read16(client, 0x0084, &data16);
	pr_info("[SEN:DUMP] analogue_gain_code_min(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x0086, &data16);
	pr_info("[SEN:DUMP] analogue_gain_code_max(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x1084, &data16);
	pr_info("[SEN:DUMP] digital_gain_min(%x)\n", data16);
	fimc_is_sensor_read16(client, 0x1086, &data16);
	pr_info("[SEN:DUMP] digital_gain_max(%x)\n", data16);

	pr_info("[SEN:DUMP] *******************************\n");
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_2l3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = fimc_is_sensor_write8(client, 0x0104, hold);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;
p_err:
	return ret;
}
#else
static inline int sensor_2l3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_2l3_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2l3_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2l3_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_2l3_global, sensor_2l3_global_size);
	if (ret < 0) {
		err("sensor_2l3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2l3_cis_retention_crc_check(struct v4l2_subdev *subdev);
int sensor_2l3_cis_retention_prepare(struct v4l2_subdev *subdev);

int sensor_2l3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode > sensor_2l3_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;

	if (ext_info != NULL && ext_info->use_retention_mode == SENSOR_RETENTION_USE)
		sensor_2l3_cis_retention_crc_check(subdev);

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_2l3_check_rev is fail");
			goto p_err;
		}
	}

#if 0 /* cis_data_calculation is called in module_s_format */
	sensor_2l3_cis_data_calculation(sensor_2l3_pllinfos[mode], cis->cis_data);
#endif
	sensor_2l3_set_integration_max_margin(mode, cis->cis_data);

#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* Retention mode sensor mode select */
	if (ext_info->use_retention_mode != SENSOR_RETENTION_DISABLE) {
		switch (mode) {
		case SENSOR_2L3_4032X3024_30FPS:
			info("[%s] retention mode: SENSOR_2L3_4032X3024_30FPS\n", __func__);
			fimc_is_sensor_write16(cis->client, 0x0100, 0x0003);
			fimc_is_sensor_write16(cis->client, 0x021E, 0x0100);
			fimc_is_sensor_write16(cis->client, 0x6028, 0x2000);
			fimc_is_sensor_write16(cis->client, 0x602A, 0x0AA0);
			fimc_is_sensor_write16(cis->client, 0x6F12, 0x2000);
			fimc_is_sensor_write16(cis->client, 0x6F12, 0xA100);
			break;
		default:
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);
			ret = sensor_cis_set_registers(subdev, sensor_2l3_setfiles[mode],
								sensor_2l3_setfile_sizes[mode]);
			if (ret < 0) {
				err("sensor_2l3_set_registers fail!!");
				goto p_err;
			}
		}
	} else {
		info("[%s] sensor mode(%d)\n", __func__, mode);
		ret = sensor_cis_set_registers(subdev, sensor_2l3_setfiles[mode],
								sensor_2l3_setfile_sizes[mode]);
		if (ret < 0) {
			err("sensor_2l3_set_registers fail!!");
			goto p_err;
		}
	}

	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#ifdef CAMERA_REAR2
#ifdef CONFIG_SENSOR_RETENTION_USE
	switch (mode) {
	case SENSOR_2L3_4032X3024_30FPS:
		break;
	default:
		if (ext_info->use_retention_mode != SENSOR_RETENTION_DISABLE) {
			ret = sensor_2l3_cis_stream_on(subdev);
			if (ret < 0)
				err("sensor_2l3_stream_on fail!!");
			ret = sensor_2l3_cis_stream_off(subdev);
			if (ret < 0)
				err("sensor_2l3_stream_off fail!!");
			usleep_range(2000, 2010);
		}
		break;
	}
#endif
#endif
	/* sensor_2l3_cis_log_status(subdev); */

	return ret;
}

int sensor_2l3_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev, u16 frame_length_lines)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	pr_info("[%s] lownoise mode changed(%d)\n", __func__, cis->cis_data->cur_lownoise_mode);

	ret = fimc_is_sensor_write16(cis->client, 0x0104, 0x0101);

	switch (cis->cis_data->cur_lownoise_mode) {
	case FIMC_IS_CIS_LNOFF:
		dbg_sensor(1, "[%s] FIMC_IS_CIS_LNOFF\n", __func__);
		ret |= fimc_is_sensor_write16(cis->client, 0x0B30, 0x0100);
		break;
	case FIMC_IS_CIS_LN2:
		dbg_sensor(1, "[%s] FIMC_IS_CIS_LN2\n", __func__);
		ret |= fimc_is_sensor_write16(cis->client, 0x0B30, 0x0200);
		break;
	case FIMC_IS_CIS_LN4:
		dbg_sensor(1, "[%s] FIMC_IS_CIS_LN4\n", __func__);
		ret |= fimc_is_sensor_write16(cis->client, 0x0B30, 0x0300);
		break;
	default:
		dbg_sensor(1, "[%s] not support lownoise mode(%d)\n",
				__func__, cis->cis_data->cur_lownoise_mode);
	}

	ret |= fimc_is_sensor_write16(cis->client, 0x0340, frame_length_lines);
	ret |= fimc_is_sensor_write16(cis->client, 0x0104, 0x0001);

	if (ret < 0) {
		err("sensor_2l3_set_registers fail!!");
		goto p_err;
	}

	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;

p_err:
	return ret;
}

int sensor_2l3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;

	/* setfile global setting is at camera entrance */
	if (ext_info != NULL && ext_info->use_retention_mode != SENSOR_RETENTION_USE) {
		sensor_2l3_cis_set_global_setting_internal(subdev);
		/*
		if (ext_info->use_retention_mode == SENSOR_RETENTION_READY) {
			sensor_2l3_cis_retention_prepare(subdev);
		}
		*/
	}

	return ret;
}

int sensor_2l3_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef CONFIG_SENSOR_RETENTION_USE
	int i = 0;
#endif
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	err("To DO retention");

	return ret;

#if 0
#ifdef CONFIG_SENSOR_RETENTION_USE
	I2C_MUTEX_LOCK(cis->i2c_lock);
	for (i = 0; i < sensor_2l3_max_retention_num; i++) {
		ret = sensor_cis_set_registers(subdev, sensor_2l3_retention[i], sensor_2l3_retention_size[i]);
		if (ret < 0) {
			err("sensor_2l3_set_registers fail!!");
			goto p_err;
		}
	}
	dbg_sensor(1, "[%s] retention sensor RAM write done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
#endif
	return ret;
#endif
}

int sensor_2l3_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
#if 0
	u8 crc_check = 0;
#endif
	struct fimc_is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	err("To DO retention");

	return ret;

#if 0
	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* retention mode CRC check */
	mdelay(4);

	fimc_is_sensor_write16(cis->client, 0x602C, 0x2000);
	fimc_is_sensor_write16(cis->client, 0x602E, 0xBBD4);
	fimc_is_sensor_read8(cis->client, 0x6F12, &crc_check);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (crc_check == 0x01) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);
		info("\tUndefined CRC Check register value: 0x%x\n", crc_check);

		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_2l3_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		ret = sensor_2l3_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:
	return ret;
#endif
}
/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_2l3_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct fimc_is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif
	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_2l3_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_2L3_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_2L3_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_2L3_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_2L3_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_2L3_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_2L3_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		goto p_err;

	/* 3. output address setting */
	ret = fimc_is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		goto p_err;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = fimc_is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		goto p_err;

#if 0
	/* 5. binnig setting */
	ret = fimc_is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;
#endif

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full, 4:Separate vertical) */
	ret = fimc_is_sensor_write16(client, 0x0400, 0x0000);
	if (ret < 0)
		goto p_err;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed))
	 * down scale factor = down_scale_m / down_scale_n
	 */
	ret = fimc_is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n",
		__func__, cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2l3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

#ifdef CAMERA_REAR2
	struct fimc_is_device_sensor *device = NULL;
	u32 setfile;
	u32 scene;
	bool dual_flag = false;
	u32 mode;
#endif

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	sensor_2l3_cis_set_mipi_clock(subdev);
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

#ifdef DEBUG_2L3_PLL
	{
		u16 pll;

		fimc_is_sensor_read16(client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0304, &pll);
		dbg_sensor(1, "______ vt_pre_pll_clk_div(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0306, &pll);
		dbg_sensor(1, "______ vt_pll_multiplier(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0308, &pll);
		dbg_sensor(1, "______ op_pix_clk_div(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);

		fimc_is_sensor_read16(client, 0x030c, &pll);
		dbg_sensor(1, "______ vt_pll_post_scaler(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pre_pll_clk_dv(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0310, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0312, &pll);
		dbg_sensor(1, "______ op_pll_post_scalar(%x)\n", pll);

		fimc_is_sensor_read16(client, 0x0314, &pll);
		dbg_sensor(1, "______ DRAM_pre_pll_clk_div(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0316, &pll);
		dbg_sensor(1, "______ DRAM_pll_multiplier(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0318, &pll);
		dbg_sensor(1, "______ DRAM_pll_post_scalar(%x)\n", pll);

		fimc_is_sensor_read16(client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
		fimc_is_sensor_read16(client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

	/*
	 * If a companion is used,
	 * then 8 ms waiting is needed before the StreamOn of a sensor (S5K2L3).
	 */
	if (test_bit(FIMC_IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state))
		mdelay(8);

	/* Sensor stream on */
	fimc_is_sensor_write16(client, 0x0100, 0x0103);

	ret = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;

#ifdef CAMERA_REAR2
	mode = cis_data->sens_config_index_cur;
	dbg_sensor(1, "[%s] scene=%d, scenario_flag=%d, sens_config_index_cur=%d\n", __func__, scene, dual_flag, mode);

	switch (mode) {
	case SENSOR_2L3_4032X3024_30FPS:
		device = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
		if (unlikely(!device)) {
			err("device sensor is null");
			goto p_err;
		}

		if (device->ischain) {
			setfile = device->ischain->setfile;
		} else {
			err("device->ischain is null");
			ret = -EINVAL;
			goto p_err;
		}

		scene = (setfile & FIMC_IS_SCENARIO_MASK) >> FIMC_IS_SCENARIO_SHIFT;
		dual_flag = (scene == FIMC_IS_SCENARIO_AUTO_DUAL);

		if (dual_flag == true) {
			cis->cis_data->min_frame_us_time = (u32)((u64)(sensor_2l3_pllinfos[mode]->frame_length_lines + 0xA)
				* sensor_2l3_pllinfos[mode]->line_length_pck * 1000 / (sensor_2l3_pllinfos[mode]->pclk / 1000));
			cis->cis_data->min_sync_frame_us_time = cis->cis_data->min_frame_us_time;
		}
		break;
	default:
		break;
	}
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

#ifdef CONFIG_SENSOR_RETENTION_USE
	/* retention mode CRC check register enable */
	fimc_is_sensor_write16(client, 0x6028, 0x2000);
	fimc_is_sensor_write16(client, 0x602A, 0x0ACD);
	fimc_is_sensor_write8(client, 0x6F12, 0x01);
#endif

	fimc_is_sensor_write8(client, 0x0100, 0x00);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_exposure);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * (u64)(vt_pic_clk_freq_mhz)) / 1000 - min_fine_int)
											/ line_length_pck;
	short_coarse_int = ((target_exposure->short_val * (u64)(vt_pic_clk_freq_mhz)) / 1000 - min_fine_int)
											/ line_length_pck;

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/* WDR mode */
	if (fimc_is_vender_wdr_mode_on(cis_data)) {
		fimc_is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		fimc_is_sensor_write16(cis->client, 0x021E, 0x0100);
	} else {
		fimc_is_sensor_write16(cis->client, 0x021E, 0x0000);
	}

	/* Short exposure */
	ret = fimc_is_sensor_write16(client, 0x0202, short_coarse_int);
	if (ret < 0)
		goto p_err;

	/* Long exposure */
	if (fimc_is_vender_wdr_mode_on(cis_data)) {
		ret = fimc_is_sensor_write16(client, 0x0226, long_coarse_int);
		if (ret < 0)
			goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_mhz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_mhz/1000,
		line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
		long_coarse_int, short_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_mhz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_mhz(%d)\n", cis->id, __func__, vt_pic_clk_freq_mhz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_mhz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_mhz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_mhz(%d)\n", cis->id, __func__, vt_pic_clk_freq_mhz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_mhz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time,
			cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_duration);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)((((u64)(vt_pic_clk_freq_mhz) * input_exposure_time) / 1000
					- cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_mhz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count,
			input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_2l3_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)(((u64)(vt_pic_clk_freq_mhz) * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_mhz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_mhz/1000, frame_duration,
			line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	if (cis->long_term_mode.sen_strm_off_on_enable && frame_length_lines > 0x7FFF)
		frame_length_lines = 0x7FFF;

	if (cis->cis_data->cur_lownoise_mode != cis->cis_data->pre_lownoise_mode)
		ret = sensor_2l3_cis_set_lownoise_mode_change(subdev, frame_length_lines);
	else
		ret = fimc_is_sensor_write16(client, 0x0340, frame_length_lines);

	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time =
		cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_2l3_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:

	return ret;
}

int sensor_2l3_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_permile);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_2l3_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0])
		analog_gain = cis->cis_data->min_analog_gain[0];

	if (analog_gain > cis->cis_data->max_analog_gain[0])
		analog_gain = cis->cis_data->max_analog_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0204, analog_gain);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->min_analog_gain[0] = 0x20; /* x1, gain=x/0x20 */
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0],
		cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->max_analog_gain[0] = 0x200; /* x16, gain=x/0x20 */
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0],
		cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0])
		long_gain = cis->cis_data->min_digital_gain[0];

	if (long_gain > cis->cis_data->max_digital_gain[0])
		long_gain = cis->cis_data->max_digital_gain[0];

	if (short_gain < cis->cis_data->min_digital_gain[0])
		short_gain = cis->cis_data->min_digital_gain[0];

	if (short_gain > cis->cis_data->max_digital_gain[0])
		short_gain = cis->cis_data->max_digital_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
			KERN_CONT "long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, long_gain, short_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/*
	 * NOTE : In SAK2L3, digital gain is long/short seperated, should set 2 registers like below,
	 * Write same value to : 0x020E : short // GreenB
	 * Write same value to : 0x0214 : short // GreenR
	 * Write same value to : Need To find : long
	 */

	/* Short digital gain */
	ret = fimc_is_sensor_write16(client, 0x020E, short_gain);
	if (ret < 0)
		goto p_err;

	/* Long digital gain */
	if (fimc_is_vender_wdr_mode_on(cis_data)) {
		ret = fimc_is_sensor_write16(client, 0x3072, long_gain);
		if (ret < 0)
			goto p_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/*
	 * NOTE : In SAK2L3, digital gain is long/short seperated, should set 2 registers like below,
	 * Write same value to : 0x020E : short // GreenB
	 * Write same value to : 0x0214 : short // GreenR
	 * Write same value to : Need To find : long
	 */

	ret = fimc_is_sensor_read16(client, 0x020E, &digital_gain);
	if (ret < 0)
		goto p_err;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2l3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2l3_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = 0x100;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_digital_gain[0],
		cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = 0x8000;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_digital_gain[0],
		cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2l3_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct fimc_is_long_term_expo_mode *lte_mode;

	WARN_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	err("LTE need to implementation");

	return ret;

#if 0
	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {

		ret |= fimc_is_sensor_write16(cis->client, 0x6028, 0x2000);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0C10);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x1133);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D68);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x1133);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D88);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x1133);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D98);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x1133);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0B62);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x03C0);

		ret |= fimc_is_sensor_write16(cis->client, 0x0340, 0x7FFF);
		ret |= fimc_is_sensor_write16(cis->client, 0x0342, 0xE500);

		/* Current save to line_length_pck for recover line_length_pck */
		lte_mode->pre_line_length_pck = cis->cis_data->line_length_pck;
		cis->cis_data->line_length_pck = 0xE500;
	} else {
		ret |= fimc_is_sensor_write16(cis->client, 0x6028, 0x2000);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0C10);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x0361);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D68);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x0361);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D88);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x0361);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0D98);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x0361);
		ret |= fimc_is_sensor_write16(cis->client, 0x602A, 0x0B62);
		ret |= fimc_is_sensor_write16(cis->client, 0x6F12, 0x0102);

		ret |= fimc_is_sensor_write16(cis->client, 0x0340, 0x24F0);
		ret |= fimc_is_sensor_write16(cis->client, 0x0342, 0x27B0);

		/* Recover line_length_pck when sensor mode set value */
		cis->cis_data->line_length_pck = lte_mode->pre_line_length_pck;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s enable(%d)", __func__, lte_mode->sen_strm_off_on_enable);

	if (ret < 0) {
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
		return ret;
	}

	return ret;
#endif
}

#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
static int sensor_2l3_cis_update_mipi_info(struct v4l2_subdev *subdev)
{
	return 0;
}
#endif

static struct fimc_is_cis_ops cis_ops_2l3 = {
	.cis_init = sensor_2l3_cis_init,
	.cis_log_status = sensor_2l3_cis_log_status,
	.cis_group_param_hold = sensor_2l3_cis_group_param_hold,
	.cis_set_global_setting = sensor_2l3_cis_set_global_setting,
	.cis_mode_change = sensor_2l3_cis_mode_change,
	.cis_set_size = sensor_2l3_cis_set_size,
	.cis_stream_on = sensor_2l3_cis_stream_on,
	.cis_stream_off = sensor_2l3_cis_stream_off,
	.cis_set_exposure_time = sensor_2l3_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_2l3_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_2l3_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_2l3_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_2l3_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_2l3_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_2l3_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_2l3_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_2l3_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_2l3_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_2l3_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_2l3_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_2l3_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_2l3_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_2l3_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_data_calculation = sensor_2l3_cis_data_calc,
	.cis_set_long_term_exposure = sensor_2l3_cis_long_term_exposure,
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	.cis_update_mipi_info = sensor_2l3_cis_update_mipi_info,
#endif
#ifdef USE_CAMERA_EMBEDDED_HEADER
	.cis_get_frame_id = sensor_2l3_cis_get_frame_id,
#endif
};

static int cis_2l3_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id[FIMC_IS_STREAM_COUNT] = {0, };
	u32 sensor_id_len;
	const u32 *sensor_id_spec;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;
	int i;

	WARN_ON(!client);
	WARN_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	probe_info("%s sensor_id_spec %d, sensor_id_len %d\n", __func__,
			*sensor_id_spec, sensor_id_len);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		device = &core->sensor[sensor_id[i]];

		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_SAK2L3);
		if (!sensor_peri) {
			probe_info("sensor peri is net yet probed");
			return -EPROBE_DEFER;
		}
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_SAK2L3);

		cis = &sensor_peri->cis;
		subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
		if (!subdev_cis) {
			probe_err("subdev_cis is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		sensor_peri->subdev_cis = subdev_cis;

		cis->id = SENSOR_NAME_SAK2L3;
		cis->subdev = subdev_cis;
		cis->device = sensor_id[i];
		cis->client = client;
		sensor_peri->module->client = cis->client;
		cis->i2c_lock = NULL;
		cis->ctrl_delay = N_PLUS_TWO_FRAME;
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
		cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
		cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
#endif
		cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
		if (!cis->cis_data) {
			err("cis_data is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		cis->cis_ops = &cis_ops_2l3;

		/* belows are depend on sensor cis. MUST check sensor spec */
		cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

		if (of_property_read_bool(dnode, "sensor_f_number")) {
			ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
			if (ret)
				warn("f-number read is fail(%d)", ret);
		} else {
			cis->aperture_num = F1_5;
		}

		probe_info("%s f-number %d\n", __func__, cis->aperture_num);

		cis->use_dgain = true;
		cis->hdr_ctrl_by_again = false;

		v4l2_set_subdevdata(subdev_cis, cis);
		v4l2_set_subdev_hostdata(subdev_cis, device);
		snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);
	}

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 ||
			strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_A_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_A_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_A;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_A_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_A;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_A);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_A_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_A_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_A_retention);
#endif
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_B_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_B_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_B;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_B_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_B;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_B);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_B_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_B_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_B_retention);
#endif
	} else if (strcmp(setfile, "setC") == 0) {
		probe_info("%s setfile_C\n", __func__);
		sensor_2l3_global = sensor_2l3_setfile_C_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_C_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_C;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_C_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_C;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_C);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_C_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_C_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_C_retention);
#endif
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_2l3_global = sensor_2l3_setfile_A_Global;
		sensor_2l3_global_size = ARRAY_SIZE(sensor_2l3_setfile_A_Global);
		sensor_2l3_setfiles = sensor_2l3_setfiles_A;
		sensor_2l3_setfile_sizes = sensor_2l3_setfile_A_sizes;
		sensor_2l3_pllinfos = sensor_2l3_pllinfos_A;
		sensor_2l3_max_setfile_num = ARRAY_SIZE(sensor_2l3_setfiles_A);
#ifdef CONFIG_SENSOR_RETENTION_USE
		sensor_2l3_retention = sensor_2l3_setfiles_A_retention;
		sensor_2l3_retention_size = sensor_2l3_setfile_A_sizes_retention;
		sensor_2l3_max_retention_num = ARRAY_SIZE(sensor_2l3_setfiles_A_retention);
#endif
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_2l3_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-cis-2l3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_2l3_match);

static const struct i2c_device_id sensor_cis_2l3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_2l3_driver = {
	.probe	= cis_2l3_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_2l3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_2l3_idt
};

static int __init sensor_cis_2l3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_2l3_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_2l3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_2l3_init);
