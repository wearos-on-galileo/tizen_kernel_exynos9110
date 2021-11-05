/*
 *  include/linux/power/input_stat.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __INPUT_STAT_H
#define __INPUT_STAT_H

enum input_stat_ev_wakeup {
	EV_WU_TW, /* tatal wakeup count */
	EV_WU_00,
	EV_WU_10, /* ev count per wakeup less than 10 */
	EV_WU_20,
	EV_WU_30,
	EV_WU_40,
	EV_WU_GT_40, /* ev count per wakeup greater than 40 */
	EV_WU_MAX
};

enum input_stat_direction_patten {
	CounterClockwise,
	Detent_Return,
	Detent_None,
	Detent_Leave,
	Clockwise,
	Direction_MAX,
};

struct input_stat_emon {
	unsigned long key_press;
	unsigned long key_release;
	unsigned long touch_press;
	unsigned long touch_release;
	unsigned long direction[Direction_MAX];
	unsigned int ev_wakeup[EV_WU_MAX];
};

#ifdef CONFIG_INPUT_STAT
extern int input_stat_get_stat_delta(int type,
 					struct input_stat_emon *emon_stat);
#else
static inline int input_stat_get_stat_delta(int type,
 					struct input_stat_emon *emon_stat) {	return 0;}
#endif

#endif /* __INPUT_STAT_H */

