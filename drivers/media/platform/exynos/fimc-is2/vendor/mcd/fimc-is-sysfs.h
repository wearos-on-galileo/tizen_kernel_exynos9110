/* linux/arch/arm/mach-exynos/include/mach/fimc-is-sysfs.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _FIMC_IS_SYSFS_H_
#define _FIMC_IS_SYSFS_H_

#include "fimc-is-core.h"

#ifdef CAMERA_SYSFS_V2
enum fimc_is_cam_info_isp {
	CAM_INFO_ISP_TYPE_INTERNAL = 0,
	CAM_INFO_ISP_TYPE_EXTERNAL,
	CAM_INFO_ISP_TYPE_SOC,
};

enum fimc_is_cam_info_cal_mem {
	CAM_INFO_CAL_MEM_TYPE_NONE = 0,
	CAM_INFO_CAL_MEM_TYPE_FROM,
	CAM_INFO_CAL_MEM_TYPE_EEPROM,
	CAM_INFO_CAL_MEM_TYPE_OTP,
};

enum fimc_is_cam_info_read_ver {
	CAM_INFO_READ_VER_SYSFS = 0,
	CAM_INFO_READ_VER_CAMON,
};

enum fimc_is_cam_info_core_voltage {
	CAM_INFO_CORE_VOLT_NONE = 0,
	CAM_INFO_CORE_VOLT_USE,
};

enum fimc_is_cam_info_upgrade {
	CAM_INFO_FW_UPGRADE_NONE = 0,
	CAM_INFO_FW_UPGRADE_SYSFS,
	CAM_INFO_FW_UPGRADE_CAMON,
};

enum fimc_is_cam_info_fw_write {
	CAM_INFO_FW_WRITE_NONE = 0,
	CAM_INFO_FW_WRITE_OS,
	CAM_INFO_FW_WRITE_SD,
	CAM_INFO_FW_WRITE_ALL,
};

enum fimc_is_cam_info_fw_dump {
	CAM_INFO_FW_DUMP_NONE = 0,
	CAM_INFO_FW_DUMP_USE,
};

enum fimc_is_cam_info_companion {
	CAM_INFO_COMPANION_NONE = 0,
	CAM_INFO_COMPANION_USE,
};

enum fimc_is_cam_info_ois {
	CAM_INFO_OIS_NONE = 0,
	CAM_INFO_OIS_USE,
};

struct fimc_is_cam_info {
	unsigned int isp;
	unsigned int cal_memory;
	unsigned int read_version;
	unsigned int core_voltage;
	unsigned int upgrade;
	unsigned int fw_write;
	unsigned int fw_dump;
	unsigned int companion;
	unsigned int ois;
	unsigned int valid;
};

int fimc_is_get_cam_info(struct fimc_is_cam_info **caminfo);

#endif
#endif /* _FIMC_IS_SYSFS_H_ */
