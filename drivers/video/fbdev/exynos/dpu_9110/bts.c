 /*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * BTS file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "decon.h"

#include <soc/samsung/bts.h>
#include <media/v4l2-subdev.h>

#define DISP_FACTOR		100UL
#define PPC			1UL	/* Morion PPC : 1 */
#define LCD_REFRESH_RATE	63UL
#define MULTI_FACTOR 		(1UL << 10)

u64 dpu_bts_calc_aclk_disp(struct decon_device *decon,
		struct decon_win_config *config, u64 resol_clock)
{
	u64 s_ratio_h, s_ratio_v;
	u64 aclk_disp;
	u64 ppc;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;

	s_ratio_h = (src->w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src->w / (u64)dst->w;
	s_ratio_v = (src->h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src->h / (u64)dst->h;

		ppc = PPC;

	aclk_disp = resol_clock * s_ratio_h * s_ratio_v * DISP_FACTOR  / 100UL
		/ ppc * (MULTI_FACTOR * (u64)dst->w / (u64)decon->lcd_info->xres)
		/ (MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR);

	return aclk_disp;
}

/* bus utilization 75% */
#define BUS_UTIL	75

void dpu_bts_find_max_disp_freq(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	int idx;
	u32 disp_ch_bw;
	u32 max_disp_ch_bw;
	u32 disp_op_freq = 0, freq = 0;
	u64 resol_clock;
	u64 op_fps = LCD_REFRESH_RATE;
	struct decon_win_config *config = regs->dpp_config;

    /* # of DMA of 9110 : 3 */
	    disp_ch_bw = decon->bts.bw[BTS_DPP0] + decon->bts.bw[BTS_DPP1] + decon->bts.bw[BTS_DPP2];

	DPU_DEBUG_BTS("\tCH%d = %d\n", i, disp_ch_bw);

	max_disp_ch_bw = disp_ch_bw;

	decon->bts.peak = max_disp_ch_bw;
	decon->bts.max_disp_freq = max_disp_ch_bw * 100 / (16 * BUS_UTIL) + 1;

	/* 1.1: 10% margin, 1000: for KHZ, 1: for raising to a unit */
	resol_clock = decon->lcd_info->xres * decon->lcd_info->yres *
		op_fps * 11 / 10 / 1000 + 1;
	decon->bts.resol_clk = resol_clock;

	DPU_DEBUG_BTS("\tDECON%d : resol clock = %d Khz\n",
		decon->id, decon->bts.resol_clk);

	for (i = 0; i < MAX_DECON_WIN; ++i) {
		idx = config[i].idma_type;
		if (config[i].state != DECON_WIN_STATE_BUFFER)
			continue;

		freq = dpu_bts_calc_aclk_disp(decon, &config[i], resol_clock);
		if (disp_op_freq < freq)
			disp_op_freq = freq;
	}

	DPU_DEBUG_BTS("\tDISP bus freq(%d), operating freq(%d)\n",
			decon->bts.max_disp_freq, disp_op_freq);

	if (decon->bts.max_disp_freq < disp_op_freq)
		decon->bts.max_disp_freq = disp_op_freq;

	DPU_DEBUG_BTS("\tMAX DISP CH FREQ = %d\n", decon->bts.max_disp_freq);
}

void dpu_bts_calc_bw(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_win_config *config = regs->dpp_config;
	struct bts_decon_info bts_info;
	enum dpp_rotate rot;
	int idx, i;

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("\n");
	DPU_DEBUG_BTS("%s + : DECON%d\n", __func__, decon->id);

	memset(&bts_info, 0, sizeof(struct bts_decon_info));
	for (i = 0; i < MAX_DECON_WIN; ++i) {
		if (config[i].state == DECON_WIN_STATE_BUFFER) {
			idx = config[i].idma_type;
			bts_info.dpp[idx].used = true;
		} else {
			continue;
		}

		bts_info.dpp[idx].idma_type = idx;
		bts_info.dpp[idx].bpp = dpu_get_bpp(config[i].format);
		bts_info.dpp[idx].src_w = config[i].src.w;
		bts_info.dpp[idx].src_h = config[i].src.h;
		bts_info.dpp[idx].dst.x1 = config[i].dst.x;
		bts_info.dpp[idx].dst.x2 = config[i].dst.x + config[i].dst.w;
		bts_info.dpp[idx].dst.y1 = config[i].dst.y;
		bts_info.dpp[idx].dst.y2 = config[i].dst.y + config[i].dst.h;
		rot = config[i].dpp_parm.rot;
		bts_info.dpp[idx].rotation = (rot > DPP_ROT_180) ? true : false;

		DPU_DEBUG_BTS("\tDPP%d : bpp(%d) src w(%d) h(%d) rot(%d)\n",
				idx, bts_info.dpp[idx].bpp,
				bts_info.dpp[idx].src_w, bts_info.dpp[idx].src_h,
				bts_info.dpp[idx].rotation);
		DPU_DEBUG_BTS("\t\t\t\tdst x(%d) right(%d) y(%d) bottom(%d)\n",
				bts_info.dpp[idx].dst.x1,
				bts_info.dpp[idx].dst.x2,
				bts_info.dpp[idx].dst.y1,
				bts_info.dpp[idx].dst.y2);
	}

	bts_info.vclk = decon->bts.resol_clk;
	bts_info.lcd_w = decon->lcd_info->xres;
	bts_info.lcd_h = decon->lcd_info->yres;
	decon->bts.total_bw = bts_calc_bw(decon->bts.type, &bts_info);
	memcpy(&decon->bts.bts_info, &bts_info, sizeof(struct bts_decon_info));

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		decon->bts.bw[i] = bts_info.dpp[i].bw;
		if (decon->bts.bw[i])
			DPU_DEBUG_BTS("\tDPP%d bandwidth = %d\n",
					i, decon->bts.bw[i]);
	}

	DPU_DEBUG_BTS("\tDECON%d total bandwidth = %d\n", decon->id,
			decon->bts.total_bw);

	dpu_bts_find_max_disp_freq(decon, regs);

	DPU_DEBUG_BTS("%s -\n", __func__);
}

static void dpu_bts_log_info_output(struct decon_device *decon, struct decon_reg_data *regs)
{
	int idx, i;
	struct decon_win_config *config = regs->dpp_config;
	struct bts_decon_info *bts_info = &decon->bts.bts_info;

	for (i = 0; i < MAX_DECON_WIN; ++i) {
		if (config[i].state != DECON_WIN_STATE_BUFFER)
			continue;

		idx = config[i].idma_type;
		DPU_DEBUG_BTS("[%d] DPP[%d] (%d) (%4d %4d)\t(%4d %4d %4d %4d)\n",
				i, bts_info->dpp[idx].idma_type, bts_info->dpp[idx].bpp,
				bts_info->dpp[idx].src_w, bts_info->dpp[idx].src_h,
				bts_info->dpp[idx].dst.x1, bts_info->dpp[idx].dst.y1,
				bts_info->dpp[idx].dst.x2, bts_info->dpp[idx].dst.y2);
	}
	DPU_DEBUG_BTS("BW(KB/s): type%d bw %up %ur\n",
			decon->id, decon->bts.peak, decon->bts.total_bw);
}

void dpu_bts_update_bw(struct decon_device *decon, struct decon_reg_data *regs,
		u32 is_after)
{
	struct bts_bw bw = { 0, };

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	/* update peak & read bandwidth per DPU port */
	bw.peak = decon->bts.peak;
	bw.read = decon->bts.total_bw;
	DPU_DEBUG_BTS("\tpeak = %d, read = %d\n", bw.peak, bw.read);

	if (bw.read == 0)
		bw.peak = 0;

	if (is_after) { /* after DECON h/w configuration */
		if (decon->bts.total_bw <= decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.type, bw);

		if (decon->bts.max_disp_freq <= decon->bts.prev_max_disp_freq)
			pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);

		decon->bts.prev_total_bw = decon->bts.total_bw;
		decon->bts.prev_max_disp_freq = decon->bts.max_disp_freq;
	} else {
		if (decon->bts.total_bw > decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.type, bw);

		if (decon->bts.max_disp_freq > decon->bts.prev_max_disp_freq)
			pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);

		if (dpu_bts_log_level >= 7)
			dpu_bts_log_info_output(decon, regs);
	}

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_acquire_bw(struct decon_device *decon)
{
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;
}

void dpu_bts_release_bw(struct decon_device *decon)
{
	struct bts_bw bw = { 0, };
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	bts_update_bw(decon->bts.type, bw);
	decon->bts.prev_total_bw = 0;
	pm_qos_update_request(&decon->bts.disp_qos, 0);
	decon->bts.prev_max_disp_freq = 0;

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_init(struct decon_device *decon)
{
	int comp_ratio;

	DPU_DEBUG_BTS("%s +\n", __func__);

	decon->bts.enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS9110_BTS)) {
		DPU_ERR_BTS("decon%d bts feature is disabled\n", decon->id);
		return;
	}

		decon->bts.type = BTS_BW_DECON0;

	DPU_DEBUG_BTS("BTS_BW_TYPE(%d) -\n", decon->bts.type);

	if (decon->lcd_info->dsc_enabled)
		comp_ratio = 3;
	else
		comp_ratio = 1;

		/*
		 * Resol clock(KHZ) = lcd width x lcd height x 63(refresh rate) x
		 *               1.1(10% margin) x comp_ratio(1/3 DSC) / 2(2PPC) /
		 *		1000(for KHZ) + 1(for raising to a unit)
		 */
	decon->bts.resol_clk = decon->lcd_info->xres *
		decon->lcd_info->yres * LCD_REFRESH_RATE * 11 / 10 / 1000 + 1;

	DPU_DEBUG_BTS("[Init: D%d] resol clock = %d Khz\n",
		decon->id, decon->bts.resol_clk);

	pm_qos_add_request(&decon->bts.disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
	decon->bts.scen_updated = 0;

	decon->bts.enabled = true;

	DPU_INFO_BTS("decon%d bts feature is enabled\n", decon->id);
}

void dpu_bts_deinit(struct decon_device *decon)
{
	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("%s +\n", __func__);
	pm_qos_remove_request(&decon->bts.disp_qos);
	DPU_DEBUG_BTS("%s -\n", __func__);
}

struct decon_bts_ops decon_bts_control = {
	.bts_init		= dpu_bts_init,
	.bts_calc_bw		= dpu_bts_calc_bw,
	.bts_update_bw		= dpu_bts_update_bw,
	.bts_acquire_bw		= dpu_bts_acquire_bw,
	.bts_release_bw		= dpu_bts_release_bw,
	.bts_deinit		= dpu_bts_deinit,
};
