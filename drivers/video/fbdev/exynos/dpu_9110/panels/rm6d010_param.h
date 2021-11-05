/* rm6d010_param.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __RM6D010_PARAM_H__
#define __RM6D010_PARAM_H__

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			100
#define DEFAULT_BRIGHTNESS		80
#define HBM_BRIHGTNESS			120

#define POWER_IS_ON(pwr)	((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define LDI_ID_REG		0x04
#define LDI_ID_LEN		3

#define LDI_CASET		0x2A
#define LDI_PASET		0x2B
#define LDI_CHIP_ID		0xD6

#define REFRESH_60HZ		60

#define PANEL_DISCONNEDTED		0
#define PANEL_CONNECTED		1

static const unsigned char INIT_1[] = {
	0xfe,
	0x05, 0x00,
};

static const unsigned char INIT_2[] = {
	0x05,
	0x1f, 0x00,
};

static const unsigned char INIT_3[] = {
	0xfe,
	0x00, 0x00,
};

static const unsigned char INIT_4[] = {
	0x35,
	0x02, 0x00,
};

static const unsigned char INIT_5[] = {
	0x2a,
	0x00, 0x06, 0x01, 0x8b,
};

static const unsigned char INIT_6[] = {
	0x2b,
	0x00, 0x00, 0x01, 0x85,
};

static const unsigned char INIT_7[] = {
	0x11,
	0x00, 0x00,
};

static const unsigned char INIT_8[] = {
	0x29,
	0x00, 0x00,
};

#endif
