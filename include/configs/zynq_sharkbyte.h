/*
 * Configuration settings for the Sharkbyte board
 * Based on zynq_zc70x.h - See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ZYNQ_SHARKBYTE_H
#define __CONFIG_ZYNQ_SHARKBYTE_H

#define CONFIG_ZYNQ_I2C0
#define CONFIG_ZYNQ_EEPROM
#define CONFIG_DFU_SF

#include <configs/zynq-common.h>

/* Sharkbyte has no reset button; MIO14 is used by eMMC (SD1 DAT2).
 * Disable misc_init_r to prevent false "button pressed" env reset. */
#undef CONFIG_MISC_INIT_R

#endif /* __CONFIG_ZYNQ_SHARKBYTE_H */
