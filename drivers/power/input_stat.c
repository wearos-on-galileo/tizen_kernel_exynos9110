/*
 *  drivers/power/input_stat.c
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/power/input_stat.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

struct key_stat {
	unsigned int ev_type;
	unsigned int last_ev_code;
	unsigned int last_ev_value;
	ktime_t last_ev_time;
	unsigned int ev_wakeup[EV_WU_MAX]; /* press event count per wakeup */
	unsigned long press_count;
	unsigned long release_count;
};

struct wheel_stat {
	unsigned int ev_type;
	unsigned int last_ev_code;
	unsigned int last_ev_value;
	ktime_t last_ev_time;
	unsigned int ev_wakeup[EV_WU_MAX]; /* ccw/cw event count per wakeup */
	unsigned long direction[Direction_MAX];
};

struct input_stat {
	struct key_stat key;
	struct key_stat touch;
	struct wheel_stat wheel;
	struct input_stat_emon pm_stat;
	unsigned int ev_wakeup[EV_WU_MAX];

#ifdef CONFIG_ENERGY_MONITOR
	struct input_stat_emon emon_stat;
#endif

	struct dentry *debug;
};

static struct input_stat *input_stat;

#ifdef CONFIG_ENERGY_MONITOR
int input_stat_get_stat_delta(int type, struct input_stat_emon *emon_stat)
{
	int i;

	emon_stat->key_press =
		input_stat->key.press_count - input_stat->emon_stat.key_press;
	emon_stat->key_release =
		input_stat->key.release_count - input_stat->emon_stat.key_release;
	emon_stat->touch_press =
		input_stat->touch.press_count - input_stat->emon_stat.touch_press;
	emon_stat->touch_release =
		input_stat->touch.release_count - input_stat->emon_stat.touch_release;
	for (i = 0; i < Direction_MAX; i++)
		emon_stat->direction[i] =
			input_stat->wheel.direction[i] - input_stat->emon_stat.direction[i];
	for (i = 0; i < EV_WU_MAX; i++)
		emon_stat->ev_wakeup[i] =
			input_stat->ev_wakeup[i] - input_stat->emon_stat.ev_wakeup[i];

	if (type != ENERGY_MON_TYPE_DUMP) {
		input_stat->emon_stat.key_press = input_stat->key.press_count;
		input_stat->emon_stat.key_release = input_stat->key.release_count;
		input_stat->emon_stat.touch_press = input_stat->touch.press_count;
		input_stat->emon_stat.touch_release = input_stat->touch.release_count;
		for (i = 0; i < Direction_MAX; i++)
			input_stat->emon_stat.direction[i] = input_stat->wheel.direction[i];
		for (i = 0; i < EV_WU_MAX; i++)
			input_stat->emon_stat.ev_wakeup[i] = input_stat->ev_wakeup[i];
	}

	return 0;
}
#endif

static void input_stat_events(struct input_handle *handle,
			       const struct input_value *vals, unsigned int count)
{
	struct input_handler *handler = handle->handler;
	struct input_stat *stat = handler->private;
	int i;

	for (i = 0; i < count; i++) {
		if (vals[i].type == EV_KEY) {
			pr_debug("%s:%d:%d:%d\n", __func__,
				vals[i].type, vals[i].code, vals[i].value);
			if (vals[i].code == BTN_TOUCH) {
				stat->touch.last_ev_time = ktime_get();
				stat->touch.last_ev_code = vals[i].code;
				stat->touch.last_ev_value = vals[i].value;
				if (vals[i].value)
					stat->touch.press_count++;
				else
					stat->touch.release_count++;
			} else {
				stat->key.last_ev_time = ktime_get();
				stat->key.last_ev_code = vals[i].code;
				stat->key.last_ev_value = vals[i].value;
				if (vals[i].value)
					stat->key.press_count++;
				else
					stat->key.release_count++;
			}
		} else if (vals[i].type == EV_REL){
			if (vals[i].code == REL_WHEEL &&
				vals[i].value >= -2 && vals[i].value <= 2) {
				pr_debug("%s:%d:%d:%d\n", __func__,
					vals[i].type, vals[i].code, vals[i].value);
				stat->wheel.last_ev_time = ktime_get();
				stat->wheel.last_ev_code = vals[i].code;
				stat->wheel.last_ev_value = vals[i].value;
				stat->wheel.direction[vals[i].value+2]++;
			}
		}
	}

}

static int input_stat_connect(struct input_handler *handler,
					  struct input_dev *dev,
					  const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "input_stat";

	error = input_register_handle(handle);
	if (error) {
		pr_err("Failed to register input_stat handler, error %d\n",
		       error);
		kfree(handle);
		return error;
	}

	error = input_open_device(handle);
	if (error) {
		pr_err("Failed to open input_stat device, error %d\n", error);
		input_unregister_handle(handle);
		kfree(handle);
		return error;
	}

	return 0;
}

static void input_stat_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id input_stat_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER)},
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(KEY_BACK)] = BIT_MASK(KEY_BACK)},
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_RELBIT,
		.evbit = { BIT_MASK(EV_REL) },
		.relbit = { [BIT_WORD(REL_WHEEL)] = BIT_MASK(REL_WHEEL)},
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
	},
	{ },
};

MODULE_DEVICE_TABLE(input, input_stat_ids);

static struct input_handler input_stat_handler = {
	.events =	input_stat_events,
	.connect =	input_stat_connect,
	.disconnect =	input_stat_disconnect,
	.name =		"input_stat",
	.id_table = input_stat_ids,
};

static int input_stat_show(struct seq_file *m, void *unused)
{
	seq_puts(m, "ev_type  last_ev_code  last_ev_value  last_ev_time    "
			"ev_wu_00      ev_wu_10      ev_wu_20      "
			"ev_wu_30      ev_wu_40      ev_wu_gt_40 "
			"press_count   release_count\n");

	seq_printf(m, "%-7u  %-12u  %-13u  %-14llu  "
		"%-12u  %-12u  %-12u  %-12u  %-12u  %-12u"
		"%-12lu  %-12lu\n",
		input_stat->key.ev_type,
		input_stat->key.last_ev_code,
		input_stat->key.last_ev_value,
		ktime_to_ms(input_stat->key.last_ev_time),
		input_stat->key.ev_wakeup[EV_WU_00],
		input_stat->key.ev_wakeup[EV_WU_10],
		input_stat->key.ev_wakeup[EV_WU_20],
		input_stat->key.ev_wakeup[EV_WU_30],
		input_stat->key.ev_wakeup[EV_WU_40],
		input_stat->key.ev_wakeup[EV_WU_GT_40],
		input_stat->key.press_count,
		input_stat->key.release_count);

	seq_printf(m, "%-7u  %-12u  %-13u  %-14llu  "
		"%-12u  %-12u  %-12u  %-12u  %-12u  %-12u"
		"%-12lu  %-12lu\n",
		input_stat->touch.ev_type,
		input_stat->touch.last_ev_code,
		input_stat->touch.last_ev_value,
		ktime_to_ms(input_stat->touch.last_ev_time),
		input_stat->touch.ev_wakeup[EV_WU_00],
		input_stat->touch.ev_wakeup[EV_WU_10],
		input_stat->touch.ev_wakeup[EV_WU_20],
		input_stat->touch.ev_wakeup[EV_WU_30],
		input_stat->touch.ev_wakeup[EV_WU_40],
		input_stat->touch.ev_wakeup[EV_WU_GT_40],
		input_stat->touch.press_count,
		input_stat->touch.release_count);

	seq_printf(m, "%-7u  %-12u  %-13d  %-14llu  "
		"%-12u  %-12u  %-12u  %-12u  %-12u  %-12u"
		"%-12lu  %-12lu  %-12lu  %-12lu\n",
		input_stat->wheel.ev_type,
		input_stat->wheel.last_ev_code,
		input_stat->wheel.last_ev_value,
		ktime_to_ms(input_stat->wheel.last_ev_time),
		input_stat->wheel.ev_wakeup[EV_WU_00],
		input_stat->wheel.ev_wakeup[EV_WU_10],
		input_stat->wheel.ev_wakeup[EV_WU_20],
		input_stat->wheel.ev_wakeup[EV_WU_30],
		input_stat->wheel.ev_wakeup[EV_WU_40],
		input_stat->wheel.ev_wakeup[EV_WU_GT_40],
		input_stat->wheel.direction[CounterClockwise],
		input_stat->wheel.direction[Detent_Return],
		input_stat->wheel.direction[Detent_Leave],
		input_stat->wheel.direction[Clockwise]);

	seq_printf(m, "%-7u  %-12u  %-13d  %-14u  "
		"%-12u  %-12u  %-12u  %-12u  %-12u  %-12u"
		"%-12u  %-12u\n",
		0,
		0,
		0,
		input_stat->ev_wakeup[EV_WU_TW],
		input_stat->ev_wakeup[EV_WU_00],
		input_stat->ev_wakeup[EV_WU_10],
		input_stat->ev_wakeup[EV_WU_20],
		input_stat->ev_wakeup[EV_WU_30],
		input_stat->ev_wakeup[EV_WU_40],
		input_stat->ev_wakeup[EV_WU_GT_40],
		0,
		0);

	return 0;
}

static int input_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, input_stat_show, NULL);
}

static const struct  file_operations input_stat_fops = {
	.open = input_stat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int input_stat_find_ev_wakeup_index(unsigned int ev_count)
{
	int index;

	if (ev_count == 0)
		index = EV_WU_00;
	else if (ev_count > 0 && ev_count <= 10)
		index = EV_WU_10;
	else if (ev_count > 10 && ev_count <= 20)
		index = EV_WU_20;
	else if (ev_count > 20 && ev_count <= 30)
		index = EV_WU_30;
	else if (ev_count > 30 && ev_count <= 40)
		index = EV_WU_40;
	else
		index = EV_WU_GT_40;

	return index;
}

static void input_stat_pm_suspend_prepare_cb(void)
{
	int ev_wakeup_index;
	int wakeup_direction;
	unsigned long key_press;
	unsigned long key_release;
	unsigned long touch_press;
	unsigned long touch_release;
	unsigned long direction[Direction_MAX];

	key_press =
		input_stat->key.press_count - input_stat->pm_stat.key_press;
	key_release =
		input_stat->key.release_count - input_stat->pm_stat.key_release;
	touch_press =
		input_stat->touch.press_count - input_stat->pm_stat.touch_press;
	touch_release =
		input_stat->touch.release_count - input_stat->pm_stat.touch_release;
	direction[CounterClockwise] =
		input_stat->wheel.direction[CounterClockwise] -
		input_stat->pm_stat.direction[CounterClockwise];
	direction[Detent_Return] =
		input_stat->wheel.direction[Detent_Return] -
		input_stat->pm_stat.direction[Detent_Return];
	direction[Detent_Leave] =
		input_stat->wheel.direction[Detent_Leave] -
		input_stat->pm_stat.direction[Detent_Leave];
	direction[Clockwise] =
		input_stat->wheel.direction[Clockwise] -
		input_stat->pm_stat.direction[Clockwise];

	input_stat->ev_wakeup[EV_WU_TW]++;

 	ev_wakeup_index = input_stat_find_ev_wakeup_index(key_press);
	input_stat->key.ev_wakeup[ev_wakeup_index]++;

 	ev_wakeup_index = input_stat_find_ev_wakeup_index(touch_press);
	input_stat->touch.ev_wakeup[ev_wakeup_index]++;

	wakeup_direction = direction[CounterClockwise] + direction[Detent_Leave] + direction[Clockwise];
 	ev_wakeup_index = input_stat_find_ev_wakeup_index(wakeup_direction);
	input_stat->wheel.ev_wakeup[ev_wakeup_index]++;

 	ev_wakeup_index = input_stat_find_ev_wakeup_index(
		key_press + touch_press + wakeup_direction);
	input_stat->ev_wakeup[ev_wakeup_index]++;

	pr_info("[is][%lu][%lu][%lu][%lu][%lu][%lu][%lu][%lu][%u][%d]\n",
		key_press, key_release,touch_press, touch_release,
		direction[CounterClockwise], direction[Detent_Return],
		direction[Detent_Leave], direction[Clockwise],
		ev_wakeup_index, input_stat->ev_wakeup[ev_wakeup_index]);

#ifdef DEBUG
{
	int i;

	for (i = 0; i < EV_WU_MAX; i++)
		pr_debug("[is_ew][%u][%u][%u][%u]\n",
			input_stat->key.ev_wakeup[i],
			input_stat->touch.ev_wakeup[i],
			input_stat->wheel.ev_wakeup[i],
			input_stat->ev_wakeup[i]);
}
#endif
}

static int input_stat_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		input_stat_pm_suspend_prepare_cb();
		break;
	case PM_POST_SUSPEND:
		input_stat->pm_stat.key_press = input_stat->key.press_count;
		input_stat->pm_stat.key_release = input_stat->key.release_count;
		input_stat->pm_stat.touch_press = input_stat->touch.press_count;
		input_stat->pm_stat.touch_release = input_stat->touch.release_count;
		input_stat->pm_stat.direction[CounterClockwise] =
			input_stat->wheel.direction[CounterClockwise];
		input_stat->pm_stat.direction[Detent_Return] =
			input_stat->wheel.direction[Detent_Return];
		input_stat->pm_stat.direction[Detent_Leave] =
			input_stat->wheel.direction[Detent_Leave];
		input_stat->pm_stat.direction[Clockwise] =
			input_stat->wheel.direction[Clockwise];

		pr_debug("%s:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n", __func__,
			input_stat->pm_stat.key_press,
			input_stat->pm_stat.key_release,
			input_stat->pm_stat.touch_press,
			input_stat->pm_stat.touch_release,
			input_stat->pm_stat.direction[CounterClockwise],
			input_stat->pm_stat.direction[Detent_Return],
			input_stat->pm_stat.direction[Detent_Leave],
			input_stat->pm_stat.direction[Clockwise]);
		break;
	  default:
		break;
	}
	return 0;
}

static struct notifier_block pm_notifier_block = {
	.notifier_call = input_stat_pm_notifier,
};

static int __init input_stat_init(void)
{
	unsigned int ret = 0;

	pr_debug("%s\n", __func__);

	input_stat = kzalloc(sizeof(*input_stat), GFP_KERNEL);
	if (input_stat == NULL)
		return -ENOMEM;

	input_stat->key.ev_type = EV_KEY;
	input_stat->touch.ev_type = EV_KEY;
	input_stat->wheel.ev_type = EV_REL;

	input_stat_handler.private = input_stat;
	ret = input_register_handler(&input_stat_handler);
	if (ret)
		goto err;

	ret = register_pm_notifier(&pm_notifier_block);
	if (ret < 0)
		goto err_pm;

	input_stat->debug = debugfs_create_file("input_stat",
		0660, NULL, NULL, &input_stat_fops);
	if (IS_ERR(input_stat->debug)) {
		ret = PTR_ERR(input_stat->debug);
		goto err_debugfs;
	}

	return ret;
err_debugfs:
	unregister_pm_notifier(&pm_notifier_block);
err_pm:
	input_unregister_handler(&input_stat_handler);
err:
	kfree(input_stat);
	return ret;
}

static void __exit input_stat_exit(void)
{
	debugfs_remove(input_stat->debug);
	unregister_pm_notifier(&pm_notifier_block);
	input_unregister_handler(&input_stat_handler);
	kfree(input_stat);
}

module_init(input_stat_init);
module_exit(input_stat_exit);

MODULE_AUTHOR("Junho Jang <vincent.jang@samsung.com>");
MODULE_DESCRIPTION("Input Statistics for Tizen");
MODULE_LICENSE("GPL");
