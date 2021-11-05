/* drivers/power/sleep_stat.c
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2018>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>

#include <linux/sleep_stat.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

#define SLEEP_STAT_PREFIX	"sleep_stat: "

struct sleep_stat_cb {
 	int soc_type;
	int (*soc_read)(struct sleep_stat *);
	struct sleep_stat snapshot;
};

struct sleep_stat_cb stat;

int sleep_stat_get_stat_delta(int type, struct sleep_stat *sleep_stat)
{
	struct sleep_stat *snapshot = &stat.snapshot;
	struct sleep_stat soc_now;
	int ret = 0;

	sleep_stat->fail = suspend_stats.fail -
								snapshot->fail;
	sleep_stat->failed_freeze = suspend_stats.failed_freeze -
								snapshot->failed_freeze;
	sleep_stat->failed_prepare = suspend_stats.failed_prepare -
								snapshot->failed_prepare;
	sleep_stat->failed_suspend = suspend_stats.failed_suspend -
								snapshot->failed_suspend;
	sleep_stat->failed_suspend_late = suspend_stats.failed_suspend_late -
								snapshot->failed_suspend_late;
	sleep_stat->failed_suspend_noirq = suspend_stats.failed_suspend_noirq -
								snapshot->failed_suspend_noirq;
	sleep_stat->suspend_success = suspend_stats.success -
								snapshot->suspend_success;

#ifdef CONFIG_ENERGY_MONITOR
	if (type != ENERGY_MON_TYPE_DUMP) {
		snapshot->fail = suspend_stats.fail;
		snapshot->failed_freeze = suspend_stats.failed_freeze;
		snapshot->failed_prepare = suspend_stats.failed_prepare;
		snapshot->failed_suspend = suspend_stats.failed_suspend;
		snapshot->failed_suspend_late = suspend_stats.failed_suspend_late;
		snapshot->failed_suspend_noirq = suspend_stats.failed_suspend_noirq;
		snapshot->suspend_success = suspend_stats.success;
	}
#else
	snapshot->fail = suspend_stats.fail;
	snapshot->failed_freeze = suspend_stats.failed_freeze;
	snapshot->failed_prepare = suspend_stats.failed_prepare;
	snapshot->failed_suspend = suspend_stats.failed_suspend;
	snapshot->failed_suspend_late = suspend_stats.failed_suspend_late;
	snapshot->failed_suspend_noirq = suspend_stats.failed_suspend_noirq;
	snapshot->suspend_success = suspend_stats.success;
#endif

	if (!stat.soc_type)
		return ret;

	sleep_stat->soc_type = stat.soc_type;
	if (stat.soc_read == NULL) {
		pr_err("%s: soc_read function does not exist \n", __func__);
		return -ENODEV;
	}

	ret = stat.soc_read(&soc_now);
	if(ret)
		pr_err("%s: soc_read fail\n", __func__);
	else {
		switch (stat.soc_type) {
		case SLEEP_STAT_SOC_EXYNOS:
			sleep_stat->soc.exynos.acpm_sleep_early_wakeup =
				soc_now.soc.exynos.acpm_sleep_early_wakeup -
				snapshot->soc.exynos.acpm_sleep_early_wakeup;
			sleep_stat->soc.exynos.acpm_sleep_soc_down =
				soc_now.soc.exynos.acpm_sleep_soc_down -
				snapshot->soc.exynos.acpm_sleep_soc_down;
			sleep_stat->soc.exynos.acpm_sleep_mif_down =
				soc_now.soc.exynos.acpm_sleep_mif_down -
				snapshot->soc.exynos.acpm_sleep_mif_down;
			sleep_stat->soc.exynos.acpm_sicd_early_wakeup =
				soc_now.soc.exynos.acpm_sicd_early_wakeup -
				snapshot->soc.exynos.acpm_sicd_early_wakeup;
			sleep_stat->soc.exynos.acpm_sicd_soc_down =
				soc_now.soc.exynos.acpm_sicd_soc_down -
				snapshot->soc.exynos.acpm_sicd_soc_down;
			sleep_stat->soc.exynos.acpm_sicd_mif_down =
				soc_now.soc.exynos.acpm_sicd_mif_down -
				snapshot->soc.exynos.acpm_sicd_mif_down;

#ifdef CONFIG_ENERGY_MONITOR
			if (type != ENERGY_MON_TYPE_DUMP) {
				snapshot->soc.exynos.acpm_sleep_early_wakeup =
					soc_now.soc.exynos.acpm_sleep_early_wakeup;
				snapshot->soc.exynos.acpm_sleep_soc_down =
					soc_now.soc.exynos.acpm_sleep_soc_down;
				snapshot->soc.exynos.acpm_sleep_mif_down =
					soc_now.soc.exynos.acpm_sleep_mif_down;
				snapshot->soc.exynos.acpm_sicd_early_wakeup =
					soc_now.soc.exynos.acpm_sicd_early_wakeup;
				snapshot->soc.exynos.acpm_sicd_soc_down =
					soc_now.soc.exynos.acpm_sicd_soc_down;
				snapshot->soc.exynos.acpm_sicd_mif_down =
					soc_now.soc.exynos.acpm_sicd_mif_down;
			}
#else
			snapshot->soc.exynos.acpm_sleep_early_wakeup =
			soc_now.soc.exynos.acpm_sleep_early_wakeup;
			snapshot->soc.exynos.acpm_sleep_soc_down =
				soc_now.soc.exynos.acpm_sleep_soc_down;
			snapshot->soc.exynos.acpm_sleep_mif_down =
				soc_now.soc.exynos.acpm_sleep_mif_down;
			snapshot->soc.exynos.acpm_sicd_early_wakeup =
				soc_now.soc.exynos.acpm_sicd_early_wakeup;
			snapshot->soc.exynos.acpm_sicd_soc_down =
				soc_now.soc.exynos.acpm_sicd_soc_down;
			snapshot->soc.exynos.acpm_sicd_mif_down =
				soc_now.soc.exynos.acpm_sicd_mif_down;
#endif
			break;
		default:
			break;
		}
	}

	return ret;
}

int sleep_stat_register_soc(int soc_type, void *soc_read)
{
	if (!soc_read)
		return -EINVAL;

	stat.soc_type = soc_type;
	stat.soc_read = (int (*)(struct sleep_stat *))soc_read;

	pr_info("%s: soc_type=0x%x\n", __func__, soc_type);

	return 0;
}

static int sleep_stat_show(struct seq_file *m, void *v)
{
	struct sleep_stat soc_now;

	seq_printf(m, "%s: %d\n%s: %d\n%s: %d\n"
			"%s: %d\n%s: %d\n%s: %d\n%s: %d\n",
			"fail",suspend_stats.fail,
			"failed_freeze", suspend_stats.failed_freeze,
			"failed_prepare", suspend_stats.failed_prepare,
			"failed_suspend", suspend_stats.failed_suspend,
			"failed_suspend_late", suspend_stats.failed_suspend_late,
			"failed_suspend_noirq", suspend_stats.failed_suspend_noirq,
			"suspend_success", suspend_stats.success);

	if (stat.soc_type && stat.soc_read) {
		stat.soc_read(&soc_now);
	} else
		pr_info("%s: 0x%x\n", __func__, stat.soc_type);

	switch (stat.soc_type) {
	case SLEEP_STAT_SOC_EXYNOS:
		seq_printf(m, "acpm_sleep_early_wakeup: %d\n",
					soc_now.soc.exynos.acpm_sleep_early_wakeup);
		seq_printf(m, "acpm_sleep_soc_down: %d\n",
					soc_now.soc.exynos.acpm_sleep_soc_down);
		seq_printf(m, "acpm_sleep_mif_down: %d\n",
					soc_now.soc.exynos.acpm_sleep_mif_down);
		seq_printf(m, "acpm_sicd_early_wakeup: %d\n",
					soc_now.soc.exynos.acpm_sicd_early_wakeup);
		seq_printf(m, "acpm_sicd_soc_down: %d\n",
					soc_now.soc.exynos.acpm_sicd_soc_down);
		seq_printf(m, "acpm_sicd_mif_down: %d\n",
					soc_now.soc.exynos.acpm_sicd_mif_down);
		break;
	default:
		break;
	}

	return 0;
}

static int sleep_stat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sleep_stat_show, NULL);
}

static const struct file_operations sleep_stat_fops = {
	.open       = sleep_stat_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int __init sleep_stat_init(void)
{

	stat.soc_type = 0;
	stat.soc_read = NULL;

	if (!debugfs_create_file("sleep_stat", 0400, NULL, NULL, &sleep_stat_fops))
		goto err_debugfs;

	return 0;

err_debugfs:

	return -1;
}

module_init(sleep_stat_init);
