/*
 * secgpio_dvs.h -- Samsung GPIO debugging and verification system
 */

#ifndef __SECGPIO_DVS_H
#define __SECGPIO_DVS_H

#include <linux/types.h>

enum gdvs_phone_status {
	PHONE_INIT = 0,
	PHONE_SLEEP,
	GDVS_PHONE_STATUS_MAX
};

struct gpiomap_result_t {
	unsigned short *init;
	unsigned short *sleep;
};

struct gpio_dvs_t {
	struct gpiomap_result_t *result;
	unsigned int count;
	bool check_sleep;
	bool check_init;
	void (*check_gpio_status)(unsigned char phonestate);
	int (*get_nr_gpio)(void);
};

void gpio_dvs_check_initgpio(void);
void gpio_dvs_check_sleepgpio(void);

/* list of all exported SoC specific data */
extern struct gpio_dvs_t exynos9110_secgpio_dvs;
extern int exynos9110_secgpio_get_nr_gpio(void);

#endif /* __SECGPIO_DVS_H */
