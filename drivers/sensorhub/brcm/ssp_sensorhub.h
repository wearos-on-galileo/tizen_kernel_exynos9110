/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_SENSORHUB_H__
#define __SSP_SENSORHUB_H__

#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/trm.h>
#include "ssp.h"

/* 'LIST_SIZE' should be be rounded-up to a power of 2 */
#define LIST_SIZE			8
#define MAX_DATA_COPY_TRY		2
#define WAKE_LOCK_TIMEOUT		(0.3*HZ)
#define COMPLETION_TIMEOUT		(2*HZ)
#define DATA				REL_RX
#define BIG_DATA			REL_RY
#define NOTICE				REL_RZ
#define BIG_DATA_SIZE			256
#define PRINT_TRUNCATE			6
#define BIN_PATH_SIZE			100

#define SENSORHUB_IOCTL_MAGIC		'S'
#define IOCTL_READ_BIG_CONTEXT_DATA	_IOR(SENSORHUB_IOCTL_MAGIC, 3, int)
#define IOCTL_SSP_WOM_CMD	_IO(SENSORHUB_IOCTL_MAGIC, 5)

enum {
	CHANGE_NORMAL_MODE,
	CHANGE_WATCHONLY_MODE,

	INT_WOM_IMAGE_USE = 0x10,
	EXT_WOM_IMAGE_INFO,
	EXT_WOM_IMAGE_DATA,
	EXT_WOM_IMAGE_SEND,

	INST_WOM_START = 0x20,
};

struct sensorhub_wom {
	char cmd_type;
	int size;
	unsigned long addr;
};

#define sensorhub_info(str, args...) \
	pr_info("[SSP]: %s - " str, __func__, ##args)
#define sensorhub_debug(str, args...) \
	pr_debug("[SSP]: %s - " str, __func__, ##args)
#define sensorhub_err(str, args...) \
	pr_err("[SSP]: %s - " str, __func__, ##args)


struct sensorhub_event {
	char *library_data;
	int library_length;
};

struct wom_image {
	char *buf;
	int offset_idx;
	int total_size;
};

struct wom_mcu_cmd {
	/* send_instruction_sync has internally
	 * command buf length limitation
	 */
	u8 ext_val[10];
	u16 ext_val_sz;
};

struct ssp_sensorhub_data {
	struct ssp_data *ssp_data;
	struct input_dev *sensorhub_input_dev;
	struct task_struct *sensorhub_task;
	struct miscdevice sensorhub_device;
	struct wake_lock sensorhub_wake_lock;
	struct completion read_done;
	struct completion big_read_done;
	struct completion big_write_done;
	struct sensorhub_event events[LIST_SIZE];
	struct sensorhub_event big_events;
	struct sensorhub_event big_send_events;
  struct wom_image wimage;
	struct kfifo fifo;
	bool is_big_event;
	int event_number;
	int pcm_cnt;
	wait_queue_head_t sensorhub_wq;
	spinlock_t sensorhub_lock;
	struct mutex big_events_lock;
};

int ssp_sensorhub_pcm_dump(struct ssp_sensorhub_data *hub_data);
void ssp_sensorhub_report_notice(struct ssp_data *ssp_data, char notice);
int ssp_sensorhub_handle_data(struct ssp_data *ssp_data, char *dataframe,
				int start, int end);
int ssp_sensorhub_initialize(struct ssp_data *ssp_data);
void ssp_sensorhub_remove(struct ssp_data *ssp_data);
int handle_wom_image_cmd(struct sensorhub_wom *wom, struct ssp_sensorhub_data *hub_data);
#ifdef CONFIG_COPR_SUPPORT
void convert_candela(char *buf);
#endif

#endif
