/* rm69330_param.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __RM69330_PARAM_H__
#define __RM69330_PARAM_H__

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			100
#define DEFAULT_BRIGHTNESS		80
#define HBM_BRIHGTNESS			120
#define HBM_DEFAULT_LEVEL		251
#define LPM_DEFAULT_LEVEL		255

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

static const unsigned char rm69330_br_map[MAX_BRIGHTNESS+1] = {
	49, 51, 53, 54, 56, 58, 59, 61, 63, 64,
	66, 68, 70, 71, 73, 75, 76, 78, 80, 81,
	83, 85, 86, 88, 89, 91, 92, 94, 95, 97,
	98, 100, 101, 103, 104, 106, 107, 109, 110, 112,
	113, 115, 117, 118, 120, 122, 124, 126, 127, 129,
	131, 133, 136, 138, 141, 143, 145, 148, 150, 153,
	155, 157, 159, 160, 162, 164, 166, 168, 169, 171,
	173, 175, 177, 179, 181, 183, 184, 186, 188, 190,
	192, 195, 198, 200, 203, 206, 209, 212, 214, 217,
	220, 223, 226, 228, 231, 234, 238, 242, 245, 249,
	253
};

static const unsigned char RM69330_INIT_1[] = {
	0xfe,
	0x00, 0x00,
};

static const unsigned char RM69330_INIT_2[] = {
	0x35,
	0x00, 0x00,
};

static const unsigned char RM69330_INIT_3[] = {
	0x2a,
	0x00, 0x00, 0x01, 0x67,
};

static const unsigned char RM69330_INIT_4[] = {
	0x2b,
	0x00, 0x00, 0x01, 0x67,
};

static const unsigned char RM69330_INIT_5[] = {
	0x51,
	0x00, 0x00,
};

static const unsigned char RM69330_INIT_6[] = {
	0x11,
	0x00, 0x00,
};

static const unsigned char RM69330_INIT_7[] = {
	0x10,
	0x00, 0x00,
};

static const unsigned char RM69330_INIT_8[] = {
	0x4f,
	0x01, 0x00,
};

static const unsigned char RM69330_INIT_9[] = {
	0xfe,
	0x01, 0x00,
};

static const unsigned char RM69330_INIT_10[] = {
	0x74,
	0x38, 0x00,
};

static const unsigned char RM69330_INIT_11[] = {
	0x74,
	0x08, 0x00,
};

static const unsigned char RM69330_INIT_12[] = {
	0x66,
	0x10, 0x00,
};

static const unsigned char RM69330_INIT_13[] = {
	0x69,
	0x32, 0x00,
};

static const unsigned char RM69330_INIT_14[] = {
	0xa7,
	0x0a, 0x00,
};

static const unsigned char RM69330_INIT_15[] = {
	0xa9,
	0x3a, 0x00,
};

static const unsigned char RM69330_INIT_16[] = {
	0x35,
	0x02, 0x00,
};

static const unsigned char RM69330_INIT_17[] = {
	0x15,
	0x04, 0x00,
};

static const unsigned char RM69330_INIT_18[] = {
	0x11,
	0x91, 0x00,
};

static const unsigned char RM69330_INIT_19[] = {
	0x11,
	0x81, 0x00,
};

static const unsigned char RM69330_INIT_20[] = {
	0xfe,
	0x07, 0x00,
};

static const unsigned char RM69330_DISPLAY_CTRL[] = {
	0x53,
	0x20, 0x00,
};

static const unsigned char RM69330_DISP_ON[] = {
	0x29,
	0x00, 0x00,
};
 
static const unsigned char RM69330_DISP_OFF[] = {
	0x28,
	0x00, 0x00,
};

static const unsigned char RM69330_LPM_ON[] = {
	0x39,
	0x00, 0x00,
};

static const unsigned char RM69330_LPM_OFF[] = {
	0x38,
	0x00, 0x00,
};

static const unsigned char RM69330_HBM_ON[] = {
	0x66,
	0x02,
};

static const unsigned char RM69330_HBM_OFF[] = {
	0x66,
	0x00,
};

static const unsigned char RM69330_NEGATIVE_ON[] = {
	0x21,
};

static const unsigned char RM69330_NEGATIVE_OFF[] = {
	0x20,
};

static const unsigned char RM69330_MAX_GAMMA[] = {
	0x51,
	0xff, 0x00,
};
#endif
