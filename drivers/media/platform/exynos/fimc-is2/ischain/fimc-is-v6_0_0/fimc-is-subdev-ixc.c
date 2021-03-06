/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-device-ischain.h"
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-config.h"
#include "fimc-is-param.h"
#include "fimc-is-video.h"
#include "fimc-is-type.h"

static int fimc_is_ischain_ixc_cfg(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	return 0;
}

static int fimc_is_ischain_ixc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct fimc_is_fmt *format;
	struct fimc_is_group *group;
	u32 votf_enable;
	u32 votf_dst_ip = 0;
	u32 votf_dst_axi_id_m = 0;

	BUG_ON(!queue);
	BUG_ON(!queue->framecfg.format);

	group = &device->group_isp;
	if (group->vnext) {
		/* DCP Mater input only */
		votf_enable = OTF_INPUT_COMMAND_ENABLE;
		votf_dst_ip = VOTF_IP_DCP;
		votf_dst_axi_id_m = VOTF_AXI_ID_DCP_MASTER_IN;

	} else {
		votf_enable = OTF_INPUT_COMMAND_DISABLE;
	}

	format = queue->framecfg.format;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = format->hw_format;
	dma_output->order = format->hw_order;
	dma_output->bitwidth = format->hw_bitwidth;
	dma_output->plane = format->hw_plane;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop->w;
	dma_output->dma_crop_height = otcrop->h;

	dma_output->v_otf_enable = votf_enable;
	dma_output->v_otf_dst_ip = votf_dst_ip;
	dma_output->v_otf_dst_axi_id = votf_dst_axi_id_m;
	dma_output->v_otf_token_line = VOTF_TOKEN_LINE;

	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *vdma5_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma5_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	vdma5_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixc_tag(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct isp_param *isp_param;
	struct fimc_is_crop *otcrop, otparm;
	struct fimc_is_device_ischain *device;
	u32 lindex, hindex, indexes;
	u32 pixelformat = 0;

	device = (struct fimc_is_device_ischain *)device_data;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

	mdbgs_ischain(4, "ISPC TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	isp_param = &device->is_region->parameter.isp;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	pixelformat = queue->framecfg.format->pixelformat;

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;
		otcrop->x = 0;
		otcrop->y = 0;
		otcrop->w = isp_param->otf_input.width;
		otcrop->h = isp_param->otf_input.height;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = isp_param->vdma5_output.width;
		otparm.h = isp_param->vdma5_output.height;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = fimc_is_ischain_ixc_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixc_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->ixcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_ixc_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		scalerUd->ixcTargetAddress[0] = 0;
		scalerUd->ixcTargetAddress[1] = 0;
		scalerUd->ixcTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_subdev_ops fimc_is_subdev_ixc_ops = {
	.bypass			= NULL,
	.cfg			= fimc_is_ischain_ixc_cfg,
	.tag			= fimc_is_ischain_ixc_tag,
};
