/* drivers/gpu/arm/.../platform/gpu_notifier.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_notifier.c
 */

#include <mali_kbase.h>

#include <linux/suspend.h>
#include <linux/pm_runtime.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_control.h"

#ifdef CONFIG_EXYNOS_THERMAL
#include <soc/samsung/tmu.h>
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_EXYNOS_BUSMONITOR
#include <linux/exynos-busmon.h>
#endif

extern struct kbase_device *pkbdev;

#if defined (CONFIG_EXYNOS_THERMAL) && defined(CONFIG_GPU_THERMAL)
static void gpu_tmu_normal_work(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, TMU_LOCK, 0);
#endif /* CONFIG_MALI_DVFS */
}

static int gpu_tmu_notifier(struct notifier_block *notifier,
				unsigned long event, void *v)
{
	int frequency;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	char *cooling_device_name = "GPU";
#endif

	if (!platform)
		return -ENODEV;

	if (!platform->tmu_status)
		return NOTIFY_OK;

	platform->voltage_margin = 0;
	frequency = *(int*)v;

	if (event == GPU_COLD) {
		platform->voltage_margin = platform->gpu_default_vol_margin;
	} else if (event == GPU_NORMAL) {
		gpu_tmu_normal_work(pkbdev);
	} else if (event == GPU_THROTTLING || event == GPU_TRIPPING) {
		gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, TMU_LOCK, frequency);
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
		exynos_ss_thermal(NULL, 0, cooling_device_name, frequency);
#endif
	}

	GPU_LOG(DVFS_DEBUG, LSI_TMU_VALUE, 0u, event, "tmu event %lu, frequency %d\n", event, frequency);

	gpu_set_target_clk_vol(platform->cur_clock, false);

	return NOTIFY_OK;
}

static struct notifier_block gpu_tmu_nb = {
	.notifier_call = gpu_tmu_notifier,
};
#endif /* CONFIG_EXYNOS_THERMAL */

static int gpu_power_on(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power on\n");

	if (!kbdev->is_power_on) {
		ret = pm_runtime_get_sync(kbdev->dev);
		kbdev->is_power_on = true;
	}

	if (ret > 0) {
		if (platform->early_clk_gating_status) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "already power on\n");
			gpu_control_enable_clock(kbdev);
		}
		return 0;
	} else if (ret == 0) {
		return 1;
	} else {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "runtime pm returned %d\n", ret);
		return 0;
	}
}

static void gpu_power_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power off\n");
	gpu_control_enable_customization(kbdev);

	if (kbdev->is_power_on) {
		pm_runtime_mark_last_busy(kbdev->dev);
		pm_runtime_put_sync_autosuspend(kbdev->dev);
		kbdev->is_power_on = false;
	}

	if (platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

static void gpu_power_suspend(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int ret = 0;

	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power suspend\n");
	gpu_control_enable_customization(kbdev);

	ret = pm_runtime_suspend(kbdev->dev);

	/* we must turn on GPU power when device status is running on shutdown callbacks */
	if (ret < 0) {
		gpu_control_disable_customization(kbdev);
	}

	if (platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

static void gpu_power_resume(struct kbase_device *kbdev)
{
	// Do nothing
	return;
}

#ifdef CONFIG_MALI_RT_PM
extern int kbase_device_resume(struct kbase_device *kbdev);
static int gpu_pm_notifier(struct notifier_block *nb, unsigned long event, void *cmd)
{
	int err = NOTIFY_OK;
	struct kbase_device *kbdev = pkbdev;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		GPU_LOG(DVFS_DEBUG, LSI_SUSPEND, 0u, 0u, "%s: suspend event\n", __func__);
		break;
	case PM_POST_SUSPEND:
		if (kbdev)
			kbase_device_resume(kbdev);
		GPU_LOG(DVFS_DEBUG, LSI_RESUME, 0u, 0u, "%s: resume event\n", __func__);
		break;
	default:
		break;
	}
	return err;
}

static struct notifier_block gpu_pm_nb = {
	.notifier_call = gpu_pm_notifier
};

static int gpu_device_runtime_init(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform) {
		dev_warn(kbdev->dev, "kbase_device_runtime_init failed %p\n", platform);
		ret = -ENOSYS;
		return ret;
	}

	dev_dbg(kbdev->dev, "kbase_device_runtime_init\n");

	// Remove runtime_pm_delay_time.
	pm_runtime_set_autosuspend_delay(kbdev->dev, 0);
	pm_runtime_use_autosuspend(kbdev->dev);

	pm_runtime_set_active(kbdev->dev);
	pm_runtime_enable(kbdev->dev);

	if (!pm_runtime_enabled(kbdev->dev)) {
		dev_warn(kbdev->dev, "pm_runtime not enabled");
		ret = -ENOSYS;
	}
	kbdev->is_power_on = false;

	return ret;
}

static void gpu_device_runtime_disable(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->dev);
}

static int pm_callback_dvfs_on(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	gpu_dvfs_timer_control(true);

	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;
#endif

	return 0;
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, LSI_GPU_ON, 0u, 0u, "runtime on callback\n");

	gpu_control_enable_clock(kbdev);
	platform->power_status = true;
#ifdef CONFIG_MALI_DVFS
	if (platform->dvfs_status && platform->wakeup_lock)
		gpu_set_target_clk_vol(platform->gpu_dvfs_start_clock, false);
	else
		gpu_set_target_clk_vol(platform->cur_clock, false);
#endif /* CONFIG_MALI_DVFS */

	return 0;
}
extern void preload_balance_setup(struct kbase_device *kbdev);
static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, LSI_GPU_OFF, 0u, 0u, "runtime off callback\n");

	platform->power_status = false;

#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_timer_control(false);
	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;
#endif /* CONFIG_MALI_DVFS */
	if (!platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);

#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
	preload_balance_setup(kbdev);
#endif
}
#endif /* CONFIG_MALI_RT_PM */

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = gpu_power_on,
	.power_off_callback = gpu_power_off,
	.power_suspend_callback = gpu_power_suspend,
	.power_resume_callback = gpu_power_resume,
#ifdef CONFIG_MALI_RT_PM
	.power_runtime_init_callback = gpu_device_runtime_init,
	.power_runtime_term_callback = gpu_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
	.power_dvfs_on_callback = pm_callback_dvfs_on,
#else /* CONFIG_MALI_RT_PM */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
	.power_dvfs_on_callback = NULL,
#endif /* CONFIG_MALI_RT_PM */
};

int gpu_notifier_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	platform->voltage_margin = platform->gpu_default_vol_margin;
#if defined(CONFIG_EXYNOS_THERMAL) && defined(CONFIG_GPU_THERMAL)
	exynos_gpu_add_notifier(&gpu_tmu_nb);
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_MALI_RT_PM
	if (register_pm_notifier(&gpu_pm_nb))
		return -1;
#endif /* CONFIG_MALI_RT_PM */

	platform->power_status = true;

	return 0;
}

void gpu_notifier_term(void)
{
#ifdef CONFIG_MALI_RT_PM
	unregister_pm_notifier(&gpu_pm_nb);
#endif /* CONFIG_MALI_RT_PM */
	return;
}
