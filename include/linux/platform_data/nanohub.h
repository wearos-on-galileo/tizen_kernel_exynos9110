#ifndef __LINUX_PLATFORM_DATA_NANOHUB_H
#define __LINUX_PLATFORM_DATA_NANOHUB_H

#include <linux/types.h>

struct nanohub_flash_bank {
	int bank;
	u32 address;
	size_t length;
};

#define MAX_FILE_LEN (32)

struct nanohub_platform_data {
#ifdef CONFIG_CONTEXTHUB_IPC
	void *mailbox_client;
	int irq;
	atomic_t wakeup_chub;
	atomic_t irq1_apInt;
	atomic_t chub_status;
	bool os_load;
	char os_image[MAX_FILE_LEN];
	int powermode_on;
#else
	u32 wakeup_gpio;
	u32 nreset_gpio;
	u32 boot0_gpio;
	u32 irq1_gpio;
	u32 irq2_gpio;
	u32 spi_cs_gpio;
	u32 bl_addr;
	u32 num_flash_banks;
	struct nanohub_flash_bank *flash_banks;
	u32 num_shared_flash_banks;
	struct nanohub_flash_bank *shared_flash_banks;
#endif
};

#endif /* __LINUX_PLATFORM_DATA_NANOHUB_H */
